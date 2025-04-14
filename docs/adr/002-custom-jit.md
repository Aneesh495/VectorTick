# ADR-002: Custom JIT Backend (No LLVM)

## Status

Accepted

## Context

The query execution engine needs to compile SSA IR to native machine code for maximum performance. We considered three approaches:

1. **Interpretation**: Execute IR directly
2. **LLVM JIT**: Use LLVM's JIT infrastructure
3. **Custom JIT**: Implement our own x86-64/AArch64 backends

## Decision

We will implement **custom JIT backends** for x86-64 and AArch64 without using LLVM.

### Architecture

```
┌───────────────────────────────────────────────────────────┐
│                    JIT Compiler                            │
├───────────────────────────────────────────────────────────┤
│ IR Function → Register Allocator → Instruction Selector   │
│                    ↓                                       │
│          ┌───────────────────────────────┐                │
│          │    Target Architecture        │                │
│          ├───────────────┬───────────────┤                │
│          │   x86-64      │   AArch64     │                │
│          │   Backend     │   Backend     │                │
│          └───────────────┴───────────────┘                │
│                    ↓                                       │
│          Executable Memory (mmap + mprotect)              │
└───────────────────────────────────────────────────────────┘
```

### Components

1. **Register Allocator**: Linear scan with spilling
2. **Instruction Selector**: Pattern matching on IR opcodes
3. **Assembler**: Binary encoding for target architecture
4. **Memory Manager**: mmap-based executable memory

### Supported Instructions

#### x86-64
- Arithmetic: ADD, SUB, IMUL, IDIV
- Logical: AND, OR, XOR, NOT
- Comparison: CMP, TEST
- Control: JMP, CALL, RET
- SIMD: AVX2 vector operations

#### AArch64
- Arithmetic: ADD, SUB, MUL, SDIV, UDIV
- Logical: AND, ORR, EOR, MVN
- Comparison: CMP, CMPI
- Control: B, BL, RET
- SIMD: NEON vector operations

## Rationale

### Why Not Interpretation?

| Metric | Interpreter | JIT |
|--------|-------------|-----|
| Latency | 100+ ns/op | 0.5-2 ns/op |
| Throughput | 10M ops/sec | 2B ops/sec |
| Memory | Low | Code cache |

For analytics workloads, JIT compilation provides 10-100x speedup.

### Why Not LLVM?

| Aspect | LLVM JIT | Custom JIT |
|--------|----------|------------|
| Binary Size | 50+ MB | < 1 MB |
| Compile Time | 1-10 ms | 0.1-1 ms |
| Dependencies | Heavy | None |
| Optimization | World-class | Good enough |
| Portability | Many targets | x86-64, ARM64 |
| Control | Limited | Full |

For our use case:
- **Compilation latency matters**: LLVM's optimization passes add latency
- **Binary size matters**: Embedded deployments require small footprint
- **Dependencies matter**: LLVM is a heavy dependency
- **We control the IR**: Can optimize at IR level, JIT just needs good codegen

### Why Support Both x86-64 and AArch64?

1. **x86-64**: Dominant server architecture
2. **AArch64**: Growing in data centers (AWS Graviton, Apple Silicon)
3. **Same IR**: Architecture-specific backends share the same IR

## Consequences

### Positive
- Fast compilation (sub-millisecond)
- Small binary footprint (< 1 MB)
- No external dependencies
- Full control over code generation
- Good performance for our workload

### Negative
- Less optimized than LLVM
- Limited architecture support
- Must maintain multiple backends
- No advanced optimizations (vectorization, inlining heuristics)

### Mitigations
- Optimize heavily at IR level before codegen
- Add SIMD intrinsics for hot paths
- Profile-guided optimization for codegen

## Alternatives Considered

### LLVM ORC JIT
- Rejected due to dependency size and compile-time latency

### Cranelift
- Considered for future - lighter than LLVM but still adds dependency
- May add as optional backend in the future

### libjit / GNU Lightning
- Rejected - limited to specific architectures, less control

## Implementation

- [jit/code_generator.hpp](../../include/vectortick/jit/code_generator.hpp)
- [jit/x86_assembler.hpp](../../include/vectortick/jit/x86_assembler.hpp)
- [jit/a64_assembler.hpp](../../include/vectortick/jit/a64_assembler.hpp)
- [jit/jit_compiler.hpp](../../include/vectortick/jit/jit_compiler.hpp)

## Performance

Benchmarks on Apple M1 (Release build):

| Operation | Time |
|-----------|------|
| IR Compilation | 20 μs |
| JIT Compilation | 100 μs |
| JIT Execution (simple add) | 0.5 ns |
| JIT Execution (complex query) | 5 ns |
