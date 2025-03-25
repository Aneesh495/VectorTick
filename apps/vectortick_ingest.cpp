// VectorTick Ingest Application
// Ingests market data from VTP1 PCAP files

#include "vectortick/common/types.hpp"
#include "vectortick/protocol/pcap_reader.hpp"
#include "vectortick/storage/segment_writer.hpp"

#include <iostream>
#include <fstream>
#include <cstring>

using namespace vectortick;

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options] <input.pcap>\n";
    std::cerr << "Options:\n";
    std::cerr << "  -o <output.vts>   Output segment file\n";
    std::cerr << "  -v                Verbose output\n";
    std::cerr << "  -h                Show this help\n";
}

int main(int argc, char* argv[]) {
    std::string input_file;
    std::string output_file = "output.vts";
    bool verbose = false;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            input_file = argv[i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    
    if (input_file.empty()) {
        std::cerr << "Error: No input file specified\n";
        print_usage(argv[0]);
        return 1;
    }
    
    if (verbose) {
        std::cout << "Input: " << input_file << "\n";
        std::cout << "Output: " << output_file << "\n";
    }
    
    // Open PCAP reader
    pcap::PcapReader reader;
    auto status = reader.open(input_file);
    if (!status.ok()) {
        std::cerr << "Error opening PCAP: " << status.message() << "\n";
        return 1;
    }
    
    if (verbose) {
        std::cout << "PCAP opened successfully\n";
    }
    
    // Process packets
    u64 packet_count = 0;
    u64 byte_count = 0;
    
    while (reader.is_open()) {
        auto result = reader.read_next();
        if (!result.ok()) {
            if (verbose) {
                std::cerr << "Error reading packet: " << result.status().message() << "\n";
            }
            break;
        }
        
        if (result.value() == 0) {
            // EOF
            break;
        }
        
        ++packet_count;
        byte_count += result.value();
        
        if (verbose && packet_count % 10000 == 0) {
            std::cout << "Processed " << packet_count << " packets\n";
        }
    }
    
    std::cout << "Ingestion complete:\n";
    std::cout << "  Packets: " << packet_count << "\n";
    std::cout << "  Bytes: " << byte_count << "\n";
    
    return 0;
}
