// VectorTick Query Application
// Executes queries against stored segment files

#include "vectortick/common/types.hpp"
#include "vectortick/storage/segment_reader.hpp"
#include "vectortick/query/lexer.hpp"
#include "vectortick/query/parser.hpp"
#include "vectortick/ir/builder.hpp"
#include "vectortick/execution/vector_executor.hpp"
#include "vectortick/execution/reference_interpreter.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <chrono>

using namespace vectortick;

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options] <segment.vts> \"<query>\"\n";
    std::cerr << "Options:\n";
    std::cerr << "  -e <engine>       Execution engine (vector, interpreter, default: vector)\n";
    std::cerr << "  -l <limit>        Limit number of results\n";
    std::cerr << "  -v                Verbose output\n";
    std::cerr << "  -h                Show this help\n";
}

int main(int argc, char* argv[]) {
    std::string segment_file;
    std::string query_str;
    std::string engine = "vector";
    u64 limit_override = 0;
    bool verbose = false;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            engine = argv[++i];
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            limit_override = std::stoull(argv[++i]);
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
        std::cout << "Engine:  " << engine << "\n";
        std::cout << "Query:   " << query_str << "\n";
        std::cout << "----------------------------------------\n";
    }
    
    // Parse query
    query::Parser parser(query_str);
    auto ast_result = parser.parse_query();
    
    if (!ast_result.ok()) {
        std::cerr << "Parse Error: " << ast_result.status().message() << "\n";
        return 1;
    }
    
    auto* q_ast = ast_result.value().get();
    if (limit_override > 0) {
        q_ast->limit = limit_override;
    }
    
    // Open segment file
    SegmentReader reader;
    auto open_st = reader.open(segment_file);
    if (!open_st.ok()) {
        std::cerr << "Storage Error: " << open_st.message() << "\n";
        return 1;
    }

    if (engine == "vector") {
        VectorExecutor exec;
        auto res = exec.execute(reader, q_ast);
        if (!res.ok()) {
            std::cerr << "Execution Error: " << res.status().message() << "\n";
            return 1;
        }

        const auto& qres = res.value();

        // Print header
        for (usize i = 0; i < qres.column_names.size(); ++i) {
            std::cout << std::left << std::setw(16) << qres.column_names[i];
        }
        std::cout << "\n";
        for (usize i = 0; i < qres.column_names.size(); ++i) {
            std::cout << "----------------";
        }
        std::cout << "\n";

        // Print rows
        for (const auto& row : qres.rows) {
            for (const auto& cell : row) {
                std::cout << std::left << std::setw(16) << cell;
            }
            std::cout << "\n";
        }

        std::cout << "\n(" << qres.rows.size() << " rows, scanned " 
                  << qres.rows_scanned << " rows in " 
                  << std::fixed << std::setprecision(3) << (qres.execution_time_ns / 1e6) 
                  << " ms)\n";
    } else {
        // Interpreter engine path
        ir::Builder builder;
        auto func = builder.build_from_query(q_ast);
        if (!func) {
            std::cerr << "IR Lowering Error: failed to build IR\n";
            return 1;
        }

        ReferenceInterpreter interp;
        std::vector<CanonicalEvent> events;
        auto r_st = reader.read_all_events(events);
        if (!r_st.ok()) {
            std::cerr << "Read Error: " << r_st.message() << "\n";
            return 1;
        }

        auto start = std::chrono::high_resolution_clock::now();
        u64 matched = 0;
        for (const auto& ev : events) {
            auto ires = interp.execute(func.get(), &ev);
            if (ires.ok() && ires.value() != 0) {
                matched++;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << "Matched rows: " << matched << " / " << events.size() 
                  << " (Interpreter execution time: " << ms << " ms)\n";
    }
    
    return 0;
}
