# VectorTick Architecture

This document provides a detailed technical overview of VectorTick's architecture, design decisions, and implementation details.

## Table of Contents

1. [Overview](#overview)
2. [Layered Architecture](#layered-architecture)
3. [Core Primitives](#core-primitives)
4. [Protocol Layer](#protocol-layer)
5. [Storage Engine](#storage-engine)
6. [Query Engine](#query-engine)
7. [IR and Optimization](#ir-and-optimization)
8. [JIT Compilation](#jit-compilation)
9. [Memory Management](#memory-management)
10. [Concurrency Model](#concurrency-model)
11. [Performance Considerations](#performance-considerations)

---

## Overview

VectorTick is a high-performance market data analytics engine built around three core principles:

1. **Columnar Storage**: Data is stored in columnar format for efficient compression and SIMD-friendly access patterns
2. **SSA IR**: Queries are compiled to a typed SSA intermediate representation for optimization
3. **Native Execution**: IR is compiled to native machine code via custom JIT backends

The architecture follows a layered approach where each layer depends only on the layers below it, enabling clean separation of concerns and testability.

---

## Layered Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ Layer 5: Applications (ingest, query, replay, inspect, demo)                │
├─────────────────────────────────────────────────────────────────────────────┤
│ Layer 4: Query Engine (lexer, parser, AST, binder)                          │
│           + Execution (interpreter, vector VM, JIT)                          │
├─────────────────────────────────────────────────────────────────────────────┤
│ Layer 3: IR Layer (builder, basic blocks, instructions, functions, optimizer)│
├─────────────────────────────────────────────────────────────────────────────┤
│ Layer 2: Storage Engine (segment reader/writer, codecs, indexes)            │
│           + Protocol Layer (VTP1, PCAP)                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│ Layer 1: Core Primitives (types, memory, concurrency, hashing)              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Dependency Rules

- Layer N may only depend on Layer N-1 and below
- No upward dependencies
- No circular dependencies between modules

---

## Core Primitives

### Fixed-Width Types

All types are explicitly sized for portability:

```cpp
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using f32 = float;
using f64 = double;
using usize = std::size_t;
using isize = std::ptrdiff_t;
```

### Status and Result Types

Error handling uses `Status` for operations without return values and `Result<T>` for operations with return values:

```cpp
// Status for void operations
Status write_data(const u8* data, usize size);
auto status = write_data(buffer, size);
if (!status.ok()) {
    std::cerr << status.message() << "\n";
}

// Result for value-returning operations
Result<u32> compute_crc(const u8* data, usize size);
auto result = compute_crc(buffer, size);
if (!result.ok()) {
    std::cerr << result.error().message() << "\n";
} else {
    u32 crc = result.value();
}
```

### Endian Handling

All wire formats use network byte order (big-endian). The endian module provides efficient conversions:

```cpp
// Convert to network byte order
u32 network_value = host_to_network(host_value);

// Convert from network byte order
u32 host_value = network_to_host(network_value);

// Read big-endian value from buffer
u32 value = read_be_u32(buffer);

// Write big-endian value to buffer
write_be_u32(buffer, value);
```

### CRC32C

Hardware-accelerated CRC32C computation using x86 CRC32 instruction or ARM PMULL:

```cpp
u32 crc = crc32c(data, size);

// Incremental computation
u32 crc = 0;
crc = crc32c(crc, data1, size1);
crc = crc32c(crc, data2, size2);
```

---

## Protocol Layer

### VTP1 Wire Format

VTP1 is a binary wire protocol designed for market data:

```
Frame Header (24 bytes):
┌────────────────────────────────────────────────────┐
│ Magic (4B) │ Ver │ Flags │ Seq (8B) │ Ts (8B)     │
├────────────────────────────────────────────────────┤
│ MsgType │ PayloadLen (2B) │ Payload (variable)    │
└────────────────────────────────────────────────────┘

Magic:      0x56543031 ("VT01")
Version:    Protocol version (currently 0x01)
Flags:      Message flags (compression, encryption, etc.)
Sequence:   Monotonic sequence number
Timestamp:  Nanoseconds since Unix epoch
MsgType:    Message type identifier
PayloadLen: Length of payload in bytes
Payload:    Message-specific data
```

### Message Types

#### Quote Message
```
┌─────────────────────────────────────────────────────┐
│ InstrumentId (4B) │ Side (1B) │ Reserved (3B)       │
├─────────────────────────────────────────────────────┤
│ PriceTicks (8B) │ Quantity (8B) │ Flags (2B)        │
└─────────────────────────────────────────────────────┘
```

#### Trade Message
```
┌─────────────────────────────────────────────────────┐
│ InstrumentId (4B) │ AggressorSide (1B) │ Reserved   │
├─────────────────────────────────────────────────────┤
│ PriceTicks (8B) │ Quantity (8B) │ TradeId (8B)      │
└─────────────────────────────────────────────────────┘
```

### PCAP Reader

The PCAP reader handles both standard PCAP and PCAPNG formats:

```cpp
PCAPReader reader;
auto status = reader.open("market_data.pcap");
if (!status.ok()) { /* handle error */ }

while (reader.has_more()) {
    auto result = reader.next_packet();
    if (!result.ok()) { /* handle error */ }
    
    Packet packet = result.value();
    // Process packet...
}
```

---

## Storage Engine

### VTS1 File Format

VTS1 is a columnar storage format optimized for time-series market data:

```
┌────────────────────────────────────────────────────────────┐
│                      VTS1 File                              │
├────────────────────────────────────────────────────────────┤
│ File Header (64 bytes)                                      │
│   ├── Magic: "VTS1" (4 bytes)                               │
│   ├── Version: 0x0001 (2 bytes)                             │
│   ├── Flags (2 bytes)                                       │
│   ├── Segment Count (8 bytes)                               │
│   ├── Total Row Count (8 bytes)                             │
│   ├── Timestamp Min (8 bytes)                               │
│   ├── Timestamp Max (8 bytes)                               │
│   ├── Schema Offset (8 bytes)                               │
│   ├── Footer Offset (8 bytes)                               │
│   └── CRC32C (4 bytes)                                      │
├────────────────────────────────────────────────────────────┤
│ Schema Definition                                           │
│   ├── Column Count (4 bytes)                                │
│   └── Column Definitions                                    │
│       ├── Column Name Length (2 bytes)                      │
│       ├── Column Name (variable)                            │
│       ├── Column Type (1 byte)                              │
│       ├── Encoding Type (1 byte)                            │
│       └── Column Offset (8 bytes)                           │
├────────────────────────────────────────────────────────────┤
│ Segment 1 (65,536 rows max)                                 │
│   ├── Segment Header                                        │
│   │   ├── Row Count (4 bytes)                               │
│   │   ├── Data Offset (8 bytes)                             │
│   │   └── CRC32C (4 bytes)                                  │
│   ├── Column Data                                           │
│   │   ├── Column 1 (encoded)                                │
│   │   │   ├── Encoding Header                               │
│   │   │   ├── Compressed Size (4 bytes)                     │
│   │   │   └── Compressed Data                               │
│   │   ├── Column 2 (encoded)                                │
│   │   └── ...                                               │
│   ├── Zone Maps                                             │
│   │   └── Per-column min/max for each 1024-row zone         │
│   └── Bloom Filters                                         │
│       └── Per-column bloom filter for equality predicates   │
├────────────────────────────────────────────────────────────┤
│ Segment 2                                                   │
│ └── ...                                                     │
├────────────────────────────────────────────────────────────┤
│ File Footer                                                 │
│   ├── Segment Offsets (8 bytes each)                        │
│   └── CRC32C (4 bytes)                                      │
└────────────────────────────────────────────────────────────┘
```

### Column Encoding Selection

The optimal encoding is selected based on data characteristics:

```cpp
EncodingType select_encoding(const ColumnStats& stats) {
    // Low cardinality - dictionary encoding
    if (stats.unique_count < stats.row_count * 0.1) {
        return EncodingType::Dictionary;
    }
    
    // Many repeated values - RLE
    if (stats.run_count < stats.row_count * 0.5) {
        return EncodingType::RLE;
    }
    
    // Small integer range - bit packing
    if (stats.type == Type::U64 && 
        stats.max_value - stats.min_value < (1ULL << 16)) {
        return EncodingType::BitPack;
    }
    
    // Sequential values - delta encoding
    if (stats.is_sequential) {
        return EncodingType::Delta;
    }
    
    // Default - varint for integers
    return EncodingType::VarInt;
}
```

### Segment Writer

```cpp
SegmentWriter writer;

// Add rows
for (const auto& event : events) {
    writer.add_row(event);
}

// Flush to file
auto status = writer.flush("output.vts1");
```

### Segment Reader

```cpp
SegmentReader reader;
auto status = reader.open("output.vts1");
if (!status.ok()) { /* handle error */ }

// Read with predicate pushdown
auto result = reader.read_rows(
    /* time_range */ {min_ts, max_ts},
    /* predicate */ [](const Row& row) { return row.instrument_id == 42; }
);
```

---

## Query Engine

### Lexer

The lexer tokenizes query text into tokens:

```cpp
Lexer lexer("FROM ticks WHERE price > 100");
auto tokens = lexer.tokenize();
// tokens: [FROM, ID("ticks"), WHERE, ID("price"), GT, INT(100)]
```

### Token Types

```cpp
enum class TokenType {
    // Keywords
    FROM, WHERE, SELECT, AGG, GROUP, BY,
    AND, OR, NOT,
    
    // Literals
    INT_LITERAL, STRING_LITERAL,
    
    // Identifiers
    IDENTIFIER,
    
    // Operators
    PLUS, MINUS, STAR, SLASH, PERCENT,
    EQ, NE, LT, LE, GT, GE,
    
    // Punctuation
    LPAREN, RPAREN, COMMA,
    
    // Special
    END_OF_FILE, ERROR
};
```

### Parser

Recursive descent parser builds an AST:

```cpp
Parser parser(tokens);
auto ast = parser.parse_query();
// ast: Query { from: "ticks", where: BinaryOp(GT, Column("price"), 100) }
```

### AST Nodes

```cpp
struct Query {
    std::string from_table;
    std::optional<std::unique_ptr<Expression>> where_clause;
    std::vector<std::unique_ptr<Expression>> select_list;
    std::vector<std::unique_ptr<Expression>> group_by;
    std::vector<std::unique_ptr<Expression>> agg_list;
};

struct BinaryOp {
    BinaryOpType type;  // ADD, SUB, MUL, DIV, AND, OR, EQ, NE, LT, LE, GT, GE
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

struct FunctionCall {
    std::string name;  // count, sum, min, max, avg
    std::vector<std::unique_ptr<Expression>> args;
};
```

### Binder

The binder resolves identifiers to column references:

```cpp
Binder binder(schema);
auto bound_query = binder.bind(query);
// Resolves column names, checks types, validates aggregates
```

---

## IR and Optimization

### SSA Form

The IR uses Static Single Assignment form where each value is defined exactly once:

```cpp
// IR for: x = a + b; y = x * c; return y
%1 = param 0        ; a
%2 = param 1        ; b
%3 = param 2        ; c
%4 = add %1, %2     ; x = a + b
%5 = mul %4, %3     ; y = x * c
return %5
```

### Basic Blocks

```cpp
class BasicBlock {
    std::vector<std::unique_ptr<Instruction>> instructions_;
    std::vector<BasicBlock*> predecessors_;
    std::vector<BasicBlock*> successors_;
};
```

### Instructions

```cpp
enum class Opcode {
    // Control flow
    Nop, Branch, CondBranch, Return,
    
    // Constants
    ConstU8, ConstU16, ConstU32, ConstU64,
    ConstI8, ConstI16, ConstI32, ConstI64,
    ConstBool, ConstF32, ConstF64,
    
    // Arithmetic
    AddU64, SubU64, MulU64, DivU64, ModU64,
    AddI64, SubI64, MulI64, DivI64, ModI64,
    
    // Bitwise
    And, Or, Xor, Shl, Shr, Not,
    
    // Comparison
    EqU64, NeU64, LtU64, LeU64, GtU64, GeU64,
    
    // Memory
    Load, Store, Alloca,
    
    // Conversion
    ZExt, SExt, Trunc, BitCast,
    
    // Call
    Call
};
```

### IR Builder

```cpp
ir::Builder builder;
auto* func = builder.create_function("compute");

auto* entry = builder.create_block();
builder.set_insert_point(entry);

auto* a = builder.param_u64(0);
auto* b = builder.param_u64(1);
auto* sum = builder.add_u64(a, b);
builder.return_u64(sum);
```

### Optimizations

#### Dead Code Elimination
```cpp
// Before:
%1 = add %0, 10
%2 = mul %1, 2      ; unused
return %1

// After:
%1 = add %0, 10
return %1
```

#### Constant Folding
```cpp
// Before:
%1 = const 10
%2 = const 20
%3 = add %1, %2

// After:
%3 = const 30
```

#### Common Subexpression Elimination
```cpp
// Before:
%1 = add %0, 10
%2 = add %0, 10      ; duplicate
%3 = mul %1, %2

// After:
%1 = add %0, 10
%3 = mul %1, %1
```

---

## JIT Compilation

### Architecture Detection

```cpp
#if defined(__x86_64__) || defined(_M_X64)
    #define VT_ARCH_X86_64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define VT_ARCH_AARCH64 1
#endif
```

### Memory Protection

JIT memory is managed with proper protection:

```cpp
// Allocate RW memory
void* code = mmap(nullptr, size, PROT_READ | PROT_WRITE, 
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

// Write code...

// Make executable (RW → RX)
mprotect(code, size, PROT_READ | PROT_EXEC);
```

### x86-64 Code Generation

#### Register Allocation

```cpp
// Caller-saved (available for allocation)
RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11

// Callee-saved (must be preserved)
RBX, R12, R13, R14, R15, RBP
```

#### Instruction Selection

```cpp
switch (instr->opcode()) {
    case Opcode::AddU64:
        // add dst, src
        emit_rex_prefix(/* is_64bit */ true, dst, src);
        emit_byte(0x01);  // ADD r/m64, r64
        emit_modrm(/* mod */ 3, src, dst);
        break;
        
    case Opcode::MulU64:
        // imul dst, src
        emit_rex_prefix(true, dst, src);
        emit_byte(0x0F);
        emit_byte(0xAF);  // IMUL r64, r/m64
        emit_modrm(3, dst, src);
        break;
}
```

### AArch64 Code Generation

#### Register Allocation

```cpp
// Caller-saved (available for allocation)
X0-X18

// Callee-saved (must be preserved)
X19-X28, X29 (FP), X30 (LR)
```

#### Instruction Encoding

```cpp
// ADD Xd, Xn, Xm
// Format: sf=1 opc=00 shift=00 N=0 Rm=Xm imm6=000000 Rn=Xn Rd=Xd
u32 encode_add_x64(A64Reg dst, A64Reg src1, A64Reg src2) {
    return (1 << 31) |          // sf = 1 (64-bit)
           (src2 << 16) |        // Rm
           (src1 << 5) |         // Rn
           dst;                  // Rd
}

// MOV Xd, Xn
// Alias for ADD Xd, Xn, XZR
void mov_x64_x64(A64Reg dst, A64Reg src) {
    add_x64_x64_x64(dst, src, A64Reg::XZR);
}
```

### Function Pointer Execution

```cpp
// JIT compile
auto code = jit.compile(func);

// Execute
using FuncPtr = u64(*)(u64, u64);
FuncPtr fn = reinterpret_cast<FuncPtr>(code.data());
u64 result = fn(arg1, arg2);
```

---

## Memory Management

### Arena Allocator

Fast bump allocation for temporary data:

```cpp
Arena arena(4096);  // 4 KB initial size

// Allocate
void* ptr = arena.allocate(64);  // Fast, no free needed

// Reset (frees all at once)
arena.reset();
```

### Buffer Pool

Pooled allocation for frequently-used buffer sizes:

```cpp
BufferPool pool(64 * 1024);  // 64 KB buffers

// Acquire
Buffer* buf = pool.acquire();

// Use...

// Release
pool.release(buf);
```

### Aligned Buffers

SIMD-aligned buffers for vectorized operations:

```cpp
// 64-byte aligned for AVX-512
AlignedBuffer buffer(1024, 64);

// Access aligned pointer
void* ptr = buffer.data();
assert(reinterpret_cast<uintptr_t>(ptr) % 64 == 0);
```

### Memory-Mapped Files

Efficient file I/O via mmap:

```cpp
MappedFile file;
auto status = file.open("data.bin", MappedFile::Read);
if (!status.ok()) { /* error */ }

// Direct memory access
const u8* data = file.data();
usize size = file.size();

// Automatic unmapping on close
```

---

## Concurrency Model

### SPSC Ring Buffer

Single-producer single-consumer lock-free queue:

```cpp
SPSCRing<Event> ring(1024);

// Producer thread
ring.push(event);

// Consumer thread
auto result = ring.pop();
if (result.has_value()) {
    Event event = result.value();
}
```

### Worker Pool

Thread pool for parallel tasks:

```cpp
WorkerPool pool(4);  // 4 worker threads

// Submit work
pool.submit([]() {
    // Do work...
});

// Wait for completion
pool.wait();
```

---

## Performance Considerations

### Cache Optimization

- Data is aligned to cache line boundaries (64 bytes)
- Columnar layout enables sequential access
- Hot paths are optimized for L1 cache

### SIMD Vectorization

- AVX2/AVX-512 on x86-64
- NEON on AArch64
- Vectorized compression/decompression

### Branch Prediction

- Hot paths avoid branches
- Switch statements use jump tables
- Profile-guided optimization recommended

### Memory Locality

- Arena allocation for temporary data
- Buffer pooling to reduce allocation overhead
- Columnar storage for sequential scans

### JIT Optimizations

- Inline caching for hot paths
- Direct patching for quick calls
- Lazy compilation for cold functions

---

## Further Reading

- [BUILD_SPEC.md](BUILD_SPEC.md) - Build system specification
- [README.md](../README.md) - Project overview and quick start
- [Architecture Decision Records](adr/) - Key design decisions
