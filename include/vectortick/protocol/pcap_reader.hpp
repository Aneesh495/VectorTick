#pragma once

#include "../common/types.hpp"
#include "../common/endian.hpp"
#include "../common/status.hpp"
#include "../common/result.hpp"
#include "../memory/mapped_file.hpp"
#include <string>

namespace vectortick {

// Classic PCAP file reader (without linking libpcap)
// Supports:
// - Little-endian and big-endian headers
// - Microsecond and nanosecond timestamp variants
// - Ethernet II
// - 802.1Q VLAN tags (0, 1, or 2)
// - IPv4 with variable IHL
// - UDP
// - VTP1 frames

namespace pcap {

// PCAP global header (24 bytes)
struct GlobalHeader {
    u32 magic_number;
    u16 version_major;
    u16 version_minor;
    i32 thiszone;      // GMT to local correction
    u32 sigfigs;        // Accuracy of timestamps
    u32 snaplen;        // Max length of captured packets
    u32 network;        // Data link type
    
    static constexpr usize Size = 24;
    static constexpr u32 MagicLE = 0xA1B2C3D4;
    static constexpr u32 MagicLE_NS = 0xA1B23C4D;
    static constexpr u32 MagicBE = 0xD4C3B2A1;
    static constexpr u32 MagicBE_NS = 0x4D3CB2A1;
};

// PCAP packet header (16 bytes)
struct PacketHeader {
    u32 ts_sec;
    u32 ts_usec;    // Microseconds or nanoseconds depending on magic
    u32 incl_len;   // Captured length
    u32 orig_len;   // Original length
    
    static constexpr usize Size = 16;
};

// Link types
enum class LinkType : u32 {
    Null = 0,
    Ethernet = 1,
    Loopback = 108,
    LinuxCooked = 113
};

// Ethernet constants
struct Ethernet {
    static constexpr usize HeaderSize = 14;
    static constexpr u16 EtherTypeIPv4 = 0x0800;
    static constexpr u16 EtherTypeVLAN = 0x8100;
    static constexpr u16 EtherTypeVLANOuter = 0x88A8;
};

// IPv4 header
struct IPv4Header {
    u8 version_ihl;     // Version (4 bits) + IHL (4 bits)
    u8 tos;
    u16 total_length;
    u16 identification;
    u16 flags_fragment;  // Flags (3 bits) + Fragment offset (13 bits)
    u8 ttl;
    u8 protocol;
    u16 checksum;
    u32 src_addr;
    u32 dst_addr;
    
    static constexpr usize MinSize = 20;
    static constexpr u8 ProtocolUDP = 17;
    static constexpr u16 FlagDF = 0x4000;
    static constexpr u16 FlagMF = 0x2000;
    static constexpr u16 FragmentMask = 0x1FFF;
    
    [[nodiscard]] u8 ihl() const noexcept { return (version_ihl & 0x0F) * 4; }
    [[nodiscard]] u8 version() const noexcept { return (version_ihl >> 4) & 0x0F; }
    [[nodiscard]] bool is_fragmented() const noexcept { 
        return (flags_fragment & FlagMF) != 0 || (flags_fragment & FragmentMask) != 0;
    }
};

// UDP header
struct UDPHeader {
    u16 src_port;
    u16 dst_port;
    u16 length;
    u16 checksum;
    
    static constexpr usize Size = 8;
};

// PCAP reader state
class PcapReader {
public:
    PcapReader() = default;
    explicit PcapReader(const std::string& path) { (void)open(path); }
    
    // Open PCAP file
    [[nodiscard]] Status open(const std::string& path) noexcept;
    
    // Close file
    void close() noexcept { mapping_.close(); }
    
    // Read next packet
    // Returns bytes consumed, 0 on EOF, or error
    [[nodiscard]] Result<usize> read_next() noexcept;
    
    // Get current packet info
    [[nodiscard]] u64 packet_timestamp_ns() const noexcept { return packet_ts_ns_; }
    [[nodiscard]] usize packet_size() const noexcept { return packet_size_; }
    [[nodiscard]] const byte* packet_data() const noexcept { return packet_data_; }
    
    // Get extracted VTP1 payload (if any)
    [[nodiscard]] const byte* vtp1_payload() const noexcept { return vtp1_payload_; }
    [[nodiscard]] usize vtp1_payload_size() const noexcept { return vtp1_payload_size_; }
    
    // Check if file is open
    [[nodiscard]] bool is_open() const noexcept { return mapping_.is_open(); }
    
    // Get statistics
    [[nodiscard]] u64 packets_read() const noexcept { return packets_read_; }
    [[nodiscard]] u64 vtp1_frames_found() const noexcept { return vtp1_frames_found_; }
    [[nodiscard]] u64 parse_errors() const noexcept { return parse_errors_; }
    
    // Reset to beginning
    void reset() noexcept {
        offset_ = GlobalHeader::Size;
        packets_read_ = 0;
        vtp1_frames_found_ = 0;
        parse_errors_ = 0;
    }

private:
    [[nodiscard]] Status parse_global_header() noexcept;
    [[nodiscard]] Status parse_packet() noexcept;
    [[nodiscard]] Status parse_ethernet(const byte* data, usize size, usize& offset) noexcept;
    [[nodiscard]] Status parse_ipv4(const byte* data, usize size, usize& offset) noexcept;
    [[nodiscard]] Status parse_udp(const byte* data, usize size, usize& offset) noexcept;
    [[nodiscard]] Status parse_vtp1(const byte* data, usize size) noexcept;
    
    MappedFile mapping_;
    usize offset_ = 0;
    
    // File info
    bool is_big_endian_ = false;
    bool is_nanosecond_ts_ = false;
    u32 snaplen_ = 0;
    LinkType link_type_ = LinkType::Ethernet;
    
    // Current packet info
    u64 packet_ts_ns_ = 0;
    usize packet_size_ = 0;
    const byte* packet_data_ = nullptr;
    
    // VTP1 payload
    const byte* vtp1_payload_ = nullptr;
    usize vtp1_payload_size_ = 0;
    
    // Statistics
    u64 packets_read_ = 0;
    u64 vtp1_frames_found_ = 0;
    u64 parse_errors_ = 0;
};

} // namespace pcap

} // namespace vectortick
