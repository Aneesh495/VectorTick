#include "../test_framework.hpp"
#include "vectortick/query/parser.hpp"

using namespace vectortick;
using namespace vectortick::query;

VT_TEST(query_parser_tests, parse_select_with_projections_and_where) {
    std::string sql = "SELECT instrument_id, price_ticks, quantity FROM quotes WHERE price_ticks > 5000";
    Parser parser(sql);
    auto res = parser.parse_query();
    VT_ASSERT(res.ok());
    
    auto& q = res.value();
    VT_ASSERT(q->table_name == "quotes");
    VT_ASSERT_EQ(q->projections.size(), 3ULL);
    VT_ASSERT(q->where_expr != nullptr);
    VT_ASSERT_EQ(static_cast<int>(q->where_expr->type), static_cast<int>(ExprType::BinaryOp));
}

VT_TEST(query_parser_tests, parse_select_star_and_aliases) {
    std::string sql = "SELECT * FROM events";
    Parser parser(sql);
    auto res = parser.parse_query();
    VT_ASSERT(res.ok());
    auto& q = res.value();
    VT_ASSERT_EQ(q->projections.size(), 1ULL);
    VT_ASSERT(q->projections[0].is_wildcard);

    std::string sql_alias = "SELECT price_ticks AS p, quantity q FROM trades";
    Parser parser2(sql_alias);
    auto res2 = parser2.parse_query();
    VT_ASSERT(res2.ok());
    auto& q2 = res2.value();
    VT_ASSERT_EQ(q2->projections.size(), 2ULL);
    VT_ASSERT(q2->projections[0].alias == "p");
    VT_ASSERT(q2->projections[1].alias == "q");
}

VT_TEST(query_parser_tests, parse_group_by_order_by_limit) {
    std::string sql = "SELECT instrument_id, sum(quantity) FROM trades WHERE price_ticks > 100 GROUP BY instrument_id ORDER BY instrument_id DESC LIMIT 50";
    Parser parser(sql);
    auto res = parser.parse_query();
    VT_ASSERT(res.ok());
    
    auto& q = res.value();
    VT_ASSERT(q->table_name == "trades");
    VT_ASSERT_EQ(q->group_by_columns.size(), 1ULL);
    VT_ASSERT(q->group_by_columns[0] == "instrument_id");
    VT_ASSERT_EQ(q->order_by.size(), 1ULL);
    VT_ASSERT(q->order_by[0].first == "instrument_id");
    VT_ASSERT_EQ(q->order_by[0].second, false); // DESC
    VT_ASSERT_EQ(q->limit, 50ULL);
}

VT_TEST(query_parser_tests, parse_legacy_from_syntax) {
    std::string sql = "FROM quotes WHERE price_ticks <= 10000 AGG count(), sum(quantity) LIMIT 10";
    Parser parser(sql);
    auto res = parser.parse_query();
    VT_ASSERT(res.ok());
    
    auto& q = res.value();
    VT_ASSERT(q->table_name == "quotes");
    VT_ASSERT_EQ(q->aggregations.size(), 2ULL);
    VT_ASSERT_EQ(q->limit, 10ULL);
}
