#include "../test_framework.hpp"
#include "vectortick/execution/reference_interpreter.hpp"
#include "vectortick/ir/builder.hpp"
#include "vectortick/model/event.hpp"

using namespace vectortick;
using namespace vectortick::ir;

VT_TEST(reference_executor_tests, scalar_arithmetic_100_plus_200) {
    Builder builder;
    Function* fn = builder.create_function("add_100_200");
    ValueId c1 = builder.create_const_u64(100);
    ValueId c2 = builder.create_const_u64(200);
    ValueId sum = builder.create_add(c1, c2, Type::U64);
    builder.create_return(sum);
    
    CanonicalEvent evt = event::make_empty_event();
    ReferenceInterpreter interp;
    auto res = interp.execute(fn, &evt);
    VT_ASSERT(res.ok());
    VT_ASSERT_EQ(res.value(), 300ULL);
}

VT_TEST(reference_executor_tests, signed_price_comparison) {
    Builder builder;
    Function* fn = builder.create_function("check_price");
    ValueId param = fn->create_value(Type::U64, "row");
    fn->add_parameter(param);
    ValueId price = builder.create_load_column(7, param, Type::I64); // col 7 = price_ticks
    ValueId threshold = builder.create_const_i64(-500);
    ValueId cond = builder.create_gt(price, threshold, Type::I64);
    builder.create_return(cond);
    
    ReferenceInterpreter interp;
    
    // -100 > -500 is true
    CanonicalEvent evt1 = event::make_empty_event();
    evt1.price_ticks = -100;
    auto res1 = interp.execute(fn, &evt1);
    VT_ASSERT(res1.ok());
    VT_ASSERT_EQ(res1.value(), 1ULL);
    
    // -1000 > -500 is false
    CanonicalEvent evt2 = event::make_empty_event();
    evt2.price_ticks = -1000;
    auto res2 = interp.execute(fn, &evt2);
    VT_ASSERT(res2.ok());
    VT_ASSERT_EQ(res2.value(), 0ULL);
}

VT_TEST(reference_executor_tests, branch_and_jump_execution) {
    Builder builder;
    Function* fn = builder.create_function("branch_exec");
    BasicBlock* entry = fn->entry_block();
    BasicBlock* then_b = builder.create_block("then");
    BasicBlock* else_b = builder.create_block("else");
    BasicBlock* merge_b = builder.create_block("merge");
    
    builder.set_insertion_point(entry);
    ValueId cond = builder.create_const_bool(false);
    builder.create_branch(cond, then_b, else_b);
    
    builder.set_insertion_point(then_b);
    builder.create_jump(merge_b);
    
    builder.set_insertion_point(else_b);
    builder.create_jump(merge_b);
    
    builder.set_insertion_point(merge_b);
    ValueId ret_val = builder.create_const_u64(999);
    builder.create_return(ret_val);
    
    CanonicalEvent evt = event::make_empty_event();
    ReferenceInterpreter interp;
    auto res = interp.execute(fn, &evt);
    VT_ASSERT(res.ok());
    VT_ASSERT_EQ(res.value(), 999ULL);
}

VT_TEST(reference_executor_tests, error_on_missing_ssa_value) {
    Builder builder;
    Function* fn = builder.create_function("missing_ssa");
    // Return an undefined ValueId 9999
    builder.create_return(9999);
    
    CanonicalEvent evt = event::make_empty_event();
    ReferenceInterpreter interp;
    auto res = interp.execute(fn, &evt);
    VT_ASSERT(!res.ok());
    VT_ASSERT(res.status().code() == StatusCode::InternalError);
}

VT_TEST(reference_executor_tests, cross_row_aggregates) {
    std::vector<CanonicalEvent> events(5);
    for (size_t i = 0; i < 5; ++i) {
        events[i] = event::make_empty_event();
        events[i].quantity = static_cast<u32>((i + 1) * 10); // 10, 20, 30, 40, 50
    }
    
    // SUM(quantity)
    Builder builder;
    Function* sum_fn = builder.create_function("sum_qty");
    ValueId param = sum_fn->create_value(Type::U64, "row");
    sum_fn->add_parameter(param);
    ValueId qty = builder.create_load_column(8, param, Type::U32); // col 8 = quantity
    ValueId sum = builder.create_sum(qty, Type::U64);
    builder.create_return(sum);
    
    ReferenceInterpreter interp;
    auto res = interp.execute_aggregate(sum_fn, events);
    VT_ASSERT(res.ok());
    VT_ASSERT_EQ(res.value(), 150ULL); // 10+20+30+40+50 = 150
}
