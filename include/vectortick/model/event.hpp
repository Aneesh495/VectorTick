#pragma once

#include "../common/types.hpp"
#include "../common/status.hpp"
#include "../common/hash.hpp"
#include <functional>
#include <cstring>

namespace vectortick {

// CanonicalEvent is defined in common/types.hpp
// This file provides additional utilities for event handling

// Event validation helpers
namespace event {

// Validate a canonical event (returns Status)
[[nodiscard]] inline Status validate(const CanonicalEvent& evt) noexcept {
    // Check event type
    if (evt.event_type > EventType::Heartbeat) {
        return Status(StatusCode::InvalidPayload, "Invalid event type");
    }
    
    // Check side (except for heartbeat)
    if (evt.event_type != EventType::Heartbeat && 
        evt.side > Side::Ask) {
        return Status(StatusCode::InvalidPayload, "Invalid side");
    }
    
    // Check timestamp bounds
    if (evt.exchange_ts_ns > Limits::MaxTimestamp) {
        return Status(StatusCode::InvalidTimestamp, "Exchange timestamp out of range");
    }
    if (evt.receive_ts_ns > Limits::MaxTimestamp) {
        return Status(StatusCode::InvalidTimestamp, "Receive timestamp out of range");
    }
    
    // Check instrument bounds
    if (evt.instrument_id >= Limits::MaxInstruments) {
        return Status(StatusCode::InvalidPayload, "Instrument ID out of range");
    }
    
    // Check price bounds
    if (evt.price_ticks < -Limits::MaxPrice || evt.price_ticks > Limits::MaxPrice) {
        return Status(StatusCode::InvalidPayload, "Price out of range");
    }
    
    // Check quantity bounds
    if (evt.quantity > Limits::MaxQuantity) {
        return Status(StatusCode::InvalidPayload, "Quantity out of range");
    }
    
    // Check venue and source
    if (evt.venue_id >= Limits::MaxVenues) {
        return Status(StatusCode::InvalidPayload, "Venue ID out of range");
    }
    if (evt.source_id >= Limits::MaxSources) {
        return Status(StatusCode::InvalidPayload, "Source ID out of range");
    }
    
    return Status::OK();
}

// Member validate function that calls the above
inline Status CanonicalEvent_validate(const CanonicalEvent& evt) noexcept {
    return validate(evt);
}

// Compute hash for an event using SplitMix64
[[nodiscard]] inline Hash256 compute_hash(const CanonicalEvent& evt) noexcept {
    Hash256 result;
    
    // Simple hash using built-in hash
    u64 h1 = std::hash<u64>{}(evt.exchange_ts_ns);
    h1 ^= std::hash<u64>{}(evt.receive_ts_ns) + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
    h1 ^= std::hash<u64>{}(evt.sequence) + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
    h1 ^= std::hash<u32>{}(evt.instrument_id) + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
    
    u64 h2 = std::hash<i64>{}(evt.price_ticks);
    h2 ^= std::hash<u32>{}(evt.quantity) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
    h2 ^= std::hash<u64>{}(evt.trade_or_order_id) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
    
    std::memcpy(result.data, &h1, 8);
    std::memcpy(result.data + 8, &h2, 8);
    std::memcpy(result.data + 16, &h1, 8);
    std::memcpy(result.data + 24, &h2, 8);
    
    return result;
}

// Compare two events for equality
[[nodiscard]] inline bool equal(const CanonicalEvent& a, const CanonicalEvent& b) noexcept {
    return std::memcmp(&a, &b, sizeof(CanonicalEvent)) == 0;
}

// Create a default/empty event
[[nodiscard]] inline CanonicalEvent make_empty_event() noexcept {
    CanonicalEvent evt{};
    evt.exchange_ts_ns = 0;
    evt.receive_ts_ns = 0;
    evt.sequence = 0;
    evt.instrument_id = 0;
    evt.event_type = EventType::Invalid;
    evt.side = Side::Invalid;
    evt.flags = 0;
    evt.price_ticks = 0;
    evt.quantity = 0;
    evt.venue_id = 0;
    evt.source_id = 0;
    evt.trade_or_order_id = 0;
    return evt;
}

// Create a heartbeat event
[[nodiscard]] inline CanonicalEvent make_heartbeat(u64 ts_ns, u32 source) noexcept {
    CanonicalEvent evt{};
    evt.exchange_ts_ns = ts_ns;
    evt.receive_ts_ns = ts_ns;
    evt.sequence = 0;
    evt.instrument_id = 0;
    evt.event_type = EventType::Heartbeat;
    evt.side = Side::Invalid;
    evt.flags = 0;
    evt.price_ticks = 0;
    evt.quantity = 0;
    evt.venue_id = 0;
    evt.source_id = static_cast<u16>(source);
    evt.trade_or_order_id = 0;
    return evt;
}

// Create a trade event
[[nodiscard]] inline CanonicalEvent make_trade(
    u64 exchange_ts, u64 receive_ts, u64 seq,
    u32 inst, Side side, i64 price, u32 qty,
    u64 trade_id, u16 venue = 0, u16 source = 0) noexcept {
    CanonicalEvent evt{};
    evt.exchange_ts_ns = exchange_ts;
    evt.receive_ts_ns = receive_ts;
    evt.sequence = seq;
    evt.instrument_id = inst;
    evt.event_type = EventType::Trade;
    evt.side = side;
    evt.flags = 0;
    evt.price_ticks = price;
    evt.quantity = qty;
    evt.venue_id = venue;
    evt.source_id = source;
    evt.trade_or_order_id = trade_id;
    return evt;
}

// Create a quote event
[[nodiscard]] inline CanonicalEvent make_quote(
    u64 exchange_ts, u64 receive_ts, u64 seq,
    u32 inst, Side side, i64 price, u32 qty,
    u64 order_id, u16 venue = 0, u16 source = 0) noexcept {
    CanonicalEvent evt{};
    evt.exchange_ts_ns = exchange_ts;
    evt.receive_ts_ns = receive_ts;
    evt.sequence = seq;
    evt.instrument_id = inst;
    evt.event_type = EventType::Quote;
    evt.side = side;
    evt.flags = 0;
    evt.price_ticks = price;
    evt.quantity = qty;
    evt.venue_id = venue;
    evt.source_id = source;
    evt.trade_or_order_id = order_id;
    return evt;
}

} // namespace event

} // namespace vectortick
