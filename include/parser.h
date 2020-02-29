#ifndef _PARSER_H_
#define _PARSER_H_
#include <iostream>
#include <vector>
#include <string>
#include<map>
#include<unordered_map>
#include "ast.h"
#include "lexer.h"
namespace toy_compiler{
using namespace std;
/*
parser的主要职责是从lexer中读取token构建出ast，也就是确定程序的语义
*/
class parser final
{
	ast_vector_t ast_vec;	//存放所有已经创建的ast
	lexer& linked_lexer;
	//proto查找表
	unordered_map<string_view, prototype_ast*> prototype_tab;
	//用户自定义operator的优先级查找表
	map<string, int> user_defined_operator_prio_tab;
	const token& get_cur_token() {return linked_lexer.get_cur_token();}
	const token& get_next_token() {return linked_lexer.get_next_token();}
	void handle_toplevel_expression();
	void handle_definition();
	void handle_extern();
	shared_ptr<function_ast> parse_definition();
	prototype_t parse_extern();
	prototype_t parse_prototype();
	expr_t parse_expr();
	expr_t parse_primary_expr();
	expr_t parse_number();
	expr_t parse_identifier();
	expr_t parse_paren();
	expr_t parse_if();
	expr_t parse_for();
/*
	expr_t parse_binary_op_rhs(
		expr_t lhs,  binary_operator_t prev_op_type, 
		int prev_op_prio);
*/
//	expr_t parser::parse_binary_op_rhs(int prev_op_prio);
	expr_t parse_binary_expr(int prev_op_prio, expr_t lhs);
	expr_t parse_unary_expr();
public:
	parser(lexer& in_lexer) : linked_lexer(in_lexer) {}
	void parse();
	const ast_vector_t& get_ast_vec() const {return ast_vec;};

//所有的原型都放在这里，以便call的时候查找，算是函数符号表了
	unordered_map<string_view, prototype_ast*>& get_proto_tab()
	{
		return prototype_tab;
	}

	prototype_t find_prototype(const string_view& key)
	{
		const auto& result = get_proto_tab().find(key);
		if (result != get_proto_tab().cend())
			return result->second->get_shared_ptr();
		else
			return nullptr;
	}

	int get_user_defined_operator_prio(const string& op)
	{
		const auto result = user_defined_operator_prio_tab.find(op);
		if (result != user_defined_operator_prio_tab.cend())
			return result->second;
		else
			return  -1;
	}

	bool set_user_defined_operator_prio(const string& op, int prio)
	{
		const auto result = user_defined_operator_prio_tab.find(op);
		if (result == user_defined_operator_prio_tab.cend())
		{
			//string可以直接make_pair，会拷贝一份
			user_defined_operator_prio_tab.insert(make_pair(op, prio));
			return true;
		}
		else
			return  false;
	}
};

}   // end of namespace toy_compiler
#endif