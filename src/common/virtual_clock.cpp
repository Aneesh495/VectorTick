#include "vectortick/common/virtual_clock.hpp"
#include <chrono>
#include <thread>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#elif defined(__linux__)
#include <time.h>
#endif

namespace vectortick {

u64 get_wall_time_ns() noexcept {
#if defined(__APPLE__)
    static mach_timebase_info_data_t timebase = {0, 0};
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }
    u64 now = mach_absolute_time();
    return now * timebase.numer / timebase.denom;
#elif defined(__linux__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<u64>(ts.tv_sec) * 1000000000ULL + static_cast<u64>(ts.tv_nsec);
#else
    // Fallback using C++ steady_clock
    auto now = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
    return static_cast<u64>(ns.count());
#endif
}

void sleep_ns(u64 duration_ns) noexcept {
    if (duration_ns == 0) return;
    
    std::this_thread::sleep_for(std::chrono::nanoseconds(duration_ns));
}

} // namespace vectortick
