#pragma once

#include "../common/types.hpp"
#include "../common/status.hpp"
#include "aligned_buffer.hpp"
#include <vector>
#include <mutex>

namespace vectortick {

// Buffer pool for reusable vector batches
// Thread-safe pool of pre-allocated aligned buffers
class BufferPool {
public:
    explicit BufferPool(usize buffer_size = SegmentConfig::BatchSize * sizeof(u64), 
                        usize initial_count = 4)
        : buffer_size_(buffer_size) {
        for (usize i = 0; i < initial_count; ++i) {
            auto buf = std::make_unique<AlignedBuffer>();
            if (buf->allocate(buffer_size).ok()) {
                buffers_.push_back(std::move(buf));
            }
        }
    }
    
    // Acquire a buffer from the pool
    [[nodiscard]] std::unique_ptr<AlignedBuffer> acquire() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (buffers_.empty()) {
            // Create new buffer
            auto buf = std::make_unique<AlignedBuffer>();
            if (!buf->allocate(buffer_size_).ok()) {
                return nullptr;
            }
            return buf;
        }
        
        auto buf = std::move(buffers_.back());
        buffers_.pop_back();
        return buf;
    }
    
    // Return a buffer to the pool
    void release(std::unique_ptr<AlignedBuffer> buffer) noexcept {
        if (!buffer) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Don't grow unbounded
        if (buffers_.size() < max_pool_size_) {
            buffers_.push_back(std::move(buffer));
        }
        // Otherwise, let it be destroyed
    }
    
    // Get buffer size
    [[nodiscard]] usize buffer_size() const noexcept { return buffer_size_; }
    
    // Get pool size
    [[nodiscard]] usize pool_size() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffers_.size();
    }
    
    // Clear all buffers
    void clear() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        buffers_.clear();
    }
    
    // Set max pool size
    void set_max_pool_size(usize size) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        max_pool_size_ = size;
    }

private:
    usize buffer_size_;
    usize max_pool_size_ = 64;  // Maximum buffers to keep
    std::vector<std::unique_ptr<AlignedBuffer>> buffers_;
    mutable std::mutex mutex_;
};

// RAII wrapper for borrowed buffer
class BorrowedBuffer {
public:
    BorrowedBuffer(BufferPool& pool, std::unique_ptr<AlignedBuffer> buffer)
        : pool_(&pool), buffer_(std::move(buffer)) {}
    
    BorrowedBuffer(BorrowedBuffer&& other) noexcept
        : pool_(other.pool_), buffer_(std::move(other.buffer_)) {
        other.pool_ = nullptr;
    }
    
    BorrowedBuffer& operator=(BorrowedBuffer&& other) noexcept {
        if (this != &other) {
            release();
            pool_ = other.pool_;
            buffer_ = std::move(other.buffer_);
            other.pool_ = nullptr;
        }
        return *this;
    }
    
    ~BorrowedBuffer() {
        release();
    }
    
    // Non-copyable
    BorrowedBuffer(const BorrowedBuffer&) = delete;
    BorrowedBuffer& operator=(const BorrowedBuffer&) = delete;
    
    [[nodiscard]] byte* data() noexcept { return buffer_ ? buffer_->data() : nullptr; }
    [[nodiscard]] const byte* data() const noexcept { return buffer_ ? buffer_->data() : nullptr; }
    [[nodiscard]] usize size() const noexcept { return buffer_ ? buffer_->size() : 0; }
    
    [[nodiscard]] AlignedBuffer* get() noexcept { return buffer_.get(); }
    [[nodiscard]] const AlignedBuffer* get() const noexcept { return buffer_.get(); }
    
    [[nodiscard]] explicit operator bool() const noexcept { 
        return buffer_ && buffer_->data(); 
    }

private:
    void release() noexcept {
        if (pool_ && buffer_) {
            pool_->release(std::move(buffer_));
        }
    }
    
    BufferPool* pool_;
    std::unique_ptr<AlignedBuffer> buffer_;
};

// Helper to acquire a borrowed buffer
[[nodiscard]] inline BorrowedBuffer borrow_buffer(BufferPool& pool) noexcept {
    return BorrowedBuffer(pool, pool.acquire());
}

} // namespace vectortick
