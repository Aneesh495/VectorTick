#pragma once

#include "spsc_ring.hpp"
#include "../common/types.hpp"
#include "../common/status.hpp"
#include <thread>
#include <vector>
#include <functional>
#include <atomic>

namespace vectortick {

// Worker pool for parallel query execution
// Fixed number of workers, each with its own command/result queues
class WorkerPool {
public:
    using WorkFunction = std::function<void(u32 worker_id, WorkerCommand cmd, WorkerResult& result)>;
    
    explicit WorkerPool(u32 num_workers = 0, usize queue_capacity = 256)
        : num_workers_(num_workers)
        , running_(false) {
        if (num_workers == 0) {
            num_workers_ = static_cast<u32>(std::thread::hardware_concurrency());
            if (num_workers_ == 0) num_workers_ = 1;
        }
        
        // Create queues for each worker
        for (u32 i = 0; i < num_workers_; ++i) {
            command_queues_.push_back(std::make_unique<CommandQueue>(queue_capacity));
            result_queues_.push_back(std::make_unique<ResultQueue>(queue_capacity));
        }
    }
    
    ~WorkerPool() {
        stop();
    }
    
    // Non-copyable, non-movable
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;
    WorkerPool(WorkerPool&&) = delete;
    WorkerPool& operator=(WorkerPool&&) = delete;
    
    // Start workers with the given work function
    void start(WorkFunction work_fn) {
        if (running_.load()) return;
        
        running_.store(true);
        work_function_ = std::move(work_fn);
        
        for (u32 i = 0; i < num_workers_; ++i) {
            workers_.emplace_back([this, i] {
                worker_loop(i);
            });
        }
    }
    
    // Stop all workers
    void stop() noexcept {
        if (!running_.load()) return;
        
        running_.store(false);
        
        // Send stop command to each worker
        WorkerCommand stop_cmd{WorkerCommand::Stop, 0, 0, 0};
        for (u32 i = 0; i < num_workers_; ++i) {
            while (!command_queues_[i]->try_push(stop_cmd)) {
                std::this_thread::yield();
            }
        }
        
        // Wait for workers to finish
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }
    
    // Send command to a specific worker
    [[nodiscard]] bool send_command(u32 worker_id, const WorkerCommand& cmd) noexcept {
        if (worker_id >= num_workers_) return false;
        return command_queues_[worker_id]->try_push(cmd);
    }
    
    // Send command with blocking (wait until queue has space)
    void send_command_blocking(u32 worker_id, const WorkerCommand& cmd) noexcept {
        if (worker_id >= num_workers_) return;
        while (!command_queues_[worker_id]->try_push(cmd)) {
            std::this_thread::yield();
        }
    }
    
    // Try to receive result from a specific worker
    [[nodiscard]] bool try_receive(u32 worker_id, WorkerResult& result) noexcept {
        if (worker_id >= num_workers_) return false;
        return result_queues_[worker_id]->try_pop(result);
    }
    
    // Receive result with blocking
    void receive_blocking(u32 worker_id, WorkerResult& result) noexcept {
        if (worker_id >= num_workers_) return;
        while (!result_queues_[worker_id]->try_pop(result)) {
            std::this_thread::yield();
        }
    }
    
    // Broadcast same command to all workers
    void broadcast(const WorkerCommand& cmd) noexcept {
        for (u32 i = 0; i < num_workers_; ++i) {
            send_command_blocking(i, cmd);
        }
    }
    
    // Wait for all workers to complete their current work
    void wait_all() noexcept {
        // Wait until all command queues are empty
        for (u32 i = 0; i < num_workers_; ++i) {
            while (!command_queues_[i]->empty()) {
                std::this_thread::yield();
            }
        }
        
        // Wait for all results to be consumed
        for (u32 i = 0; i < num_workers_; ++i) {
            while (!result_queues_[i]->empty()) {
                std::this_thread::yield();
            }
        }
    }
    
    // Get number of workers
    [[nodiscard]] u32 num_workers() const noexcept { return num_workers_; }
    
    // Check if running
    [[nodiscard]] bool is_running() const noexcept { return running_.load(); }

private:
    void worker_loop(u32 worker_id) {
        WorkerCommand cmd;
        WorkerResult result;
        
        while (running_.load()) {
            if (command_queues_[worker_id]->try_pop(cmd)) {
                if (cmd.type == WorkerCommand::Stop) {
                    break;
                }
                
                result.status = WorkerResult::Success;
                result.worker_id = worker_id;
                result.rows_processed = 0;
                result.result_hash = 0;
                
                // Execute work function
                if (work_function_) {
                    work_function_(worker_id, cmd, result);
                }
                
                // Send result
                while (!result_queues_[worker_id]->try_push(result)) {
                    if (!running_.load()) return;
                    std::this_thread::yield();
                }
            } else {
                std::this_thread::yield();
            }
        }
    }
    
    u32 num_workers_;
    std::atomic<bool> running_;
    WorkFunction work_function_;
    
    std::vector<std::unique_ptr<CommandQueue>> command_queues_;
    std::vector<std::unique_ptr<ResultQueue>> result_queues_;
    std::vector<std::thread> workers_;
};

// Cache-line padded counter for per-worker statistics
struct alignas(64) PaddedCounter {
    std::atomic<u64> value{0};
    char padding[64 - sizeof(std::atomic<u64>)];
};

// Per-worker scratch space
struct WorkerScratch {
    u32 worker_id;
    usize batch_size;
    std::vector<u8> decode_buffer;
    std::vector<u64> selection_vector;
    std::vector<u64> aggregate_table;
    PaddedCounter rows_processed;
    PaddedCounter bytes_read;
};

} // namespace vectortick
