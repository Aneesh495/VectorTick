#pragma once

#include "types.hpp"
#include <optional>
#include <limits>

namespace vectortick {

// Checked arithmetic operations that return nullopt on overflow
// These are essential for safe parsing and processing of external data

template <typename T>
inline std::optional<T> checked_add(T a, T b) noexcept {
    static_assert(std::is_integral_v<T>, "T must be integral");
    
    if constexpr (std::is_signed_v<T>) {
        if (b > 0 && a > std::numeric_limits<T>::max() - b) {
            return std::nullopt;
        }
        if (b < 0 && a < std::numeric_limits<T>::min() - b) {
            return std::nullopt;
        }
    } else {
        if (a > std::numeric_limits<T>::max() - b) {
            return std::nullopt;
        }
    }
    return static_cast<T>(a + b);
}

template <typename T>
inline std::optional<T> checked_sub(T a, T b) noexcept {
    static_assert(std::is_integral_v<T>, "T must be integral");
    
    if constexpr (std::is_signed_v<T>) {
        if (b > 0 && a < std::numeric_limits<T>::min() + b) {
            return std::nullopt;
        }
        if (b < 0 && a > std::numeric_limits<T>::max() + b) {
            return std::nullopt;
        }
    } else {
        if (a < b) {
            return std::nullopt;
        }
    }
    return static_cast<T>(a - b);
}

template <typename T>
inline std::optional<T> checked_mul(T a, T b) noexcept {
    static_assert(std::is_integral_v<T>, "T must be integral");
    
    if (a == 0 || b == 0) {
        return T{0};
    }
    
    if constexpr (std::is_signed_v<T>) {
        // Signed multiplication overflow check
        if (a > 0) {
            if (b > 0) {
                if (a > std::numeric_limits<T>::max() / b) {
                    return std::nullopt;
                }
            } else {
                if (b < std::numeric_limits<T>::min() / a) {
                    return std::nullopt;
                }
            }
        } else {
            if (b > 0) {
                if (a < std::numeric_limits<T>::min() / b) {
                    return std::nullopt;
                }
            } else {
                if (b < std::numeric_limits<T>::max() / a) {
                    return std::nullopt;
                }
            }
        }
    } else {
        // Unsigned multiplication overflow check
        if (a > std::numeric_limits<T>::max() / b) {
            return std::nullopt;
        }
    }
    
    return static_cast<T>(a * b);
}

template <typename T>
inline std::optional<T> checked_div(T a, T b) noexcept {
    static_assert(std::is_integral_v<T>, "T must be integral");
    
    if (b == 0) {
        return std::nullopt;
    }
    
    // Check for INT_MIN / -1 which would overflow
    if constexpr (std::is_signed_v<T>) {
        if (a == std::numeric_limits<T>::min() && b == -1) {
            return std::nullopt;
        }
    }
    
    return static_cast<T>(a / b);
}

// Safe narrowing conversion
template <typename Dest, typename Src>
inline std::optional<Dest> checked_narrow(Src value) noexcept {
    static_assert(std::is_integral_v<Dest> && std::is_integral_v<Src>,
                  "Both types must be integral");
    
    // Check if value fits in destination type
    if constexpr (std::is_signed_v<Src> == std::is_signed_v<Dest>) {
        // Same signedness
        if (value < static_cast<Src>(std::numeric_limits<Dest>::min()) ||
            value > static_cast<Src>(std::numeric_limits<Dest>::max())) {
            return std::nullopt;
        }
    } else if constexpr (std::is_signed_v<Src>) {
        // Signed to unsigned
        if (value < 0) {
            return std::nullopt;
        }
        if (static_cast<u64>(value) > std::numeric_limits<Dest>::max()) {
            return std::nullopt;
        }
    } else {
        // Unsigned to signed
        if (value > static_cast<u64>(std::numeric_limits<Dest>::max())) {
            return std::nullopt;
        }
    }
    
    return static_cast<Dest>(value);
}

// Saturating arithmetic (clamps instead of overflowing)
template <typename T>
inline T saturating_add(T a, T b) noexcept {
    static_assert(std::is_integral_v<T>, "T must be integral");
    
    auto result = checked_add(a, b);
    return result.value_or(
        (std::is_signed_v<T> && (b < 0)) ? std::numeric_limits<T>::min()
                                         : std::numeric_limits<T>::max()
    );
}

template <typename T>
inline T saturating_sub(T a, T b) noexcept {
    static_assert(std::is_integral_v<T>, "T must be integral");
    
    auto result = checked_sub(a, b);
    return result.value_or(
        (std::is_signed_v<T> && (b > 0)) ? std::numeric_limits<T>::min()
                                         : std::numeric_limits<T>::max()
    );
}

template <typename T>
inline T saturating_mul(T a, T b) noexcept {
    static_assert(std::is_integral_v<T>, "T must be integral");
    
    auto result = checked_mul(a, b);
    if (result) {
        return *result;
    }
    
    // Overflow occurred - determine sign
    bool negative = (a < 0) != (b < 0);
    return negative ? std::numeric_limits<T>::min() 
                    : std::numeric_limits<T>::max();
}

// Align value up to nearest alignment (power of 2 only)
template <typename T>
inline T align_up(T value, usize alignment) noexcept {
    static_assert(std::is_integral_v<T>, "T must be integral");
    const usize mask = alignment - 1;
    return static_cast<T>((static_cast<usize>(value) + mask) & ~mask);
}

// Check if value is aligned
template <typename T>
inline bool is_aligned(T value, usize alignment) noexcept {
    static_assert(std::is_integral_v<T>, "T must be integral");
    return (static_cast<usize>(value) & (alignment - 1)) == 0;
}

// Absolute value without undefined behavior
template <typename T>
inline std::optional<T> checked_abs(T value) noexcept {
    static_assert(std::is_signed_v<T>, "T must be signed");
    
    if (value == std::numeric_limits<T>::min()) {
        return std::nullopt;  // Would overflow
    }
    return static_cast<T>(value < 0 ? -value : value);
}

// Negation without undefined behavior
template <typename T>
inline std::optional<T> checked_neg(T value) noexcept {
    static_assert(std::is_signed_v<T>, "T must be signed");
    
    if (value == std::numeric_limits<T>::min()) {
        return std::nullopt;  // Would overflow
    }
    return static_cast<T>(-value);
}

} // namespace vectortick
