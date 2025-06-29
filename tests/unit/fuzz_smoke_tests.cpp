#include "../test_framework.hpp"

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <random>

// Forward declare LLVMFuzzerTestOneInput functions from fuzz targets
// We can test them directly by linking or declaring
extern "C" int FuzzQueryParser(const uint8_t* data, size_t size);
extern "C" int FuzzVtp1Decoder(const uint8_t* data, size_t size);
extern "C" int FuzzSegmentReader(const uint8_t* data, size_t size);

// Implementation of the fuzz tests calling the fuzzer entry points
#include "vectortick/query/lexer.hpp"
#include "vectortick/query/parser.hpp"
#include "vectortick/query/type_checker.hpp"
#include "vectortick/protocol/decoder.hpp"
#include "vectortick/storage/segment_reader.hpp"
#include <unistd.h>

using namespace vectortick;
using namespace vectortick::test;

namespace {

int run_fuzz_query_parser(const uint8_t* data, size_t size) {
    if (size == 0 || size > 65536) return 0;
    std::string q(reinterpret_cast<const char*>(data), size);
    query::Parser parser(q);
    auto ast = parser.parse_query();
    if (!ast.ok()) return 0;
    query::TypeChecker checker;
    (void)checker.check_query(ast.value().get());
    return 0;
}

int run_fuzz_vtp1_decoder(const uint8_t* data, size_t size) {
    if (size == 0 || size > 65536) return 0;
    vtp1::Decoder decoder;
    CanonicalEvent event{};
    size_t offset = 0;
    while (offset < size) {
        auto res = decoder.decode_frame(
            reinterpret_cast<const byte*>(data + offset),
            size - offset,
            event
        );
        if (!res.ok() || res.value() == 0) break;
        offset += res.value();
    }
    return 0;
}

int run_fuzz_segment_reader(const uint8_t* data, size_t size) {
    if (size == 0 || size > 1024 * 1024) return 0;
    char tmp_path[] = "/tmp/vt_smoke_seg_XXXXXX";
    int fd = mkstemp(tmp_path);
    if (fd < 0) return 0;
    ssize_t w = write(fd, data, size);
    close(fd);
    if (w > 0 && static_cast<size_t>(w) == size) {
        SegmentReader reader;
        if (reader.open(tmp_path).ok()) {
            (void)reader.validate();
            if (reader.row_count() > 0 && reader.row_count() < 500) {
                CanonicalEvent ev{};
                (void)reader.read_event(0, ev);
            }
            reader.close();
        }
    }
    unlink(tmp_path);
    return 0;
}

} // namespace

VT_TEST(fuzz_smoke_tests, query_parser_fuzz_seeds_and_mutations) {
    std::vector<std::string> seeds = {
        "",
        "SELECT",
        "SELECT *",
        "SELECT * FROM",
        "FROM events SELECT sequence",
        "SELECT COUNT(*), SUM(quantity), AVG(price_ticks) WHERE price_ticks > 100",
        "SELECT instrument_id, MAX(price_ticks) GROUP BY instrument_id ORDER BY instrument_id DESC LIMIT 10",
        "WHERE",
        "GROUP BY",
        "ORDER BY",
        "LIMIT -1",
        "LIMIT 99999999999999999999999999999",
        "SELECT (((((((((((a + b) * c) - d) / e) % f) & g) | h) ^ i) == j))",
        "SELECT * WHERE price_ticks > 'not an int'",
        "SELECT unknown_column FROM foo",
        "SELECT '\0' \x80\xFF\xFE",
        "SELECT 123456789012345678901234567890"
    };

    for (const auto& s : seeds) {
        run_fuzz_query_parser(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }

    // Pseudo-random bit-flip mutations
    std::mt19937 rng(42);
    for (int iter = 0; iter < 100; ++iter) {
        std::string mutated = "SELECT instrument_id, price_ticks WHERE quantity > 50";
        size_t mutate_ops = 1 + (rng() % 5);
        for (size_t m = 0; m < mutate_ops; ++m) {
            size_t pos = rng() % mutated.size();
            mutated[pos] = static_cast<char>(rng() % 256);
        }
        run_fuzz_query_parser(reinterpret_cast<const uint8_t*>(mutated.data()), mutated.size());
    }
}

VT_TEST(fuzz_smoke_tests, vtp1_decoder_fuzz_seeds_and_corruptions) {
    // Empty, truncated, random buffers
    std::vector<uint8_t> empty;
    run_fuzz_vtp1_decoder(empty.data(), empty.size());

    // Magic bytes only
    uint8_t magic_only[4] = {'V', 'T', 'P', '1'};
    run_fuzz_vtp1_decoder(magic_only, sizeof(magic_only));

    // Full synthetic noise
    std::mt19937 rng(1337);
    for (int iter = 0; iter < 50; ++iter) {
        size_t sz = 1 + (rng() % 256);
        std::vector<uint8_t> buf(sz);
        for (size_t i = 0; i < sz; ++i) {
            buf[i] = static_cast<uint8_t>(rng() % 256);
        }
        run_fuzz_vtp1_decoder(buf.data(), buf.size());
    }
}

VT_TEST(fuzz_smoke_tests, segment_reader_fuzz_corruptions) {
    // 0 bytes
    std::vector<uint8_t> empty;
    run_fuzz_segment_reader(empty.data(), empty.size());

    // Partial header
    uint8_t partial[16] = {'V', 'T', 'S', '1', 1, 0, 0, 0};
    run_fuzz_segment_reader(partial, sizeof(partial));

    // Synthetic header with invalid descriptor offsets
    std::vector<uint8_t> bad_offsets(256, 0);
    bad_offsets[0] = 'V'; bad_offsets[1] = 'T'; bad_offsets[2] = 'S'; bad_offsets[3] = '1';
    bad_offsets[4] = 1; // version 1
    // row count = 100
    bad_offsets[8] = 100;
    // descriptors offset = 0xFFFFFF00
    bad_offsets[48] = 0x00; bad_offsets[49] = 0xFF; bad_offsets[50] = 0xFF; bad_offsets[51] = 0xFF;
    run_fuzz_segment_reader(bad_offsets.data(), bad_offsets.size());
}
