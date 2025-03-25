#pragma once

#include "../common/types.hpp"
#include "../common/status.hpp"
#include "../common/result.hpp"
#include "aligned_buffer.hpp"
#include <string>
#include <memory>

namespace vectortick {

// Memory-mapped file wrapper
// Provides read-only or read-write access to file contents via mmap
class MappedFile {
public:
    MappedFile() = default;
    ~MappedFile();
    
    // Non-copyable
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    
    // Movable
    MappedFile(MappedFile&& other) noexcept;
    MappedFile& operator=(MappedFile&& other) noexcept;
    
    // Open file for read-only access
    [[nodiscard]] static Result<MappedFile> open_read(const std::string& path) noexcept;
    
    // Open file for read-write access
    [[nodiscard]] static Result<MappedFile> open_read_write(const std::string& path, 
                                                              usize size) noexcept;
    
    // Close mapping
    void close() noexcept;
    
    // Access
    [[nodiscard]] const byte* data() const noexcept { return data_; }
    [[nodiscard]] byte* data() noexcept { return data_; }
    [[nodiscard]] usize size() const noexcept { return size_; }
    [[nodiscard]] bool is_open() const noexcept { return data_ != nullptr; }
    [[nodiscard]] bool is_writable() const noexcept { return writable_; }
    
    // Sync to disk (write mappings only)
    [[nodiscard]] Status sync() noexcept;
    
    // Advise the kernel about access pattern
    enum class Advice {
        Normal,      // No special advice
        Sequential,  // Access sequentially
        Random,      // Access randomly
        WillNeed,    // Will need soon
        DontNeed     // Won't need soon
    };
    void advise(Advice advice) noexcept;
    
    // Get path
    [[nodiscard]] const std::string& path() const noexcept { return path_; }
    
    // Check if address is within mapping
    [[nodiscard]] bool contains(const void* ptr) const noexcept {
        const byte* p = static_cast<const byte*>(ptr);
        return p >= data_ && p < (data_ + size_);
    }
    
    // Get offset of pointer within file
    [[nodiscard]] usize offset_of(const void* ptr) const noexcept {
        return static_cast<const byte*>(ptr) - data_;
    }

private:
    MappedFile(const std::string& path, byte* data, usize size, bool writable, int fd)
        : path_(path), data_(data), size_(size), writable_(writable), fd_(fd) {}
    
    std::string path_;
    byte* data_ = nullptr;
    usize size_ = 0;
    bool writable_ = false;
    int fd_ = -1;
};

// RAII helper for temporarily mapped region
class ScopedMapping {
public:
    ScopedMapping() = default;
    ScopedMapping(const std::string& path, bool writable = false);
    ~ScopedMapping();
    
    ScopedMapping(ScopedMapping&& other) noexcept;
    ScopedMapping& operator=(ScopedMapping&& other) noexcept;
    
    // Non-copyable
    ScopedMapping(const ScopedMapping&) = delete;
    ScopedMapping& operator=(const ScopedMapping&) = delete;
    
    [[nodiscard]] const byte* data() const noexcept { return mapped_.data(); }
    [[nodiscard]] byte* data() noexcept { return mapped_.data(); }
    [[nodiscard]] usize size() const noexcept { return mapped_.size(); }
    [[nodiscard]] bool is_open() const noexcept { return mapped_.is_open(); }
    [[nodiscard]] operator bool() const noexcept { return is_open(); }

private:
    MappedFile mapped_;
};

// File I/O utilities
namespace file {

// Check if file exists
[[nodiscard]] bool exists(const std::string& path) noexcept;

// Get file size
[[nodiscard]] Result<usize> size(const std::string& path) noexcept;

// Delete file
[[nodiscard]] Status remove(const std::string& path) noexcept;

// Rename file (atomic on most filesystems)
[[nodiscard]] Status rename(const std::string& old_path, 
                           const std::string& new_path) noexcept;

// Create directory (including parents)
[[nodiscard]] Status mkdir(const std::string& path) noexcept;

// Sync directory (for durability on POSIX)
[[nodiscard]] Status sync_dir(const std::string& path) noexcept;

// Read entire file into buffer
[[nodiscard]] Result<AlignedBuffer> read_all(const std::string& path) noexcept;

// Write buffer to file
[[nodiscard]] Status write_all(const std::string& path, 
                               const byte* data, 
                               usize size) noexcept;

// Append to file
[[nodiscard]] Status append(const std::string& path,
                           const byte* data,
                           usize size) noexcept;

// Sync file to disk
[[nodiscard]] Status sync(const std::string& path) noexcept;

// Get temporary directory
[[nodiscard]] std::string temp_dir() noexcept;

// Create temporary file
[[nodiscard]] Result<std::string> temp_file(const std::string& prefix = "vectortick_") noexcept;

} // namespace file

} // namespace vectortick
