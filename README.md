# VectorTick

**JIT-Compiled Market Data Analytics Engine**

VectorTick is a C++20 columnar storage, query, and deterministic replay engine for synthetic market-event data. It implements a complete stack from PCAP/UDP decoding through compressed columnar storage, a typed query language, SSA optimizer, portable vector interpreter, and custom x86-64/AArch64 JIT backends.

## Status

**In Development** - Foundation layers complete, storage and query engine in progress.

### Completed Components
- ✅ Build system (CMake, Makefile)
- ✅ Core types and utilities
- ✅ Memory management (aligned buffers, arenas, mmap)
- ✅ Concurrency primitives (SPSC rings, worker pool)
- ✅ VTP1 wire protocol
- ✅ PCAP reader
- ✅ Compression codecs
- ✅ VTS1 storage format definition

### In Progress
- 🔄 Segment writer/reader implementation
- 🔄 Query language lexer/parser
- 🔄 SSA IR and optimizer
- 🔄 JIT backends

## Quick Start

```bash
# Configure
make configure

# Build
make build

# Run tests
make test

# Run demo
make demo
```

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Applications                             │
│  ingest │ query │ replay │ inspect │ demo                   │
├─────────────────────────────────────────────────────────────┤
│                    Query Engine                              │
│  Parser → Binder → SSA IR → Optimizer → Execution           │
│                      ↓                                       │
│          Reference │ Vector VM │ JIT (x64/ARM)              │
├─────────────────────────────────────────────────────────────┤
│                   Storage Engine                             │
│  VTS1 Format │ Codecs │ Indexes │ Recovery                  │
├─────────────────────────────────────────────────────────────┤
│                   Protocol Layer                             │
│  VTP1 Wire Format │ PCAP Reader │ UDP Receiver              │
├─────────────────────────────────────────────────────────────┤
│                    Core Primitives                           │
│  Types │ Memory │ Concurrency │ Hashing                     │
└─────────────────────────────────────────────────────────────┘
```

## Key Features

### VTP1 Wire Protocol
Original binary protocol for market data with CRC32C validation:
- 40-byte header with magic, version, sequence, timestamp
- Quote, Trade, BookDelta, Status, Heartbeat messages
- Network byte order (big-endian)

### VTS1 Storage Format
Versioned columnar format with:
- 65,536 rows per segment
- Per-column encoding (bit-packed, delta, RLE, dictionary)
- Zone maps and Bloom filters for pruning
- CRC32C checksums on all blocks
- Atomic commit protocol with recovery

### Compression Codecs
- Frame-of-reference bit packing
- Delta and delta-of-delta encoding
- Zigzag encoding for signed integers
- Run-length encoding
- Dictionary encoding

### Query Language
Typed pipeline language:
```
FROM ticks
WHERE instrument_id >= 256 AND instrument_id < 768
AGG count(), sum(quantity), min(price_ticks), max(price_ticks)
```

### JIT Compilation
Custom backends without LLVM:
- x86-64 System V ABI
- AArch64 AAPCS64 ABI
- Linear-scan register allocation
- SIMD kernels (AVX2, NEON)

## Requirements

- C++20 compiler (GCC, Clang, Apple Clang)
- CMake 3.24+
- POSIX system (Linux, macOS)

## License

MIT License - See [LICENSE](LICENSE)

## Disclaimer

**All market data is deterministic and synthetic.** This project does not connect to real exchanges, place trades, or process licensed market data. It is a demonstration of compiler construction, storage engine design, and performance engineering techniques.

## Development Log

- **2025-01-01**: Project foundation, build system, core types
- **2025-01-02**: Memory management and concurrency primitives
- **2025-01-03**: VTP1 protocol and PCAP reader
- **2025-01-04**: Compression codecs
- **2025-01-05**: VTS1 storage format definition

## Repository

https://github.com/Aneesh495/VectorTick
