#include <sstream>
#include "lexer.h"
#include "parser.h"
#include <gtest/gtest.h>
using namespace toy_compiler;
class prepare_parser_for_test_string
{
	stringstream test_input;
	streambuf *backbuf;
	lexer test_lexer;

public:
	parser test_parser;
	prepare_parser_for_test_string(const char * input) : 
		test_input(input), test_lexer(), test_parser(test_lexer)
	{
		backbuf = cin.rdbuf(test_input.rdbuf());
		test_parser.parse();
	}
	~prepare_parser_for_test_string()
	{
		cin.rdbuf(backbuf);
		//必须clear一下，否则前一个stream eof了，后面就无法再用了
		cin.clear();
	}
	auto& get_ast_vec() const {return test_parser.get_ast_vec();}
};

TEST(test_ast, def)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef("def foo(x y) x+y");
	auto& ast_vec = tdef.get_ast_vec();
	//全局ast中现在只有这个函数
	auto def_ast = ast_vec[0];
	ASSERT_TRUE(def_ast->get_type() == FUNCTION_AST);
	function_ast* func_ptr = static_cast<function_ast *> (def_ast.get());
	prototype_ast* prototype_ptr = func_ptr->get_prototype().get();
	ASSERT_TRUE(prototype_ptr->get_name() == "foo");
	const vector<string> args = prototype_ptr->get_args();
	const string & arg1 = args[0];
	const string & arg2 = args[1];
	ASSERT_TRUE(arg1 == "x");
	ASSERT_TRUE(arg2 == "y");
	expr_ast*  body = func_ptr->get_body().get();
	ASSERT_TRUE(body->get_type() == BINARY_OPERATOR_AST);
	binary_operator_ast *body_bin = static_cast<binary_operator_ast *>(body);
	ASSERT_TRUE(body_bin->get_op() == BINARY_ADD);
	ASSERT_TRUE(body_bin->get_lhs()->get_type() == VARIABLE_AST);
	ASSERT_TRUE(((variable_ast *)(body_bin->get_lhs().get()))->get_name() == "x");
	ASSERT_TRUE(((variable_ast *)(body_bin->get_rhs().get()))->get_name() == "y");
}

TEST(test_ast, external)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef("extern minus(xp1 yp2) xp1 - yp2");
	auto& ast_vec = tdef.get_ast_vec();
	//全局ast中现在只有这个extern
	auto extern_ast = ast_vec[0];
	ASSERT_TRUE(extern_ast->get_type() == PROTOTYPE_AST);
	prototype_ast* prototype_ptr = static_cast<prototype_ast *> (extern_ast.get());
	ASSERT_TRUE(prototype_ptr->get_name() == "minus");
	const vector<string> args = prototype_ptr->get_args();
	const string & arg1 = args[0];
	const string & arg2 = args[1];
	ASSERT_TRUE(arg1 == "xp1");
	ASSERT_TRUE(arg2 == "yp2");
}



TEST(test_ast, binary_op)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef("def foo(x y z v w) x + y*z - v + w");
	auto& ast_vec = tdef.get_ast_vec();
	//全局ast中现在只有这个函数
	auto def_ast = ast_vec[0];
	ASSERT_TRUE(def_ast->get_type() == FUNCTION_AST);
	function_ast* func_ptr = static_cast<function_ast *> (def_ast.get());
	expr_ast*  body = func_ptr->get_body().get();
	ASSERT_TRUE(body->get_type() == BINARY_OPERATOR_AST);
	binary_operator_ast *body_bin = static_cast<binary_operator_ast *>(body);
/*
满足同优先级左结合的x + y*z - v + w ast树结构:
line1:						+
line2:				-						w
line3:		+				v
line4:	x		*
line5:		y		z
*/

	ASSERT_TRUE(body_bin->get_op() == BINARY_ADD);
	ASSERT_TRUE(body_bin->get_rhs()->get_type() == VARIABLE_AST);
	ASSERT_TRUE(((variable_ast *)(body_bin->get_rhs().get()))->get_name() == "w");

	auto line2_left = body_bin->get_lhs();
	ASSERT_TRUE(line2_left->get_type() == BINARY_OPERATOR_AST);
	binary_operator_ast *line2_left_ptr = static_cast<binary_operator_ast *>(line2_left.get());
	ASSERT_TRUE(line2_left_ptr->get_op() == BINARY_MINUS);

//line3:		+				v
	auto line3_left = line2_left_ptr->get_lhs();
	ASSERT_TRUE(line3_left->get_type() == BINARY_OPERATOR_AST);
	binary_operator_ast *line3_left_ptr = static_cast<binary_operator_ast *>(line3_left.get());
	ASSERT_TRUE(line3_left_ptr->get_op() == BINARY_ADD);

	auto line3_right = line2_left_ptr->get_rhs();
	ASSERT_TRUE(line3_right->get_type() == VARIABLE_AST);
	variable_ast* line3_right_ptr = static_cast<variable_ast *>(line3_right.get());
	ASSERT_TRUE(line3_right_ptr->get_name() == "v");

//line4:	x		*
	auto line4_left = line3_left_ptr->get_lhs();
	ASSERT_TRUE(line4_left->get_type() == VARIABLE_AST);
	auto line4_left_ptr = static_cast<variable_ast*>(line4_left.get());
	ASSERT_TRUE(line4_left_ptr->get_name() == "x");

	auto line4_right = line3_left_ptr->get_rhs();
	ASSERT_TRUE(line4_right->get_type() == BINARY_OPERATOR_AST);
	auto line4_right_ptr = static_cast<binary_operator_ast *>(line4_right.get());
	ASSERT_TRUE(line4_right_ptr->get_op() == BINARY_MUL);

//line5:			y		z
	auto line5_left = line4_right_ptr->get_lhs();
	ASSERT_TRUE(line5_left->get_type() == VARIABLE_AST);
	variable_ast* line5_left_ptr = static_cast<variable_ast *>(line5_left.get());
	ASSERT_TRUE(line5_left_ptr->get_name() == "y");

	auto line5_right = line4_right_ptr->get_rhs();
	ASSERT_TRUE(line5_right->get_type() == VARIABLE_AST);
	variable_ast* line5_right_ptr = static_cast<variable_ast *>(line5_right.get());
	ASSERT_TRUE(line5_right_ptr->get_name() == "z");
}

TEST(test_ast, binary_op2)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef("def foo(x y z v w) x < y*z + v - w");
	auto& ast_vec = tdef.get_ast_vec();
	//全局ast中现在只有这个函数
	auto def_ast = ast_vec[0];
	ASSERT_TRUE(def_ast->get_type() == FUNCTION_AST);
	function_ast* func_ptr = static_cast<function_ast *> (def_ast.get());
	expr_ast*  body = func_ptr->get_body().get();
	ASSERT_TRUE(body->get_type() == BINARY_OPERATOR_AST);
	binary_operator_ast *body_bin = static_cast<binary_operator_ast *>(body);
/*
满足同优先级左结合的x < y*z + v - w ast树结构:
line1:					<
line2:	x								-
line3:							+			w
line4:					*			v
line5:				y		z
*/

	ASSERT_TRUE(body_bin->get_op() == BINARY_LESS_THAN);
	ASSERT_TRUE(body_bin->get_lhs()->get_type() == VARIABLE_AST);
	ASSERT_TRUE(((variable_ast *)(body_bin->get_lhs().get()))->get_name() == "x");

	auto line2_right = body_bin->get_rhs();
	ASSERT_TRUE(line2_right->get_type() == BINARY_OPERATOR_AST);
	binary_operator_ast *line2_right_ptr = static_cast<binary_operator_ast *>(line2_right.get());
	ASSERT_TRUE(line2_right_ptr->get_op() == BINARY_MINUS);

//line3:							+			w
	auto line3_left = line2_right_ptr->get_lhs();
	ASSERT_TRUE(line3_left->get_type() == BINARY_OPERATOR_AST);
	binary_operator_ast *line3_left_ptr = static_cast<binary_operator_ast *>(line3_left.get());
	ASSERT_TRUE(line3_left_ptr->get_op() == BINARY_ADD);

	auto line3_right = line2_right_ptr->get_rhs();
	ASSERT_TRUE(line3_right->get_type() == VARIABLE_AST);
	variable_ast* line3_right_ptr = static_cast<variable_ast *>(line3_right.get());
	ASSERT_TRUE(line3_right_ptr->get_name() == "w");

//line4:					*			v
	auto line4_left = line3_left_ptr->get_lhs();
	ASSERT_TRUE(line4_left->get_type() == BINARY_OPERATOR_AST);
	auto line4_left_ptr = static_cast<binary_operator_ast *>(line4_left.get());
	ASSERT_TRUE(line4_left_ptr->get_op() == BINARY_MUL);

	auto line4_right = line3_left_ptr->get_rhs();
	ASSERT_TRUE(line4_right->get_type() == VARIABLE_AST);
	auto line4_right_ptr = static_cast<variable_ast*>(line4_right.get());
	ASSERT_TRUE(line4_right_ptr->get_name() == "v");

//line5:				y		z
	auto line5_left = line4_left_ptr->get_lhs();
	ASSERT_TRUE(line5_left->get_type() == VARIABLE_AST);
	variable_ast* line5_left_ptr = static_cast<variable_ast *>(line5_left.get());
	ASSERT_TRUE(line5_left_ptr->get_name() == "y");

	auto line5_right = line4_left_ptr->get_rhs();
	ASSERT_TRUE(line5_right->get_type() == VARIABLE_AST);
	variable_ast* line5_right_ptr = static_cast<variable_ast *>(line5_right.get());
	ASSERT_TRUE(line5_right_ptr->get_name() == "z");
}



TEST(test_ast, number_ast)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef("x + 123.456");
	auto& ast_vec = tdef.get_ast_vec();
	//全局ast中现在只有这个expr
	auto bin_ast = ast_vec[0];
	ASSERT_TRUE(bin_ast->get_type() == BINARY_OPERATOR_AST);
	auto bin_ptr = static_cast<binary_operator_ast*> (bin_ast.get());
	auto num_ast = bin_ptr->get_rhs();
	ASSERT_TRUE(num_ast->get_type() == NUMBER_AST);
	auto num_ptr = static_cast<number_ast*> (num_ast.get());
	ASSERT_TRUE(num_ptr->get_val() == 123.456);
}

TEST(test_ast, call_ast)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef("def xadd(x) x+2 def yadd(y) y+xadd(1.1)");
	auto& ast_vec = tdef.get_ast_vec();
	//全局ast中有两个函数
	auto first = ast_vec[0];
	ASSERT_TRUE(first->get_type() == FUNCTION_AST);
	function_ast* func_ptr = static_cast<function_ast *> (first.get());
	prototype_ast* prototype_ptr = func_ptr->get_prototype().get();
	ASSERT_TRUE(prototype_ptr->get_name() == "xadd");

	auto second = ast_vec[1];
	ASSERT_TRUE(second->get_type() == FUNCTION_AST);
	function_ast* func_ptr2 = static_cast<function_ast *> (second.get());
	prototype_ast* prototype_ptr2 = func_ptr2->get_prototype().get();
	ASSERT_TRUE(prototype_ptr2->get_name() == "yadd");

	expr_ast*  body = func_ptr2->get_body().get();
	ASSERT_TRUE(body->get_type() == BINARY_OPERATOR_AST);
	binary_operator_ast *body_bin = static_cast<binary_operator_ast *>(body);
	ASSERT_TRUE(body_bin->get_op() == BINARY_ADD);
	ASSERT_TRUE(body_bin->get_lhs()->get_type() == VARIABLE_AST);
	ASSERT_TRUE(((variable_ast *)(body_bin->get_lhs().get()))->get_name() == "y");
	auto call = body_bin->get_rhs();
	ASSERT_TRUE(call->get_type() == CALL_AST);
	call_ast* call_ptr = static_cast<call_ast *> (call.get());
	ASSERT_TRUE(call_ptr->get_callee() == func_ptr->get_prototype());
}