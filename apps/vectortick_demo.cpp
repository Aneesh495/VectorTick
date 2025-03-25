// VectorTick Demo Application
// Demonstrates end-to-end flow: ingest -> store -> query

#include "vectortick/common/types.hpp"
#include "vectortick/model/event.hpp"
#include "vectortick/storage/segment_writer.hpp"
#include "vectortick/storage/segment_reader.hpp"
#include "vectortick/query/lexer.hpp"
#include "vectortick/query/parser.hpp"
#include "vectortick/ir/builder.hpp"
#include "vectortick/execution/reference_interpreter.hpp"

#include <iostream>
#include <fstream>

using namespace vectortick;

int main() {
    std::cout << "VectorTick Demo Application\n";
    std::cout << "===========================\n\n";
    
    // Create a sample event
    CanonicalEvent event{};
    event.exchange_ts_ns = 1704067200000000000ULL; // 2024-01-01 00:00:00 UTC
    event.receive_ts_ns = 1704067200000000001ULL;
    event.sequence = 1;
    event.instrument_id = 12345;
    event.event_type = EventType::Trade;
    event.side = Side::Bid;
    event.flags = 0;
    event.price_ticks = 10000;
    event.quantity = 100;
    event.venue_id = 1;
    event.source_id = 1;
    event.trade_or_order_id = 99999;
    
    std::cout << "Sample Event:\n";
    std::cout << "  Instrument ID: " << event.instrument_id << "\n";
    std::cout << "  Price Ticks: " << event.price_ticks << "\n";
    std::cout << "  Quantity: " << event.quantity << "\n\n";
    
    // Test query parsing
    std::string query = "SELECT instrument_id, price_ticks, quantity WHERE price_ticks > 5000";
    std::cout << "Query: " << query << "\n\n";
    
    query::Lexer lexer(query);
    
    std::cout << "Tokens: ";
    query::Token tok;
    do {
        tok = lexer.next_token();
        std::cout << query::token_type_name(tok.type) << " ";
    } while (tok.type != query::TokenType::Eof && tok.type != query::TokenType::Error);
    std::cout << "\n\n";
    
    // Parse with a new parser
    query::Parser parser(query);
    auto ast_result = parser.parse_query();
    
    if (ast_result.ok()) {
        auto& ast = ast_result.value();
        
        // Build IR
        ir::Builder builder;
        auto func = builder.build_from_query(ast.get());
        
        if (func) {
            std::cout << "IR built successfully\n";
            std::cout << "Function has " << func->num_blocks() << " blocks\n";
            std::cout << "Function has " << func->num_values() << " values\n\n";
            
            // Execute with reference interpreter
            ReferenceInterpreter interp;
            auto result = interp.execute(func.get(), &event);
            
            if (result.ok()) {
                std::cout << "Execution result: " << result.value() << "\n";
            } else {
                std::cout << "Execution error: " << result.status().message() << "\n";
            }
        } else {
            std::cout << "Failed to build IR\n";
        }
    } else {
        std::cout << "Failed to parse query: " << ast_result.status().message() << "\n";
    }
    
    std::cout << "\nDemo completed successfully!\n";
    return 0;
}
