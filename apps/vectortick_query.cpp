// VectorTick Query Application
// Executes queries against stored segment files

#include "vectortick/common/types.hpp"
#include "vectortick/storage/segment_reader.hpp"
#include "vectortick/query/lexer.hpp"
#include "vectortick/query/parser.hpp"
#include "vectortick/ir/builder.hpp"
#include "vectortick/execution/reference_interpreter.hpp"

#include <iostream>
#include <fstream>
#include <cstring>

using namespace vectortick;

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options] <segment.vts> <query>\n";
    std::cerr << "Options:\n";
    std::cerr << "  -l <limit>        Limit number of results\n";
    std::cerr << "  -v                Verbose output\n";
    std::cerr << "  -h                Show this help\n";
}

int main(int argc, char* argv[]) {
    std::string segment_file;
    std::string query_str;
    bool verbose = false;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            (void)std::stoull(argv[++i]);  // limit unused for now
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            if (segment_file.empty()) {
                segment_file = argv[i];
            } else {
                query_str = argv[i];
            }
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    
    if (segment_file.empty() || query_str.empty()) {
        std::cerr << "Error: Segment file and query required\n";
        print_usage(argv[0]);
        return 1;
    }
    
    if (verbose) {
        std::cout << "Segment: " << segment_file << "\n";
        std::cout << "Query: " << query_str << "\n";
    }
    
    // Parse query
    query::Parser parser(query_str);
    auto ast_result = parser.parse_query();
    
    if (!ast_result.ok()) {
        std::cerr << "Error: Failed to parse query: " << ast_result.status().message() << "\n";
        return 1;
    }
    
    if (verbose) {
        std::cout << "Query parsed successfully\n";
    }
    
    // Build IR
    ir::Builder builder;
    auto func = builder.build_from_query(ast_result.value().get());
    
    if (!func) {
        std::cerr << "Error: Failed to build IR\n";
        return 1;
    }
    
    if (verbose) {
        std::cout << "IR built: " << func->num_blocks() << " blocks, " 
                  << func->num_values() << " values\n";
    }
    
    // Execute query
    std::cout << "Query execution not yet fully implemented\n";
    std::cout << "Query parsed and IR built successfully\n";
    
    return 0;
}
