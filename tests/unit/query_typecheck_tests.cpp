#include "../test_framework.hpp"
#include "vectortick/query/parser.hpp"
#include "vectortick/query/type_checker.hpp"

using namespace vectortick;
using namespace vectortick::query;

VT_TEST(query_typecheck_tests, valid_columns_and_types) {
    std::string sql = "SELECT instrument_id, price_ticks + 500 FROM trades WHERE price_ticks > 1000 GROUP BY venue_id ORDER BY instrument_id ASC";
    Parser parser(sql);
    auto res = parser.parse_query();
    VT_ASSERT(res.ok());
    
    TypeChecker tc;
    Status st = tc.check_query(res.value().get());
    VT_ASSERT(st.ok());
}

VT_TEST(query_typecheck_tests, reject_unknown_column) {
    std::string sql = "SELECT non_existent_column FROM quotes";
    Parser parser(sql);
    auto res = parser.parse_query();
    VT_ASSERT(res.ok());
    
    TypeChecker tc;
    Status st = tc.check_query(res.value().get());
    VT_ASSERT(!st.ok());
    VT_ASSERT(st.code() == StatusCode::InvalidArgument);
}

VT_TEST(query_typecheck_tests, reject_unknown_group_by_column) {
    std::string sql = "SELECT instrument_id FROM quotes GROUP BY nonexistent_venue";
    Parser parser(sql);
    auto res = parser.parse_query();
    VT_ASSERT(res.ok());
    
    TypeChecker tc;
    Status st = tc.check_query(res.value().get());
    VT_ASSERT(!st.ok());
    VT_ASSERT(st.code() == StatusCode::InvalidArgument);
}
