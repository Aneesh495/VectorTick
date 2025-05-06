#include "../test_framework.hpp"
#include "vectortick/ir/builder.hpp"
#include "vectortick/query/parser.hpp"

using namespace vectortick;
using namespace vectortick::ir;

VT_TEST(ir_tests, build_function_constants_and_arithmetic) {
    Builder builder;
    Function* fn = builder.create_function("test_add");
    VT_ASSERT(fn != nullptr);
    VT_ASSERT_EQ(fn->name(), "test_add");
    
    ValueId c1 = builder.create_const_u64(42);
    ValueId c2 = builder.create_const_u64(58);
    ValueId sum = builder.create_add(c1, c2, Type::U64);
    builder.create_return(sum);
    
    VT_ASSERT_EQ(fn->num_blocks(), 1ULL);
    VT_ASSERT_EQ(fn->entry_block()->num_instructions(), 4ULL);
    VT_ASSERT_EQ(static_cast<int>(fn->entry_block()->instruction(2)->opcode()), static_cast<int>(Opcode::AddU64));
    VT_ASSERT_EQ(static_cast<int>(fn->entry_block()->instruction(3)->opcode()), static_cast<int>(Opcode::Return));
}

VT_TEST(ir_tests, lower_where_query_with_typed_columns) {
    std::string sql = "SELECT * FROM quotes WHERE price_ticks > 5000";
    query::Parser parser(sql);
    auto ast_res = parser.parse_query();
    VT_ASSERT(ast_res.ok());
    
    Builder builder;
    auto fn = builder.build_from_query(ast_res.value().get());
    VT_ASSERT(fn != nullptr);
    VT_ASSERT_EQ(fn->num_blocks(), 1ULL);
    
    // Check that price_ticks was lowered as LoadColumn with column_id 7 and Type::I64
    const BasicBlock* entry = fn->entry_block();
    VT_ASSERT(entry->num_instructions() >= 3ULL);
    
    const Instruction* load = entry->instruction(0);
    VT_ASSERT_EQ(static_cast<int>(load->opcode()), static_cast<int>(Opcode::LoadColumn));
    auto load_op = static_cast<const LoadColumnOp*>(load);
    VT_ASSERT_EQ(load_op->column_id(), 7U); // price_ticks is ID 7
    VT_ASSERT_EQ(static_cast<int>(load_op->result_type()), static_cast<int>(Type::I64));
}

VT_TEST(ir_tests, branch_and_jump_control_flow) {
    Builder builder;
    Function* fn = builder.create_function("test_branch");
    BasicBlock* entry = fn->entry_block();
    BasicBlock* then_block = builder.create_block("then");
    BasicBlock* else_block = builder.create_block("else");
    
    builder.set_insertion_point(entry);
    ValueId cond = builder.create_const_bool(true);
    builder.create_branch(cond, then_block, else_block);
    
    builder.set_insertion_point(then_block);
    ValueId v1 = builder.create_const_u64(100);
    builder.create_return(v1);
    
    builder.set_insertion_point(else_block);
    ValueId v2 = builder.create_const_u64(200);
    builder.create_return(v2);
    
    VT_ASSERT_EQ(fn->num_blocks(), 3ULL);
}
