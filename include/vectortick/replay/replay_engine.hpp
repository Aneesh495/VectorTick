#pragma once

#include "vectortick/common/types.hpp"
#include "vectortick/model/event.hpp"
#include "vectortick/storage/catalog.hpp"
#include "vectortick/storage/segment_reader.hpp"

#include <functional>
#include <string>
#include <vector>

namespace vectortick {

struct ReplayOptions {
    u64 rate_limit_events_per_sec{0}; // 0 = unlimited
    bool preserve_relative_timing{false};
    double time_dilation{1.0};        // 1.0 = real-time, 2.0 = 2x speed
    bool verify_sequence{true};
};

struct ReplayStatistics {
    u64 total_events{0};
    u64 sequence_breaks{0};
    u64 out_of_order_events{0};
    u64 min_sequence{0};
    u64 max_sequence{0};
    u64 elapsed_wall_ns{0};
    double throughput_events_per_sec{0.0};
};

class ReplayEngine {
public:
    using EventCallback = std::function<void(const CanonicalEvent&)>;

    explicit ReplayEngine(ReplayOptions options = {});

    const ReplayOptions& options() const { return options_; }
    void set_options(const ReplayOptions& options) { options_ = options; }

    const ReplayStatistics& stats() const { return stats_; }
    void reset_stats();

    Status replay_events(const std::vector<CanonicalEvent>& events,
                         EventCallback callback = nullptr);

    Status replay_segment(const std::string& segment_path,
                          EventCallback callback = nullptr);

    Status replay_catalog(const Catalog& catalog,
                          EventCallback callback = nullptr);

private:
    void process_event(const CanonicalEvent& ev, EventCallback callback);

    ReplayOptions options_;
    ReplayStatistics stats_;
    u64 last_sequence_{0};
    u64 last_receive_ts_{0};
    bool first_event_{true};
};

} // namespace vectortick
