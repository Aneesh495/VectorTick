#pragma once

#include "../common/types.hpp"
#include "../common/status.hpp"
#include "../common/hash.hpp"
#include <functional>

namespace vectortick {

// Canonical event (56 bytes logical schema)
// This is the normalized representation used throughout the system
// Prices are integer ticks, quantities are integer lots, timestamps are nanoseconds
struct CanonicalEvent {
    u64 exchange_ts_ns;    // Exchange timestamp (8 bytes)
    u64 receive_ts_ns;     // Receive timestamp (8 bytes)
    u64 sequence;          // Source sequence number (8 bytes)
    u32 instrument_id;     // Instrument identifier (4 bytes)
    EventType event_type;  // Event type enum (1 byte)
    Side side;             // Bid/Ask side (1 byte)
    u16 flags;             // Event flags (2 bytes)
    i64 price_ticks;       // Price in ticks (8 bytes, signed)
    u32 quantity;          // Quantity in lots (4 bytes)
    u16 venue_id;          // Venue identifier (2 bytes)
    u16 source_id;         // Source identifier (2 bytes)
    u64 trade_or_order_id; // Trade or order ID (8 bytes)
    
    // Total logical size: 56 bytes
    static constexpr usize LogicalSize = 56;
    
    // Default constructor
    CanonicalEvent() = default;
    
    // Full constructor
    CanonicalEvent(u64 exchange_ts, u64 receive_ts, u64 seq, u32 inst,
                   EventType type, Side s, u16 f, i64 price, u32 qty,
                   u16 venue, u16 source, u64 id)
        : exchange_ts_ns(exchange_ts)
        , receive_ts_ns(receive_ts)
        , sequence(seq)
        , instrument_id(inst)
        , event_type(type)
        , side(s)
        , flags(f)
        , price_ticks(price)
        , quantity(qty)
        , venue_id(venue)
        , source_id(source)
        , trade_or_order_id(id) {}
    
    // Validate event
    [[nodiscard]] Status validate() const noexcept {
        // Check event type
        if (event_type > EventType::Heartbeat) {
            return Status(StatusCode::InvalidPayload, "Invalid event type");
        }
        
        // Check side (except for heartbeat)
        if (event_type != EventType::Heartbeat && 
            side > Side::Ask) {
            return Status(StatusCode::InvalidPayload, "Invalid side");
        }
        
        // Check timestamp bounds
        if (exchange_ts_ns > Limits::MaxTimestamp) {
            return Status(StatusCode::InvalidTimestamp, "Exchange timestamp out of range");
        }
        if (receive_ts_ns > Limits::MaxTimestamp) {
            return Status(StatusCode::InvalidTimestamp, "Receive timestamp out of range");
        }
        
        // Check instrument bounds
        if (instrument_id >= Limits::MaxInstruments) {
            return Status(StatusCode::InvalidPayload, "Instrument ID out of range");
        }
        
        // Check price bounds
        if (price_ticks < -Limits::MaxPrice || price_ticks > Limits::MaxPrice) {
            return Status(StatusCode::InvalidPayload, "Price out of range");
        }
        
        // Check quantity bounds
        if (quantity > Limits::MaxQuantity) {
            return Status(StatusCode::InvalidPayload, "Quantity out of range");
        }
        
        // Check venue and source
        if (venue_id >= Limits::MaxVenues) {
            return Status(StatusCode::InvalidPayload, "Venue ID out of range");
        }
        if (source_id >= Limits::MaxSources) {
            return Status(StatusCode::InvalidPayload, "Source ID out of range");
        }
        
        return Status::OK();
    }
    
    // Compute hash for this event
    [[nodiscard]] u64 hash() const noexcept {
        FnvHash hasher;
        hasher.update(reinterpret_cast<const byte*>(this), sizeof(*this));
        return hasher.get();
    }
    
    // Comparison for sorting
    [[nodiscard]] bool operator<(const CanonicalEvent& other) const noexcept {
        // Primary: exchange timestamp
        if (exchange_ts_ns != other.exchange_ts_ns) {
            return exchange_ts_ns < other.exchange_ts_ns;
        }
        // Secondary: sequence number
        return sequence < other.sequence;
    }
    
    [[nodiscard]] bool operator==(const CanonicalEvent& other) const noexcept {
        return exchange_ts_ns == other.exchange_ts_ns &&
               receive_ts_ns == other.receive_ts_ns &&
               sequence == other.sequence &&
               instrument_id == other.instrument_id &&
               event_type == other.event_type &&
               side == other.side &&
               flags == other.flags &&
               price_ticks == other.price_ticks &&
               quantity == other.quantity &&
               venue_id == other.venue_id &&
               source_id == other.source_id &&
               trade_or_order_id == other.trade_or_order_id;
    }
};

// Event batch for vectorized processing
class EventBatch {
public:
    static constexpr usize DefaultCapacity = SegmentConfig::BatchSize;
    
    EventBatch() : capacity_(DefaultCapacity), size_(0) {
        allocate_buffers();
    }
    
    explicit EventBatch(usize capacity) : capacity_(capacity), size_(0) {
        allocate_buffers();
    }
    
    // Non-copyable
    EventBatch(const EventBatch&) = delete;
    EventBatch& operator=(const EventBatch&) = delete;
    
    // Movable
    EventBatch(EventBatch&& other) noexcept
        : exchange_ts_ns_(std::move(other.exchange_ts_ns_))
        , receive_ts_ns_(std::move(other.receive_ts_ns_))
        , sequences_(std::move(other.sequences_))
        , instrument_ids_(std::move(other.instrument_ids_))
        , event_types_(std::move(other.event_types_))
        , sides_(std::move(other.sides_))
        , flags_(std::move(other.flags_))
        , price_ticks_(std::move(other.price_ticks_))
        , quantities_(std::move(other.quantities_))
        , venue_ids_(std::move(other.venue_ids_))
        , source_ids_(std::move(other.source_ids_))
        , trade_or_order_ids_(std::move(other.trade_or_order_ids_))
        , capacity_(other.capacity_)
        , size_(other.size_) {
        other.capacity_ = 0;
        other.size_ = 0;
    }
    
    EventBatch& operator=(EventBatch&& other) noexcept {
        if (this != &other) {
            exchange_ts_ns_ = std::move(other.exchange_ts_ns_);
            receive_ts_ns_ = std::move(other.receive_ts_ns_);
            sequences_ = std::move(other.sequences_);
            instrument_ids_ = std::move(other.instrument_ids_);
            event_types_ = std::move(other.event_types_);
            sides_ = std::move(other.sides_);
            flags_ = std::move(other.flags_);
            price_ticks_ = std::move(other.price_ticks_);
            quantities_ = std::move(other.quantities_);
            venue_ids_ = std::move(other.venue_ids_);
            source_ids_ = std::move(other.source_ids_);
            trade_or_order_ids_ = std::move(other.trade_or_order_ids_);
            capacity_ = other.capacity_;
            size_ = other.size_;
            other.capacity_ = 0;
            other.size_ = 0;
        }
        return *this;
    }
    
    // Add event to batch
    [[nodiscard]] bool push(const CanonicalEvent& event) noexcept {
        if (size_ >= capacity_) return false;
        
        exchange_ts_ns_[size_] = event.exchange_ts_ns;
        receive_ts_ns_[size_] = event.receive_ts_ns;
        sequences_[size_] = event.sequence;
        instrument_ids_[size_] = event.instrument_id;
        event_types_[size_] = event.event_type;
        sides_[size_] = event.side;
        flags_[size_] = event.flags;
        price_ticks_[size_] = event.price_ticks;
        quantities_[size_] = event.quantity;
        venue_ids_[size_] = event.venue_id;
        source_ids_[size_] = event.source_id;
        trade_or_order_ids_[size_] = event.trade_or_order_id;
        
        ++size_;
        return true;
    }
    
    // Get event at index
    [[nodiscard]] CanonicalEvent get(usize index) const noexcept {
        return CanonicalEvent(
            exchange_ts_ns_[index],
            receive_ts_ns_[index],
            sequences_[index],
            instrument_ids_[index],
            event_types_[index],
            sides_[index],
            flags_[index],
            price_ticks_[index],
            quantities_[index],
            venue_ids_[index],
            source_ids_[index],
            trade_or_order_ids_[index]
        );
    }
    
    // Clear batch
    void clear() noexcept { size_ = 0; }
    
    // Accessors
    [[nodiscard]] usize size() const noexcept { return size_; }
    [[nodiscard]] usize capacity() const noexcept { return capacity_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] bool full() const noexcept { return size_ >= capacity_; }
    
    // Column accessors
    u64* exchange_ts_ns() noexcept { return exchange_ts_ns_.data(); }
    const u64* exchange_ts_ns() const noexcept { return exchange_ts_ns_.data(); }
    
    u64* receive_ts_ns() noexcept { return receive_ts_ns_.data(); }
    const u64* receive_ts_ns() const noexcept { return receive_ts_ns_.data(); }
    
    u64* sequences() noexcept { return sequences_.data(); }
    const u64* sequences() const noexcept { return sequences_.data(); }
    
    u32* instrument_ids() noexcept { return instrument_ids_.data(); }
    const u32* instrument_ids() const noexcept { return instrument_ids_.data(); }
    
    EventType* event_types() noexcept { return event_types_.data(); }
    const EventType* event_types() const noexcept { return event_types_.data(); }
    
    Side* sides() noexcept { return sides_.data(); }
    const Side* sides() const noexcept { return sides_.data(); }
    
    u16* flags() noexcept { return flags_.data(); }
    const u16* flags() const noexcept { return flags_.data(); }
    
    i64* price_ticks() noexcept { return price_ticks_.data(); }
    const i64* price_ticks() const noexcept { return price_ticks_.data(); }
    
    u32* quantities() noexcept { return quantities_.data(); }
    const u32* quantities() const noexcept { return quantities_.data(); }
    
    u16* venue_ids() noexcept { return venue_ids_.data(); }
    const u16* venue_ids() const noexcept { return venue_ids_.data(); }
    
    u16* source_ids() noexcept { return source_ids_.data(); }
    const u16* source_ids() const noexcept { return source_ids_.data(); }
    
    u64* trade_or_order_ids() noexcept { return trade_or_order_ids_.data(); }
    const u64* trade_or_order_ids() const noexcept { return trade_or_order_ids_.data(); }

private:
    void allocate_buffers() {
        exchange_ts_ns_.resize(capacity_);
        receive_ts_ns_.resize(capacity_);
        sequences_.resize(capacity_);
        instrument_ids_.resize(capacity_);
        event_types_.resize(capacity_);
        sides_.resize(capacity_);
        flags_.resize(capacity_);
        price_ticks_.resize(capacity_);
        quantities_.resize(capacity_);
        venue_ids_.resize(capacity_);
        source_ids_.resize(capacity_);
        trade_or_order_ids_.resize(capacity_);
    }
    
    // Columnar storage
    std::vector<u64> exchange_ts_ns_;
    std::vector<u64> receive_ts_ns_;
    std::vector<u64> sequences_;
    std::vector<u32> instrument_ids_;
    std::vector<EventType> event_types_;
    std::vector<Side> sides_;
    std::vector<u16> flags_;
    std::vector<i64> price_ticks_;
    std::vector<u32> quantities_;
    std::vector<u16> venue_ids_;
    std::vector<u16> source_ids_;
    std::vector<u64> trade_or_order_ids_;
    
    usize capacity_;
    usize size_;
};

} // namespace vectortick
