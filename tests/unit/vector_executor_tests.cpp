#include "../test_framework.hpp"
#include "vectortick/execution/vector_executor.hpp"
#include "vectortick/execution/reference_interpreter.hpp"
#include "vectortick/query/parser.hpp"
#include "vectortick/storage/segment_writer.hpp"
#include "vectortick/storage/segment_reader.hpp"

#include <filesystem>
#include <vector>

using namespace vectortick;
using namespace vectortick::test;

VT_TEST(vector_executor_tests, simd_detection_and_primitive_kernels) {
    SimdArch arch = SimdDispatcher::detect_host_arch();
    VT_ASSERT(arch == SimdArch::NEON || arch == SimdArch::AVX2 || arch == SimdArch::Generic);

    const char* name = SimdDispatcher::arch_name(arch);
    VT_ASSERT(name != nullptr);

    // Test filter_gt_i64
    constexpr usize N = 100;
    i64 data[N];
    for (usize i = 0; i < N; ++i) {
        data[i] = static_cast<i64>(i * 10);
    }

    u16 out_sel[N];
    // Threshold 500: values 510..990 match (indices 51..99 => 49 items)
    usize matched = SimdDispatcher::filter_gt_i64(data, 500, nullptr, N, out_sel);
    VT_ASSERT_EQ(matched, 49ULL);
    VT_ASSERT_EQ(static_cast<u64>(out_sel[0]), 51ULL);
    VT_ASSERT_EQ(static_cast<u64>(out_sel[matched - 1]), 99ULL);

    // Test sum_i64
    i64 total = SimdDispatcher::sum_i64(data, nullptr, N);
    // 0 + 10 + 20 + ... + 990 = 10 * (99 * 100 / 2) = 49500
    VT_ASSERT_EQ(total, 49500LL);

    // Test min and max
    i64 min_val = SimdDispatcher::min_i64(data, nullptr, N);
    i64 max_val = SimdDispatcher::max_i64(data, nullptr, N);
    VT_ASSERT_EQ(min_val, 0LL);
    VT_ASSERT_EQ(max_val, 990LL);
}

VT_TEST(vector_executor_tests, execute_projection_and_filter) {
    std::vector<CanonicalEvent> events;
    for (usize i = 0; i < 200; ++i) {
        CanonicalEvent ev{};
        ev.sequence = i + 1;
        ev.exchange_ts_ns = 1'000'000'000ULL + i * 1000;
        ev.receive_ts_ns = ev.exchange_ts_ns + 50;
        ev.instrument_id = (i % 2 == 0) ? 1001 : 1002;
        ev.event_type = EventType::Trade;
        ev.side = Side::Bid;
        ev.price_ticks = static_cast<i64>(10000 + i * 100);
        ev.quantity = static_cast<u32>(10 + i);
        events.push_back(ev);
    }

    query::Parser parser("SELECT instrument_id, price_ticks, quantity WHERE price_ticks > 25000 LIMIT 10");
    auto parse_res = parser.parse_query();
    VT_ASSERT(parse_res.ok());

    VectorExecutor exec;
    auto query_res = exec.execute_events(events, parse_res.value().get());
    VT_ASSERT(query_res.ok());

    const auto& res = query_res.value();
    VT_ASSERT_EQ(res.rows_scanned, 200ULL);
    // price > 25000: 10000 + i*100 > 25000 => i*100 > 15000 => i > 150 => i in [151, 199] => 49 matched
    VT_ASSERT_EQ(res.rows_matched, 49ULL);
    // Limit is 10
    VT_ASSERT_EQ(res.rows.size(), 10ULL);

    // Verify first row
    // i = 151: price = 10000 + 15100 = 25100, qty = 161
    VT_ASSERT_EQ(res.rows[0][1], "25100");
    VT_ASSERT_EQ(res.rows[0][2], "161");
}

VT_TEST(vector_executor_tests, execute_aggregations) {
    std::vector<CanonicalEvent> events;
    for (usize i = 0; i < 100; ++i) {
        CanonicalEvent ev{};
        ev.sequence = i + 1;
        ev.exchange_ts_ns = 1'000'000'000ULL + i;
        ev.instrument_id = 500;
        ev.price_ticks = 1000;
        ev.quantity = 10;
        events.push_back(ev);
    }

    query::Parser parser("SELECT COUNT(*), SUM(quantity), MIN(price_ticks), MAX(price_ticks)");
    auto parse_res = parser.parse_query();
    VT_ASSERT(parse_res.ok());

    VectorExecutor exec;
    auto query_res = exec.execute_events(events, parse_res.value().get());
    VT_ASSERT(query_res.ok());

    const auto& res = query_res.value();
    VT_ASSERT_EQ(res.rows.size(), 1ULL);
    VT_ASSERT_EQ(res.rows[0][0], "100");    // COUNT
    VT_ASSERT_EQ(res.rows[0][1], "1000");   // SUM(10 * 100)
    VT_ASSERT_EQ(res.rows[0][2], "1000");   // MIN
    VT_ASSERT_EQ(res.rows[0][3], "1000");   // MAX
}

VT_TEST(vector_executor_tests, execute_over_segment_file) {
    std::string seg_path = "test_run_vector_exec.vts";
    std::error_code ec;
    std::filesystem::remove(seg_path, ec);

    {
        SegmentWriter writer(1, 150);
        for (usize i = 0; i < 150; ++i) {
            CanonicalEvent ev{};
            ev.sequence = i + 1;
            ev.exchange_ts_ns = 2'000'000'000ULL + i * 500;
            ev.receive_ts_ns = ev.exchange_ts_ns + 10;
            ev.instrument_id = (i < 50) ? 100 : 200;
            ev.event_type = EventType::Trade;
            ev.side = Side::Ask;
            ev.price_ticks = 50000;
            ev.quantity = 25;
            auto ast = writer.add_event(ev);
            VT_ASSERT(ast.ok());
        }
        auto st = writer.write_to_file(seg_path);
        VT_ASSERT(st.ok());
    }

    SegmentReader reader;
    auto st = reader.open(seg_path);
    VT_ASSERT(st.ok());

    query::Parser parser("SELECT COUNT(*), SUM(quantity) WHERE instrument_id = 100");
    auto parse_res = parser.parse_query();
    VT_ASSERT(parse_res.ok());

    VectorExecutor exec;
    auto query_res = exec.execute(reader, parse_res.value().get());
    VT_ASSERT(query_res.ok());

    const auto& res = query_res.value();
    VT_ASSERT_EQ(res.rows_scanned, 150ULL);
    VT_ASSERT_EQ(res.rows_matched, 50ULL);
    VT_ASSERT_EQ(res.rows.size(), 1ULL);
    VT_ASSERT_EQ(res.rows[0][0], "50");   // COUNT
    VT_ASSERT_EQ(res.rows[0][1], "1250"); // SUM(25 * 50)

    reader.close();
    std::filesystem::remove(seg_path, ec);
}
