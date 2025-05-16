#include "../test_framework.hpp"
#include "vectortick/storage/catalog.hpp"
#include "vectortick/storage/segment_writer.hpp"
#include "vectortick/replay/replay_engine.hpp"

#include <filesystem>
#include <vector>
#include <fstream>

using namespace vectortick;
using namespace vectortick::test;

namespace {

void create_test_segment(const std::string& path, u64 seg_id, u64 start_seq, usize count) {
    SegmentWriter writer(seg_id, count);
    for (usize i = 0; i < count; ++i) {
        CanonicalEvent ev{};
        ev.sequence = start_seq + i;
        ev.exchange_ts_ns = 1'000'000'000ULL + (start_seq + i) * 1000;
        ev.receive_ts_ns = ev.exchange_ts_ns + 50;
        ev.instrument_id = 100 + static_cast<u32>(i % 5);
        ev.event_type = EventType::Trade;
        ev.side = (i % 2 == 0) ? Side::Bid : Side::Ask;
        ev.price_ticks = 50000 + static_cast<i64>(i * 10);
        ev.quantity = 10 + static_cast<u32>(i % 100);
        ev.venue_id = 1;
        ev.source_id = 1;
        ev.trade_or_order_id = 5000 + i;
        
        auto st = writer.add_event(ev);
        VT_ASSERT(st.ok());
    }
    auto st = writer.write_to_file(path);
    VT_ASSERT(st.ok());
}

} // namespace

VT_TEST(recovery_tests, manifest_serialize_deserialize_and_crc) {
    Manifest m;
    m.set_next_segment_id(10);
    m.set_next_tx_id(20);

    SegmentMeta s1;
    s1.segment_id = 1;
    s1.filename = "seg_1.vts";
    s1.min_seq = 100;
    s1.max_seq = 200;
    s1.row_count = 101;
    s1.active = true;
    m.add_segment(s1);

    SegmentMeta s2;
    s2.segment_id = 2;
    s2.filename = "seg_2.vts";
    s2.min_seq = 201;
    s2.max_seq = 300;
    s2.row_count = 100;
    s2.active = true;
    m.add_segment(s2);

    std::vector<u8> buffer;
    auto st = m.serialize(buffer);
    VT_ASSERT(st.ok());
    VT_ASSERT(!buffer.empty());

    // Deserialize into fresh manifest
    Manifest m2;
    st = m2.deserialize(buffer);
    VT_ASSERT(st.ok());
    VT_ASSERT_EQ(m2.next_segment_id(), 10ULL);
    VT_ASSERT_EQ(m2.next_tx_id(), 20ULL);
    VT_ASSERT_EQ(m2.segments().size(), 2ULL);
    VT_ASSERT_EQ(m2.segments()[0].segment_id, 1ULL);
    VT_ASSERT_EQ(m2.segments()[0].filename, "seg_1.vts");
    VT_ASSERT_EQ(m2.segments()[1].segment_id, 2ULL);

    // Corrupt one byte of data and verify rejection
    buffer[20] ^= 0xFF;
    Manifest m3;
    st = m3.deserialize(buffer);
    VT_ASSERT(!st.ok());
}

VT_TEST(recovery_tests, journal_write_and_recover_torn_write) {
    std::string test_dir = "test_run_journal";
    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);
    std::filesystem::create_directories(test_dir, ec);

    std::string wal_path = test_dir + "/test.wal";

    {
        Journal wal;
        auto st = wal.open(wal_path);
        VT_ASSERT(st.ok());

        st = wal.append_begin(1);
        VT_ASSERT(st.ok());

        SegmentMeta sm;
        sm.segment_id = 1;
        sm.filename = "seg1.vts";
        sm.row_count = 50;
        st = wal.append_add_segment(1, sm);
        VT_ASSERT(st.ok());

        st = wal.append_commit(1);
        VT_ASSERT(st.ok());

        // Tx 2: uncommitted / aborted
        st = wal.append_begin(2);
        VT_ASSERT(st.ok());
        st = wal.append_abort(2);
        VT_ASSERT(st.ok());

        // Tx 3: begins and adds segment, but no commit (simulating crash)
        st = wal.append_begin(3);
        VT_ASSERT(st.ok());
        sm.segment_id = 3;
        sm.filename = "seg3.vts";
        st = wal.append_add_segment(3, sm);
        VT_ASSERT(st.ok());

        wal.close();
    }

    // Now append 5 bytes of garbage to simulate a torn write at crash
    {
        std::ofstream out(wal_path, std::ios::binary | std::ios::app);
        out.write("TORN!", 5);
    }

    // Read back journal
    std::vector<Journal::RecoveredTx> txs;
    auto st = Journal::read_all(wal_path, txs);
    VT_ASSERT(st.ok());

    // Tx 1 was committed
    bool found_tx1 = false;
    bool found_tx2 = false;
    bool found_tx3 = false;

    for (const auto& tx : txs) {
        if (tx.tx_id == 1) {
            found_tx1 = true;
            VT_ASSERT(tx.committed);
            VT_ASSERT_EQ(tx.added_segments.size(), 1ULL);
            VT_ASSERT_EQ(tx.added_segments[0].segment_id, 1ULL);
        } else if (tx.tx_id == 2) {
            found_tx2 = true;
            VT_ASSERT(!tx.committed);
        } else if (tx.tx_id == 3) {
            found_tx3 = true;
            VT_ASSERT(!tx.committed); // Uncommitted!
        }
    }

    VT_ASSERT(found_tx1);
    VT_ASSERT(found_tx2);
    VT_ASSERT(found_tx3);

    std::filesystem::remove_all(test_dir, ec);
}

VT_TEST(recovery_tests, catalog_commit_and_recovery) {
    std::string test_dir = "test_run_catalog";
    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);

    std::string seg1_path;
    {
        Catalog cat(test_dir);
        auto st = cat.open();
        VT_ASSERT(st.ok());

        // Write a temp segment
        std::string tmp_seg = test_dir + "/.tmp_write1.vts";
        create_test_segment(tmp_seg, 1, 1000, 50);

        SegmentMeta meta;
        meta.min_seq = 1000;
        meta.max_seq = 1049;
        meta.min_exchange_ts = 1'000'000'000ULL;
        meta.max_exchange_ts = 1'000'049'000ULL;
        meta.row_count = 50;

        st = cat.commit_segment(tmp_seg, meta, &seg1_path);
        VT_ASSERT(st.ok());
        VT_ASSERT(std::filesystem::exists(seg1_path));
        VT_ASSERT(!std::filesystem::exists(tmp_seg));

        auto active = cat.active_segments();
        VT_ASSERT_EQ(active.size(), 1ULL);
        VT_ASSERT_EQ(active[0].segment_id, 1ULL);
    }

    // Simulate an orphaned temp file from a killed process
    std::string orphan = test_dir + "/.tmp_orphaned.tmp";
    {
        std::ofstream out(orphan);
        out << "orphaned data";
    }
    VT_ASSERT(std::filesystem::exists(orphan));

    // Reopen catalog and verify recovery
    {
        Catalog cat(test_dir);
        auto st = cat.open();
        VT_ASSERT(st.ok());

        // Orphaned file must have been cleaned up
        VT_ASSERT(!std::filesystem::exists(orphan));

        // Committed segment must still be present and active
        auto active = cat.active_segments();
        VT_ASSERT_EQ(active.size(), 1ULL);
        VT_ASSERT_EQ(active[0].segment_id, 1ULL);
        VT_ASSERT_EQ(active[0].row_count, 50ULL);
    }

    std::filesystem::remove_all(test_dir, ec);
}

VT_TEST(recovery_tests, catalog_compaction) {
    std::string test_dir = "test_run_compaction";
    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);

    {
        Catalog cat(test_dir);
        auto st = cat.open();
        VT_ASSERT(st.ok());

        // Commit segment 1 (seq 100 to 149)
        std::string tmp1 = test_dir + "/.tmp1.vts";
        create_test_segment(tmp1, 1, 100, 50);
        SegmentMeta m1;
        m1.row_count = 50;
        st = cat.commit_segment(tmp1, m1);
        VT_ASSERT(st.ok());

        // Commit segment 2 (seq 150 to 199)
        std::string tmp2 = test_dir + "/.tmp2.vts";
        create_test_segment(tmp2, 2, 150, 50);
        SegmentMeta m2;
        m2.row_count = 50;
        st = cat.commit_segment(tmp2, m2);
        VT_ASSERT(st.ok());

        VT_ASSERT_EQ(cat.active_segments().size(), 2ULL);

        // Compact segments 1 and 2
        std::vector<u64> new_ids;
        st = cat.compact_segments({1, 2}, &new_ids);
        VT_ASSERT(st.ok());
        VT_ASSERT_EQ(new_ids.size(), 1ULL);

        auto active = cat.active_segments();
        VT_ASSERT_EQ(active.size(), 1ULL);
        VT_ASSERT_EQ(active[0].segment_id, new_ids[0]);
        VT_ASSERT_EQ(active[0].row_count, 100ULL);
        VT_ASSERT_EQ(active[0].min_seq, 100ULL);
        VT_ASSERT_EQ(active[0].max_seq, 199ULL);
        VT_ASSERT(active[0].is_compacted);

        // Verify old segment files were deleted
        VT_ASSERT(!std::filesystem::exists(test_dir + "/segment_1.vts"));
        VT_ASSERT(!std::filesystem::exists(test_dir + "/segment_2.vts"));
        VT_ASSERT(std::filesystem::exists(test_dir + "/" + active[0].filename));
    }

    // Reopen catalog after compaction and verify persistence
    {
        Catalog cat(test_dir);
        auto st = cat.open();
        VT_ASSERT(st.ok());

        auto active = cat.active_segments();
        VT_ASSERT_EQ(active.size(), 1ULL);
        VT_ASSERT_EQ(active[0].row_count, 100ULL);

        // Replay compacted segment and verify all 100 events
        ReplayEngine replay;
        u64 replayed = 0;
        st = replay.replay_catalog(cat, [&](const CanonicalEvent& ev) {
            replayed++;
            (void)ev;
        });
        VT_ASSERT(st.ok());
        VT_ASSERT_EQ(replayed, 100ULL);
        VT_ASSERT_EQ(replay.stats().sequence_breaks, 0ULL);
        VT_ASSERT_EQ(replay.stats().out_of_order_events, 0ULL);
    }

    std::filesystem::remove_all(test_dir, ec);
}
