# ADR-004: Custom VTP1 Wire Protocol

## Status

Accepted

## Context

The system needs a wire protocol for market data transmission. Options considered:

1. **Standard Protocol**: FIX, SBE, Protobuf
2. **Existing Market Data Protocol**: ITCH, OUCH
3. **Custom Protocol**: VTP1

## Decision

We will implement a **custom wire protocol (VTP1)** optimized for our synthetic market data.

### Protocol Design

#### Frame Structure

```
┌─────────────────────────────────────────────────────────────┐
│                      VTP1 Frame                              │
├─────────────────────────────────────────────────────────────┤
│ Header (24 bytes)                                            │
│   ├── Magic: "VT01" (4 bytes)                                │
│   ├── Version: 0x01 (1 byte)                                 │
│   ├── Flags (1 byte)                                         │
│   ├── Sequence Number (8 bytes)                              │
│   ├── Timestamp (8 bytes)                                    │
│   ├── Message Type (1 byte)                                  │
│   └── Payload Length (2 bytes)                               │
├─────────────────────────────────────────────────────────────┤
│ Payload (variable)                                           │
│   └── Message-specific data                                  │
├─────────────────────────────────────────────────────────────┤
│ Trailer (4 bytes)                                            │
│   └── CRC32C (4 bytes)                                       │
└─────────────────────────────────────────────────────────────┘
```

#### Message Types

| Type | Code | Description |
|------|------|-------------|
| Quote | 0x01 | Bid/ask price level |
| Trade | 0x02 | Executed trade |
| BookDelta | 0x03 | Order book change |
| Status | 0x04 | Market status |
| Heartbeat | 0x05 | Keep-alive |

#### Quote Message (28 bytes)

```
┌───────────────────────────────────────────────────┐
│ InstrumentId (4B) │ Side (1B) │ Reserved (3B)    │
├───────────────────────────────────────────────────┤
│ PriceTicks (8B) │ Quantity (8B) │ Flags (2B)     │
└───────────────────────────────────────────────────┘
```

#### Trade Message (32 bytes)

```
┌───────────────────────────────────────────────────┐
│ InstrumentId (4B) │ AggressorSide (1B) │ Reserved │
├───────────────────────────────────────────────────┤
│ PriceTicks (8B) │ Quantity (8B) │ TradeId (8B)    │
└───────────────────────────────────────────────────┘
```

## Rationale

### Why Not FIX?

| Aspect | FIX | VTP1 |
|--------|-----|------|
| Encoding | Text | Binary |
| Size | Large | Compact |
| Parsing | Slow | Fast |
| Adoption | Industry standard | Custom |

FIX is designed for human readability and interoperability, but market data systems need efficiency.

### Why Not SBE (Simple Binary Encoding)?

| Aspect | SBE | VTP1 |
|--------|-----|------|
| Flexibility | Schema-based | Fixed |
| Overhead | Message headers | Minimal |
| Learning curve | Moderate | Simple |

SBE is excellent but adds complexity for our use case.

### Why Not ITCH?

ITCH is exchange-specific with complex message types. Our synthetic data has simpler requirements.

### Why Custom VTP1?

1. **Optimized for our use case**: No unnecessary fields
2. **Simple implementation**: Easy to understand and debug
3. **Efficient**: Binary encoding, minimal overhead
4. **Extensible**: Version field allows evolution

## Consequences

### Positive
- Efficient encoding (binary, minimal overhead)
- Fast parsing (fixed offsets, no variable-length fields)
- Simple implementation
- Easy to debug (fixed structure)
- Extensible (version field)

### Negative
- Not interoperable with standard protocols
- Must maintain encoder/decoder
- Schema evolution requires version handling

### Mitigations
- Provide adapters for standard formats (FIX, SBE)
- Document protocol thoroughly
- Use semantic versioning for changes

## Implementation

### Encoder
```cpp
FrameEncoder encoder;

// Encode quote
Quote quote{.instrument_id = 42, .side = Side::Bid, ...};
auto result = encoder.encode_quote(quote, sequence);
```

### Decoder
```cpp
FrameDecoder decoder;

auto result = decoder.decode(buffer, size);
if (!result.ok()) { /* handle error */ }

Frame frame = result.value();
switch (frame.message_type) {
    case MessageType::Quote:
        Quote quote = decode_quote(frame.payload);
        // Process quote...
        break;
}
```

## Files

- [protocol/frame.hpp](../../include/vectortick/protocol/frame.hpp)
- [protocol/messages.hpp](../../include/vectortick/protocol/messages.hpp)
- [protocol/decoder.hpp](../../include/vectortick/protocol/decoder.hpp)

## Future Considerations

1. **Compression**: Add optional compression flag
2. **Encryption**: Add optional encryption flag
3. **Batching**: Support multiple messages per frame
4. **Schema Registry**: Consider integration for schema evolution
