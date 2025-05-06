#pragma once

#include "opcode.hpp"
#include "../common/types.hpp"
#include <string>
#include <variant>

namespace vectortick {

namespace ir {

// Forward declarations
class Instruction;
class BasicBlock;
class Function;

// Value ID type
using ValueId = u32;

// Type system for IR values
enum class Type : u8 {
    Void,
    I1,     // Boolean
    I8,     // 8-bit integer
    I16,    // 16-bit integer
    I32,    // 32-bit integer
    I64,    // 64-bit integer
    U8,     // 8-bit unsigned
    U16,    // 16-bit unsigned
    U32,    // 32-bit unsigned
    U64,    // 64-bit unsigned
    Vector  // Vector of values
};

// Get type name
[[nodiscard]] const char* type_name(Type t) noexcept;

// Get type size in bytes
[[nodiscard]] usize type_size(Type t) noexcept;

// Check if type is signed
[[nodiscard]] bool is_signed_type(Type t) noexcept;

// Check if type is integer
[[nodiscard]] bool is_integer_type(Type t) noexcept;

// IR Value - represents a computed value
class Value {
public:
    Value(ValueId id, Type type, const std::string& name = "")
        : id_(id), type_(type), name_(name) {}
    
    [[nodiscard]] ValueId id() const noexcept { return id_; }
    [[nodiscard]] Type type() const noexcept { return type_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    
    void set_name(const std::string& name) { name_ = name; }
    
    [[nodiscard]] bool is_void() const noexcept { return type_ == Type::Void; }
    [[nodiscard]] bool is_bool() const noexcept { return type_ == Type::I1; }
    [[nodiscard]] bool is_i64() const noexcept { return type_ == Type::I64; }
    [[nodiscard]] bool is_u64() const noexcept { return type_ == Type::U64; }
    
private:
    ValueId id_;
    Type type_;
    std::string name_;
};

// Constant value
struct Constant {
    Type type;
    std::variant<std::int64_t, std::uint64_t, std::uint32_t, std::uint16_t, std::uint8_t, bool> value;
    
    [[nodiscard]] static Constant i64_const(i64 val) {
        Constant c;
        c.type = Type::I64;
        c.value = static_cast<std::int64_t>(val);
        return c;
    }
    
    [[nodiscard]] static Constant u64_const(u64 val) {
        Constant c;
        c.type = Type::U64;
        c.value = static_cast<std::uint64_t>(val);
        return c;
    }
    
    [[nodiscard]] static Constant u32_const(u32 val) {
        Constant c;
        c.type = Type::U32;
        c.value = static_cast<std::uint32_t>(val);
        return c;
    }
    
    [[nodiscard]] static Constant boolean(bool val) {
        Constant c;
        c.type = Type::I1;
        c.value = val;
        return c;
    }
    
    [[nodiscard]] u64 as_u64() const noexcept {
        return std::visit([](auto&& v) -> u64 {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                return v ? 1ULL : 0ULL;
            } else if constexpr (std::is_signed_v<T>) {
                return static_cast<u64>(static_cast<i64>(v));
            } else {
                return static_cast<u64>(v);
            }
        }, value);
    }
    
    [[nodiscard]] i64 as_i64() const noexcept {
        return static_cast<i64>(as_u64());
    }
    
    [[nodiscard]] i64 get_i64() const noexcept { return as_i64(); }
    [[nodiscard]] u64 get_u64() const noexcept { return as_u64(); }
    [[nodiscard]] u32 get_u32() const noexcept { return static_cast<u32>(as_u64()); }
    [[nodiscard]] bool get_bool() const noexcept { return as_u64() != 0; }
};

} // namespace ir

} // namespace vectortick
