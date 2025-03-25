// VectorTick Benchmark Suite
// Performance benchmarks for core operations

#include "vectortick/common/types.hpp"
#include "vectortick/common/crc32c.hpp"
#include "vectortick/model/event.hpp"
#include "vectortick/ir/builder.hpp"
#include "vectortick/execution/reference_interpreter.hpp"

#include <iostream>
#include <chrono>
#include <random>
#include <memory>

using namespace vectortick;

class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    double elapsed_ms() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }
    
    double elapsed_sec() const {
        return elapsed_ms() / 1000.0;
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

void bench_crc32c() {
    const size_t data_size = 1024 * 1024; // 1MB
    const size_t iterations = 100;
    
    std::vector<u8> data(data_size);
    std::mt19937_64 rng(42);
    for (size_t i = 0; i < data_size; ++i) {
        data[i] = static_cast<u8>(rng());
    }
    
    std::cout << "CRC32C Benchmark\n";
    std::cout << "  Data size: " << data_size << " bytes\n";
    std::cout << "  Iterations: " << iterations << "\n";
    
    Timer timer;
    u64 total_bytes = 0;
    u32 result = 0;
    
    for (size_t i = 0; i < iterations; ++i) {
        result = crc32c(data.data(), data_size);
        total_bytes += data_size;
    }
    
    double elapsed = timer.elapsed_sec();
    double throughput_gbps = (total_bytes / elapsed) / (1024.0 * 1024.0 * 1024.0);
    
    std::cout << "  Elapsed: " << elapsed << " sec\n";
    std::cout << "  Throughput: " << throughput_gbps << " GB/s\n";
    std::cout << "  Checksum: " << result << "\n\n";
}

void bench_event_creation() {
    const size_t iterations = 10000000; // 10M events
    
    std::cout << "Event Creation Benchmark\n";
    std::cout << "  Iterations: " << iterations << "\n";
    
    Timer timer;
    std::vector<CanonicalEvent> events(iterations);
    
    for (size_t i = 0; i < iterations; ++i) {
        events[i].exchange_ts_ns = i;
        events[i].instrument_id = static_cast<u32>(i % 1000);
        events[i].price_ticks = static_cast<i64>(i % 100000);
        events[i].quantity = static_cast<u64>(i % 10000);
    }
    
    double elapsed = timer.elapsed_sec();
    double throughput = iterations / elapsed;
    
    std::cout << "  Elapsed: " << elapsed << " sec\n";
    std::cout << "  Throughput: " << (throughput / 1000000.0) << " M events/sec\n\n";
}

void bench_interpreter() {
    const size_t iterations = 1000000; // 1M executions
    
    std::cout << "Reference Interpreter Benchmark\n";
    std::cout << "  Iterations: " << iterations << "\n";
    
    // Create a simple function that adds two values
    ir::Function func("add_test");
    auto* block = func.create_block("entry");
    
    // Create constants
    auto v1 = func.create_value(ir::Type::U64, "c1");
    auto v2 = func.create_value(ir::Type::U64, "c2");
    auto v3 = func.create_value(ir::Type::U64, "result");
    
    block->append(std::make_unique<ir::ConstOp>(ir::Constant::u64_const(100), v1));
    block->append(std::make_unique<ir::ConstOp>(ir::Constant::u64_const(200), v2));
    block->append(std::make_unique<ir::BinaryOp>(ir::Opcode::AddU64, v1, v2, v3, ir::Type::U64));
    block->append(std::make_unique<ir::ReturnOp>(v3));
    
    CanonicalEvent event{};
    ReferenceInterpreter interp;
    
    Timer timer;
    u64 result = 0;
    
    for (size_t i = 0; i < iterations; ++i) {
        auto r = interp.execute(&func, &event);
        if (r.ok()) {
            result = r.value();
        }
    }
    
    double elapsed = timer.elapsed_sec();
    double throughput = iterations / elapsed;
    
    std::cout << "  Elapsed: " << elapsed << " sec\n";
    std::cout << "  Throughput: " << (throughput / 1000000.0) << " M exec/sec\n";
    std::cout << "  Result: " << result << "\n\n";
}

int main() {
    std::cout << "VectorTick Benchmark Suite\n";
    std::cout << "==========================\n\n";
    
    bench_crc32c();
    bench_event_creation();
    bench_interpreter();
    
    std::cout << "Benchmarks complete!\n";
    return 0;
}
