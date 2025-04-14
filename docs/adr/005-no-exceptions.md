# ADR-005: No Exceptions, No RTTI

## Status

Accepted

## Context

C++ supports multiple error handling mechanisms:
1. Exceptions
2. Error codes
3. `std::expected` (C++23)

For a high-performance system, we need to choose an approach that:
- Has predictable performance characteristics
- Enables compiler optimizations
- Keeps binary size small
- Is easy to use correctly

## Decision

VectorTick is compiled with **`-fno-exceptions -fno-rtti`** and uses **`Status` and `Result<T>`** types for error handling.

### Compiler Flags

```cmake
add_compile_options(
    -fno-exceptions
    -fno-rtti
)
```

### Error Handling Types

#### Status (for void operations)

```cpp
class Status {
public:
    // Factory methods
    static Status OK();
    static Status Error(StatusCode code, std::string_view message);
    
    // Query methods
    bool ok() const;
    StatusCode code() const;
    std::string_view message() const;
    
private:
    StatusCode code_;
    std::string message_;
};
```

#### Result<T> (for value-returning operations)

```cpp
template<typename T>
class Result {
public:
    // Success case
    static Result<T> OK(T value);
    
    // Error case
    static Result<T> Error(StatusCode code, std::string_view message);
    
    // Query methods
    bool ok() const;
    T& value();
    const T& value() const;
    const Status& error() const;
    
    // Convenience
    T& operator*();
    T* operator->();
    
private:
    std::variant<T, Status> data_;
};
```

### Usage Pattern

```cpp
// Void operation
Status write_data(const u8* data, usize size) {
    if (!data || size == 0) {
        return Status::Error(StatusCode::InvalidArgument, "Invalid input");
    }
    // Write data...
    return Status::OK();
}

// Value-returning operation
Result<u32> compute_crc(const u8* data, usize size) {
    if (!data) {
        return Result<u32>::Error(StatusCode::InvalidArgument, "Null pointer");
    }
    u32 crc = crc32c(data, size);
    return Result<u32>::OK(crc);
}

// Caller
auto result = compute_crc(buffer, size);
if (!result.ok()) {
    std::cerr << result.error().message() << "\n";
    return;
}
u32 crc = result.value();
```

## Rationale

### Why No Exceptions?

| Aspect | Exceptions | Status/Result |
|--------|------------|---------------|
| Performance | Unpredictable | Predictable |
| Code Size | Larger (tables) | Smaller |
| Control Flow | Non-obvious | Explicit |
| Interoperability | Poor (C, other languages) | Good |
| Zero-cost (happy path) | Yes | No |

For high-performance systems:
- **Predictable performance** is more important than zero-cost happy path
- **Smaller binary** improves cache efficiency
- **Explicit error handling** makes code easier to reason about

### Why No RTTI?

| Aspect | RTTI | No RTTI |
|--------|------|---------|
| Binary Size | Larger | Smaller |
| `dynamic_cast` | Available | Not available |
| `typeid` | Available | Not available |
| Performance | Slight overhead | No overhead |

We don't use dynamic polymorphism, so RTTI is unnecessary overhead.

### Why Not `std::expected`?

`std::expected` is C++23. We target C++20 for broader compiler support.

## Consequences

### Positive
- Predictable performance (no hidden control flow)
- Smaller binary size
- Explicit error handling (harder to ignore errors)
- Better interoperability (C-compatible types)
- Enables more compiler optimizations

### Negative
- More verbose error handling
- Must check every Result
- Cannot use standard library features that throw
- Must write custom implementations for some utilities

### Mitigations
- Use `Result<T>` consistently for clarity
- Provide helper macros/functions for common patterns
- Use `Status::OK()` for success, early return on error
- Write custom allocators, containers that don't throw

## Guidelines

### Do

```cpp
// Check all results
auto result = operation();
if (!result.ok()) {
    return Status::Error(result.error());
}

// Use early return
Status process(const Input& input) {
    auto r1 = step1(input);
    if (!r1.ok()) return r1.error();
    
    auto r2 = step2(r1.value());
    if (!r2.ok()) return r2.error();
    
    return Status::OK();
}

// Use Status for void, Result<T> for values
Status write(...);
Result<u32> compute(...);
```

### Don't

```cpp
// Don't ignore results
auto result = operation();
// Missing: if (!result.ok()) ...

// Don't use exceptions
throw std::runtime_error("error");

// Don't use standard library throwing functions
std::vector<int> v;
v.at(10);  // Throws!
```

## Implementation

- [common/status.hpp](../../include/vectortick/common/status.hpp)
- [common/result.hpp](../../include/vectortick/common/result.hpp)
- [CMakeLists.txt](../../CMakeLists.txt)

## References

- [C++ Core Guidelines: Error Handling](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-errors)
- [Zero-overhead deterministic exceptions](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0709r0.pdf)
