#pragma once

#include "../common/types.hpp"
#include "../common/endian.hpp"
#include "../model/event.hpp"
#include "frame.hpp"

namespace vectortick {

namespace vtp1 {

// Quote message payload (32 bytes)
struct QuotePayload {
    u64 exchange_ts_ns;
    u32 instrument_id;
    u8 side;
    u8 reserved[3];
    i64 bid_price_ticks;
    u32 bid_quantity;
    i64 ask_price_ticks;
    u32 ask_quantity;
    
    static constexpr usize Size = 32;
    
    void read_from(const byte* buffer) noexcept {
        exchange_ts_ns = read_be_u64(buffer);
        instrument_id = read_be_u32(buffer + 8);
        side = buffer[12];
        reserved[0] = buffer[13];
        reserved[1] = buffer[14];
        reserved[2] = buffer[15];
        bid_price_ticks = read_be_i64(buffer + 16);
        bid_quantity = read_be_u32(buffer + 24);
        ask_price_ticks = read_be_i64(buffer + 28);
        ask_quantity = read_be_u32(buffer + 36);
    }
    
    void write_to(byte* buffer) const noexcept {
        write_be_u64(buffer, exchange_ts_ns);
        write_be_u32(buffer + 8, instrument_id);
        buffer[12] = side;
        buffer[13] = reserved[0];
        buffer[14] = reserved[1];
        buffer[15] = reserved[2];
        write_be_i64(buffer + 16, bid_price_ticks);
        write_be_u32(buffer + 24, bid_quantity);
        write_be_i64(buffer + 28, ask_price_ticks);
        write_be_u32(buffer + 36, ask_quantity);
    }
};

// Trade message payload (40 bytes)
struct TradePayload {
    u64 exchange_ts_ns;
    u32 instrument_id;
    u8 side;
    u8 reserved[3];
    i64 price_ticks;
    u32 quantity;
    u64 trade_id;
    u16 venue_id;
    u16 source_id;
    
    static constexpr usize Size = 40;
    
    void read_from(const byte* buffer) noexcept {
        exchange_ts_ns = read_be_u64(buffer);
        instrument_id = read_be_u32(buffer + 8);
        side = buffer[12];
        reserved[0] = buffer[13];
        reserved[1] = buffer[14];
        reserved[2] = buffer[15];
        price_ticks = read_be_i64(buffer + 16);
        quantity = read_be_u32(buffer + 24);
        trade_id = read_be_u64(buffer + 28);
        venue_id = read_be_u16(buffer + 36);
        source_id = read_be_u16(buffer + 38);
    }
    
    void write_to(byte* buffer) const noexcept {
        write_be_u64(buffer, exchange_ts_ns);
        write_be_u32(buffer + 8, instrument_id);
        buffer[12] = side;
        buffer[13] = reserved[0];
        buffer[14] = reserved[1];
        buffer[15] = reserved[2];
        write_be_i64(buffer + 16, price_ticks);
        write_be_u32(buffer + 24, quantity);
        write_be_u64(buffer + 28, trade_id);
        write_be_u16(buffer + 36, venue_id);
        write_be_u16(buffer + 38, source_id);
    }
};

// Book delta message payload (48 bytes)
struct BookDeltaPayload {
    u64 exchange_ts_ns;
    u32 instrument_id;
    u8 side;
    u8 level;
    u8 operation;  // 0=add, 1=modify, 2=delete
    u8 reserved;
    i64 price_ticks;
    u32 quantity;
    u64 order_id;
    u16 venue_id;
    u16 source_id;
    u32 flags;
    
    static constexpr usize Size = 48;
    
    void read_from(const byte* buffer) noexcept {
        exchange_ts_ns = read_be_u64(buffer);
        instrument_id = read_be_u32(buffer + 8);
        side = buffer[12];
        level = buffer[13];
        operation = buffer[14];
        reserved = buffer[15];
        price_ticks = read_be_i64(buffer + 16);
        quantity = read_be_u32(buffer + 24);
        order_id = read_be_u64(buffer + 28);
        venue_id = read_be_u16(buffer + 36);
        source_id = read_be_u16(buffer + 38);
        flags = read_be_u32(buffer + 40);
    }
    
    void write_to(byte* buffer) const noexcept {
        write_be_u64(buffer, exchange_ts_ns);
        write_be_u32(buffer + 8, instrument_id);
        buffer[12] = side;
        buffer[13] = level;
        buffer[14] = operation;
        buffer[15] = reserved;
        write_be_i64(buffer + 16, price_ticks);
        write_be_u32(buffer + 24, quantity);
        write_be_u64(buffer + 28, order_id);
        write_be_u16(buffer + 36, venue_id);
        write_be_u16(buffer + 38, source_id);
        write_be_u32(buffer + 40, flags);
    }
};

// Status message payload (16 bytes)
struct StatusPayload {
    u64 exchange_ts_ns;
    u32 instrument_id;
    u8 status;  // 0=normal, 1=halted, 2=paused, 3=pre-open, 4=post-close
    u8 reason;
    u16 reserved;
    
    static constexpr usize Size = 16;
    
    void read_from(const byte* buffer) noexcept {
        exchange_ts_ns = read_be_u64(buffer);
        instrument_id = read_be_u32(buffer + 8);
        status = buffer[12];
        reason = buffer[13];
        reserved = read_be_u16(buffer + 14);
    }
    
    void write_to(byte* buffer) const noexcept {
        write_be_u64(buffer, exchange_ts_ns);
        write_be_u32(buffer + 8, instrument_id);
        buffer[12] = status;
        buffer[13] = reason;
        write_be_u16(buffer + 14, reserved);
    }
};

// Heartbeat message payload (8 bytes)
struct HeartbeatPayload {
    u64 exchange_ts_ns;
    
    static constexpr usize Size = 8;
    
    void read_from(const byte* buffer) noexcept {
        exchange_ts_ns = read_be_u64(buffer);
    }
    
    void write_to(byte* buffer) const noexcept {
        write_be_u64(buffer, exchange_ts_ns);
    }
};

} // namespace vtp1

} // namespace vectortick
