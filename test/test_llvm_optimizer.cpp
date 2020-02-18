#include <sstream>
#include "lexer.h"
#include "parser.h"
#include "llvm_ir_codegen.h"
#include "llvm_optimizer.h"
#include "test_utils.h"
#include <gtest/gtest.h>
using namespace toy_compiler;

TEST(test_llvm_optimizer, module_optimizer)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef("def foo(x y) x+y");
	//全局ast中现在只有这个函数
	const auto& ast_vec = tdef.get_ast_vec();
	LLVM_IR_code_generator code_generator;
	ASSERT_TRUE(code_generator.codegen(ast_vec));
	Module* module = code_generator.get_module();
	llvm_optimizer::optimize_module(*module);
	cout << "after opt" << endl;
	code_generator.print_IR();
	ASSERT_TRUE(1);
}

TEST(test_llvm_optimizer, function_optimizer)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef("def foo(x y) x+y");
	//全局ast中现在只有这个函数
	const auto& ast_vec = tdef.get_ast_vec();
	LLVM_IR_code_generator code_generator;
	ASSERT_TRUE(code_generator.codegen(ast_vec));
	Module* module = code_generator.get_module();

	Function& func = module->getFunctionList().front();
	llvm_optimizer::optimize_function(func);
	code_generator.print_IR();
	ASSERT_TRUE(1);
}


TEST(test_llvm_optimizer, function_optimizer_extern)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef("extern sin(x) def foo(x y) x+sin(y)");
	//全局ast中现在只有这个函数
	const auto& ast_vec = tdef.get_ast_vec();
	LLVM_IR_code_generator code_generator;
	ASSERT_TRUE(code_generator.codegen(ast_vec));
	Module* module = code_generator.get_module();
	cout << "after opt" << endl;
	Function& func = *(module->getFunction("foo"));
	llvm_optimizer::optimize_function(func);
	string ir_out;
	code_generator.print_IR_to_str(ir_out);
	cout << ir_out << endl;
	ASSERT_TRUE(1);
}

