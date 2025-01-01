#pragma once

#include "types.hpp"
#include <atomic>

namespace vectortick {

// Virtual clock for deterministic replay
// Allows simulating time progression without wall-clock delays
class VirtualClock {
public:
    explicit VirtualClock(u64 start_time_ns = 0) noexcept 
        : current_time_ns_(start_time_ns) {}
    
    // Get current virtual time
    [[nodiscard]] u64 now() const noexcept {
        return current_time_ns_.load(std::memory_order_acquire);
    }
    
    // Advance time (monotonically increasing only)
    void advance_to(u64 time_ns) noexcept {
        u64 current = current_time_ns_.load(std::memory_order_relaxed);
        while (time_ns > current && 
               !current_time_ns_.compare_exchange_weak(
                   current, time_ns,
                   std::memory_order_release,
                   std::memory_order_relaxed)) {
            // Retry
        }
    }
    
    // Advance by a duration
    void advance_by(u64 duration_ns) noexcept {
        u64 current = current_time_ns_.load(std::memory_order_relaxed);
        advance_to(current + duration_ns);
    }
    
    // Reset clock (for testing)
    void reset(u64 time_ns = 0) noexcept {
        current_time_ns_.store(time_ns, std::memory_order_release);
    }
    
private:
    std::atomic<u64> current_time_ns_;
};

// Rate controller for paced replay
// Converts virtual time to wall-clock delays
class RateController {
public:
    // Speed factor: 1.0 = real-time, 2.0 = 2x speed, 0.0 = maximum speed
    explicit RateController(double speed_factor = 0.0) noexcept
        : speed_factor_(speed_factor)
        , virtual_start_(0)
        , wall_start_ns_(0)
        , running_(false) {}
    
    // Start timing from a virtual time
    void start(u64 virtual_time_ns, u64 wall_time_ns) noexcept {
        virtual_start_ = virtual_time_ns;
        wall_start_ns_ = wall_time_ns;
        running_ = true;
    }
    
    // Calculate how long to wait for a virtual time
    // Returns nanoseconds to wait, or 0 if behind or unlimited speed
    [[nodiscard]] u64 wait_duration(u64 virtual_time_ns, u64 current_wall_ns) const noexcept {
        if (!running_ || speed_factor_ <= 0.0) {
            return 0;
        }
        
        // Calculate expected wall time for this virtual time
        u64 virtual_elapsed = virtual_time_ns - virtual_start_;
        u64 expected_wall_elapsed = static_cast<u64>(virtual_elapsed / speed_factor_);
        u64 expected_wall_time = wall_start_ns_ + expected_wall_elapsed;
        
        if (expected_wall_time > current_wall_ns) {
            return expected_wall_time - current_wall_ns;
        }
        
        return 0;  // We're behind, no delay
    }
    
    // Set speed factor
    void set_speed_factor(double factor) noexcept {
        speed_factor_ = factor > 0.0 ? factor : 0.0;
    }
    
    [[nodiscard]] double speed_factor() const noexcept {
        return speed_factor_;
    }
    
    void stop() noexcept {
        running_ = false;
    }
    
    [[nodiscard]] bool is_running() const noexcept {
        return running_;
    }
    
private:
    double speed_factor_;
    u64 virtual_start_;
    u64 wall_start_ns_;
    bool running_;
};

// Get current wall-clock time in nanoseconds
[[nodiscard]] u64 get_wall_time_ns() noexcept;

// Sleep for specified nanoseconds
void sleep_ns(u64 duration_ns) noexcept;

// Convert between time units
[[nodiscard]] inline u64 ns_to_us(u64 ns) noexcept { return ns / 1000; }
[[nodiscard]] inline u64 ns_to_ms(u64 ns) noexcept { return ns / 1000000; }
[[nodiscard]] inline u64 ns_to_sec(u64 ns) noexcept { return ns / 1000000000; }
[[nodiscard]] inline u64 us_to_ns(u64 us) noexcept { return us * 1000; }
[[nodiscard]] inline u64 ms_to_ns(u64 ms) noexcept { return ms * 1000000; }
[[nodiscard]] inline u64 sec_to_ns(u64 sec) noexcept { return sec * 1000000000; }

// Time window for tumbling windows
struct TimeWindow {
    u64 start_ns;
    u64 end_ns;
    u64 duration_ns;
    
    [[nodiscard]] bool contains(u64 time_ns) const noexcept {
        return time_ns >= start_ns && time_ns < end_ns;
    }
    
    [[nodiscard]] u64 window_index() const noexcept {
        return start_ns / duration_ns;
    }
};

// Calculate tumbling window for a timestamp
[[nodiscard]] inline TimeWindow tumble_window(u64 time_ns, u64 window_duration_ns) noexcept {
    u64 index = time_ns / window_duration_ns;
    return TimeWindow{
        index * window_duration_ns,
        (index + 1) * window_duration_ns,
        window_duration_ns
    };
}

} // namespace vectortick
