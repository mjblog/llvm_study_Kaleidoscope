#include <sstream>
#include "lexer.h"
#include "parser.h"
#include "test_utils.h"
#include <gtest/gtest.h>
using namespace toy_compiler;

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
	prepare_parser_for_test_string tdef("extern minus(xp1 yp2)");
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
	ASSERT_TRUE(line2_left_ptr->get_op() == BINARY_SUB);

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
	ASSERT_TRUE(line2_right_ptr->get_op() == BINARY_SUB);

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


TEST(test_ast, if_ast)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef(
"def mt1(i) 1												"
"def mt(x)													"
"if mt1(1) < 5 then									"
"		if mt1(2) < 3 then							"
"			1														"
"		else													"
"			2														"
"else															"
"	3																");
	auto& ast_vec = tdef.get_ast_vec();
	//全局ast中有两个函数

	auto first = ast_vec[0];
	ASSERT_TRUE(first->get_type() == FUNCTION_AST);
	function_ast* func_ptr = static_cast<function_ast *> (first.get());
	prototype_ast* prototype_ptr = func_ptr->get_prototype().get();
	ASSERT_TRUE(prototype_ptr->get_name() == "mt1");

	auto second = ast_vec[1];
	ASSERT_TRUE(second->get_type() == FUNCTION_AST);
	function_ast* func_ptr2 = static_cast<function_ast *> (second.get());
	prototype_ast* prototype_ptr2 = func_ptr2->get_prototype().get();
	ASSERT_TRUE(prototype_ptr2->get_name() == "mt");

	expr_ast*  body = func_ptr2->get_body().get();
	ASSERT_TRUE(body->get_type() == IF_AST);
	if_ast* body_if = static_cast<if_ast *>(body);
	ASSERT_TRUE(body_if->get_cond()->get_type() == BINARY_OPERATOR_AST );
	ASSERT_TRUE(body_if->get_then()->get_type() == IF_AST);
	ASSERT_TRUE(body_if->get_else()->get_type() == NUMBER_AST);
}


TEST(test_ast, for_ast)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef(
"def mt1(i) i + 1												"
"def mt(x)															"
"for i = 1 : i < 5 : 1 in										"
"	mt1(i + x)														");
	auto& ast_vec = tdef.get_ast_vec();
	//全局ast中有两个函数

	auto first = ast_vec[0];
	ASSERT_TRUE(first->get_type() == FUNCTION_AST);
	function_ast* func_ptr = static_cast<function_ast *> (first.get());
	prototype_ast* prototype_ptr = func_ptr->get_prototype().get();
	ASSERT_TRUE(prototype_ptr->get_name() == "mt1");

	auto second = ast_vec[1];
	ASSERT_TRUE(second->get_type() == FUNCTION_AST);
	function_ast* func_ptr2 = static_cast<function_ast *> (second.get());
	prototype_ast* prototype_ptr2 = func_ptr2->get_prototype().get();
	ASSERT_TRUE(prototype_ptr2->get_name() == "mt");

	expr_ast*  body = func_ptr2->get_body().get();
	ASSERT_TRUE(body->get_type() == FOR_AST);
	for_ast* body_for = static_cast<for_ast *>(body);
	ASSERT_TRUE(body_for->get_idt_name() == "i" );
	ASSERT_TRUE(body_for->get_start()->get_type() == NUMBER_AST);
	ASSERT_TRUE(body_for->get_end()->get_type() == BINARY_OPERATOR_AST);
	ASSERT_TRUE(body_for->get_step()->get_type() == NUMBER_AST);
}

TEST(test_ast, user_defined_binary_operator_ast)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef(
"def binary / 30 (a b) a + b +1			"
"def mt(x)												"
"	x / x														");
	auto& ast_vec = tdef.get_ast_vec();
	//全局ast中有两个函数
	ASSERT_EQ(ast_vec.size(),  2);

	auto first = ast_vec[0];
	ASSERT_TRUE(first->get_type() == FUNCTION_AST);
	function_ast* func_ptr = static_cast<function_ast *> (first.get());
	prototype_ast* prototype_ptr = func_ptr->get_prototype().get();
	ASSERT_TRUE(prototype_ptr->get_name() == 
		prototype_ast::build_operator_external_name(2, "/", 30));

	auto second = ast_vec[1];
	ASSERT_TRUE(second->get_type() == FUNCTION_AST);
	function_ast* func_ptr2 = static_cast<function_ast *> (second.get());
	expr_ast*  body = func_ptr2->get_body().get();
	ASSERT_TRUE(body->get_type() == BINARY_OPERATOR_AST);
	binary_operator_ast* body_bin = static_cast<binary_operator_ast *>(body);

	ASSERT_TRUE(body_bin->get_op() == BINARY_USER_DEFINED);
	ASSERT_TRUE(body_bin->get_lhs()->get_type() == VARIABLE_AST);
	ASSERT_TRUE(body_bin->get_rhs()->get_type() == VARIABLE_AST);
}

TEST(test_ast, user_defined_unary_operator_ast)
{
	//读取string作为输入
	prepare_parser_for_test_string tdef(
"def unary ! (a) if a then 0 else 1		"
"def mt(x)													"
"	x + !x														");
	auto& ast_vec = tdef.get_ast_vec();
	//全局ast中有两个函数
	ASSERT_EQ(ast_vec.size(),  2);

	auto first = ast_vec[0];
	ASSERT_TRUE(first->get_type() == FUNCTION_AST);
	function_ast* func_ptr = static_cast<function_ast *> (first.get());
	prototype_ast* prototype_ptr = func_ptr->get_prototype().get();
	ASSERT_TRUE(prototype_ptr->get_name() == 
		prototype_ast::build_operator_external_name(1, "!"));

	auto second = ast_vec[1];
	ASSERT_TRUE(second->get_type() == FUNCTION_AST);
	function_ast* func_ptr2 = static_cast<function_ast *> (second.get());
	expr_ast*  body = func_ptr2->get_body().get();
	ASSERT_TRUE(body->get_type() == BINARY_OPERATOR_AST);
	binary_operator_ast* body_bin = static_cast<binary_operator_ast *>(body);

	ASSERT_TRUE(body_bin->get_op() == BINARY_ADD);
	ASSERT_TRUE(body_bin->get_lhs()->get_type() == VARIABLE_AST);
	ASSERT_TRUE(body_bin->get_rhs()->get_type() == UNARY_OPERATOR_AST);
	expr_ast* rhs = body_bin->get_rhs().get();
	unary_operator_ast* unary = static_cast<unary_operator_ast *>(rhs);
	ASSERT_EQ(unary->get_opcode(), "!");
	ASSERT_TRUE(unary->get_operand()->get_type() == VARIABLE_AST);
	ASSERT_EQ(unary->get_op_external_name(),
		prototype_ast::build_operator_external_name(1, "!"));
}

TEST(test_ast, user_defined_unary_operator_ast_unimplemented)
{
/*
为了阻止用户定义可能产生错误或者二义性的operator
我们在parser中对operator的名称做了检查，
语言征用的特殊字符如 ':' '=' '(' ')'不允许被自定义。
语言已经内置的运算符+-<*不允许被自定义。
原示例中的unary-暂时无法实现。
当然单就'-'来说，还有其他问题需要处理。
lexer中目前直接把内置'-'标记为了BINARY_TOKEN。
还需要专门设计一套机制用于表达这种同时可以有BINARY和UNARY语义的情况。
例如lexer不直接标记BINARY_TOKEN或者UNARY_TOKEN，而是提供
is_binary_op、is_unary_op等函数。
*/
	ASSERT_DEATH(
{
	prepare_parser_for_test_string tdef(
"def unary - (a)  0 - a 		"
"def mt(x)													"
"	x + -x														");
}, 
	"- is a protected char, which shoud not be redefined"
	);
}