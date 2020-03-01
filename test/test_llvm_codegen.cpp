#include <sstream>
#include "lexer.h"
#include "parser.h"
#include "llvm_ir_codegen.h"
#include "test_utils.h"
#include <gtest/gtest.h>
using namespace toy_compiler;

TEST(test_llvm_codegen, codegen_def)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef("def foo(x y) x+y");
	//全局ast中现在只有这个函数
    const auto& ast_vec = tdef.get_ast_vec();
    LLVM_IR_code_generator code_generator;
    ASSERT_TRUE(code_generator.codegen(ast_vec));
    code_generator.print_IR();
	ASSERT_TRUE(1);
}

TEST(test_llvm_codegen, codegen_extern)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef("extern sin(x)");
	//全局ast中现在只有这个函数
    const auto& ast_vec = tdef.get_ast_vec();
    LLVM_IR_code_generator code_generator;
    ASSERT_TRUE(code_generator.codegen(ast_vec));
    code_generator.print_IR();
	ASSERT_TRUE(1);
}


TEST(test_llvm_codegen, codegen_user_defined_op1)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef(
"def unary ! (a) if a then 0 else 1		"
"def mt(x)													"
"	x + !x														");
	//全局ast中现在只有这个函数
    const auto& ast_vec = tdef.get_ast_vec();
    LLVM_IR_code_generator code_generator;
    ASSERT_TRUE(code_generator.codegen(ast_vec));
    code_generator.print_IR();
	ASSERT_TRUE(1);
}

TEST(test_llvm_codegen, codegen_user_defined_op2)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef(
"def binary / 30 (a b) a + b +1			"
"def mt(x)												"
"	x / x														");
	//全局ast中现在只有这个函数
    const auto& ast_vec = tdef.get_ast_vec();
    LLVM_IR_code_generator code_generator;
    ASSERT_TRUE(code_generator.codegen(ast_vec));
    code_generator.print_IR();
	ASSERT_TRUE(1);
}

