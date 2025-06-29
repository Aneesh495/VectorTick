#include "vectortick/query/parser.hpp"
#include "vectortick/query/type_checker.hpp"

#include <cstdint>
#include <cstddef>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0 || size > 65536) {
        return 0;
    }
    std::string query_str(reinterpret_cast<const char*>(data), size);
    
    vectortick::query::Parser parser(query_str);
    auto ast = parser.parse_query();
    if (!ast.ok()) {
        return 0;
    }
    
    vectortick::query::TypeChecker checker;
    (void)checker.check_query(ast.value().get());
    return 0;
}
