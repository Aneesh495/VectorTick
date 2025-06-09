#include "../test_framework.hpp"
#include "vectortick/jit/jit_compiler.hpp"
#include "vectortick/execution/reference_interpreter.hpp"
#include "vectortick/execution/vector_executor.hpp"
#include "vectortick/ir/builder.hpp"
#include "vectortick/query/parser.hpp"

using namespace vectortick;
using namespace vectortick::test;

VT_TEST(jit_differential_tests, host_jit_arithmetic_100_plus_200) {
    ir::Builder builder;
    ir::Function* func = builder.create_function("add_const");
    VT_ASSERT(func != nullptr);

    ir::ValueId c1 = builder.create_const_u64(100);
    ir::ValueId c2 = builder.create_const_u64(200);
    ir::ValueId sum = builder.create_add(c1, c2, ir::Type::U64);
    builder.create_return(sum);

    // 1. Reference interpreter
    CanonicalEvent evt = event::make_empty_event();
    ReferenceInterpreter interp;
    auto interp_res = interp.execute(func, &evt);
    VT_ASSERT(interp_res.ok());
    VT_ASSERT_EQ(interp_res.value(), 300ULL);

    // 2. JIT compiler on Host architecture
    auto jit_res = jit::jit_execute(func, &evt);
    VT_ASSERT(jit_res.ok());
    VT_ASSERT_EQ(jit_res.value(), 300ULL);

    // 3. Differential equivalence
    VT_ASSERT_EQ(jit_res.value(), interp_res.value());
}

VT_TEST(jit_differential_tests, host_jit_event_load_and_comparison) {
    CanonicalEvent ev{};
    ev.instrument_id = 777;
    ev.price_ticks = 45000;
    ev.quantity = 150;

    ir::Builder builder;
    ir::Function* func = builder.create_function("check_price");
    VT_ASSERT(func != nullptr);

    // Param 0 is row/context
    func->add_parameter(func->create_value(ir::Type::U64, "ctx"));

    // Load price_ticks (column 7, I64)
    ir::ValueId price_val = builder.create_load_column(7, func->parameters()[0], ir::Type::I64);
    ir::ValueId thresh = builder.create_const_i64(40000);
    ir::ValueId cmp = builder.create_gt(price_val, thresh, ir::Type::I64);
    builder.create_return(cmp);

    // 1. Reference interpreter
    ReferenceInterpreter interp;
    auto interp_res = interp.execute(func, &ev);
    VT_ASSERT(interp_res.ok());
    VT_ASSERT_EQ(interp_res.value(), 1ULL);

    // 2. JIT execution
    auto jit_res = jit::jit_execute(func, &ev);
    VT_ASSERT(jit_res.ok());
    VT_ASSERT_EQ(jit_res.value(), 1ULL);

    // 3. Differential equivalence
    VT_ASSERT_EQ(jit_res.value(), interp_res.value());
}

VT_TEST(jit_differential_tests, host_jit_conditional_select) {
    CanonicalEvent ev{};
    ev.quantity = 250;

    ir::Builder builder;
    ir::Function* func = builder.create_function("sel_test");
    VT_ASSERT(func != nullptr);

    func->add_parameter(func->create_value(ir::Type::U64, "ctx"));

    // Load quantity (column 8, U32)
    ir::ValueId qty_val = builder.create_load_column(8, func->parameters()[0], ir::Type::U32);
    ir::ValueId thresh = builder.create_const_u64(200);
    ir::ValueId cond = builder.create_gt(qty_val, thresh, ir::Type::U64);

    ir::ValueId val_true = builder.create_const_u64(999);
    ir::ValueId val_false = builder.create_const_u64(111);
    ir::ValueId sel = builder.create_select(cond, val_true, val_false, ir::Type::U64);
    builder.create_return(sel);

    ReferenceInterpreter interp;
    auto interp_res = interp.execute(func, &ev);
    VT_ASSERT(interp_res.ok());
    VT_ASSERT_EQ(interp_res.value(), 999ULL);

    auto jit_res = jit::jit_execute(func, &ev);
    VT_ASSERT(jit_res.ok());
    VT_ASSERT_EQ(jit_res.value(), 999ULL);

    VT_ASSERT_EQ(jit_res.value(), interp_res.value());
}
