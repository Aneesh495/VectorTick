# ADR-003: SSA Intermediate Representation

## Status

Accepted

## Context

The query execution engine needs an intermediate representation (IR) that:

1. Is independent of the source query language
2. Is independent of the target architecture
3. Enables optimization passes
4. Is easy to generate from the query AST
5. Is easy to compile to native code

## Decision

We will use a **Static Single Assignment (SSA)** based IR with the following characteristics:

### SSA Form

In SSA form, each variable is assigned exactly once. This simplifies optimization:

```
// Non-SSA:
x = 1
x = x + 2
y = x

// SSA:
x1 = 1
x2 = x1 + 2
y1 = x2
```

### IR Types

```cpp
enum class Type {
    Void,
    U8, U16, U32, U64,
    I8, I16, I32, I64,
    F32, F64,
    Bool,
    Pointer
};
```

### Instruction Categories

1. **Control Flow**
   - `Nop`, `Branch`, `CondBranch`, `Return`

2. **Constants**
   - `ConstU64`, `ConstI64`, `ConstBool`, `ConstF64`

3. **Arithmetic**
   - `AddU64`, `SubU64`, `MulU64`, `DivU64`
   - `AddI64`, `SubI64`, `MulI64`, `DivI64`

4. **Bitwise**
   - `And`, `Or`, `Xor`, `Shl`, `Shr`, `Not`

5. **Comparison**
   - `EqU64`, `NeU64`, `LtU64`, `LeU64`, `GtU64`, `GeU64`

6. **Memory**
   - `Load`, `Store`, `Alloca`

7. **Conversion**
   - `ZExt`, `SExt`, `Trunc`, `BitCast`

8. **Function**
   - `Call`

### Basic Blocks

```cpp
class BasicBlock {
    std::vector<std::unique_ptr<Instruction>> instructions_;
    std::vector<BasicBlock*> predecessors_;
    std::vector<BasicBlock*> successors_;
    std::string label_;
};
```

### Functions

```cpp
class Function {
    std::vector<std::unique_ptr<BasicBlock>> blocks_;
    std::vector<Type> param_types_;
    Type return_type_;
    std::string name_;
};
```

### Builder Pattern

IR is constructed using a builder:

```cpp
Builder builder;
auto* func = builder.create_function("query");

auto* entry = builder.create_block("entry");
builder.set_insert_point(entry);

auto* param = builder.param_u64(0);
auto* ten = builder.const_u64(10);
auto* result = builder.add_u64(param, ten);
builder.return_u64(result);
```

## Rationale

### Why SSA?

| Aspect | Non-SSA | SSA |
|--------|---------|-----|
| Use-Def Chains | Complex | Trivial (single definition) |
| Dead Code Elimination | Global analysis needed | Local analysis sufficient |
| Constant Propagation | Interprocedural | Intraprocedural |
| Common Subexpression | May miss opportunities | Natural to detect |

SSA form makes most optimizations simpler and more effective.

### Why Typed IR?

1. **Safety**: Type errors caught at compile time
2. **Optimization**: Type-specific optimizations (e.g., unsigned vs signed division)
3. **Code Generation**: Direct mapping to machine types

### Why Our Own IR Instead of LLVM IR?

| Aspect | LLVM IR | Custom IR |
|--------|---------|-----------|
| Complexity | Very high | Moderate |
| Type System | Rich | Simple |
| Optimization | Many passes | Selective |
| Integration | LLVM required | Independent |

For our use case, a simpler IR is sufficient and avoids LLVM dependency.

## Consequences

### Positive
- Simpler optimization passes
- Clear use-def chains
- Easy to generate from AST
- Easy to compile to machine code
- Independent of source language and target architecture

### Negative
- Requires phi nodes for control flow merging
- More instructions than three-address code
- Must maintain SSA invariants

### Mitigations
- Builder simplifies IR construction
- Provide helper functions for common patterns
- Document SSA invariants clearly

## Optimizations

### Implemented

1. **Dead Code Elimination**: Remove unused instructions
2. **Constant Folding**: Evaluate constant expressions
3. **Common Subexpression Elimination**: Reuse computed values

### Planned

1. **Inline Expansion**: Inline small functions
2. **Loop Optimization**: Unroll, vectorize
3. **Tail Call Optimization**: Convert to loops

## Example

### Query
```sql
SELECT price + 10 FROM ticks WHERE instrument_id = 42
```

### IR
```
define u64 @query(u64 %instrument_id, u64 %price) {
entry:
  %0 = const_u64 42
  %1 = eq_u64 %instrument_id, %0
  cond_branch %1, body, exit

body:
  %2 = const_u64 10
  %3 = add_u64 %price, %2
  branch exit

exit:
  %result = phi [%3, body], [0, entry]
  return %result
}
```

## Implementation

- [ir/instruction.hpp](../../include/vectortick/ir/instruction.hpp)
- [ir/basic_block.hpp](../../include/vectortick/ir/basic_block.hpp)
- [ir/function.hpp](../../include/vectortick/ir/function.hpp)
- [ir/builder.hpp](../../include/vectortick/ir/builder.hpp)
- [ir/opcode.hpp](../../include/vectortick/ir/opcode.hpp)
