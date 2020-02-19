#ifndef _PARSER_H_
#define _PARSER_H_
#include <iostream>
#include <vector>
#include <string>
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
	expr_t parse_if() ;
/*
	expr_t parse_binary_op_rhs(
		expr_t lhs,  binary_operator_t prev_op_type, 
		int prev_op_prio);
*/
//	expr_t parser::parse_binary_op_rhs(int prev_op_prio);
	expr_t parse_binary_expr(int prev_op_prio, 
			expr_t lhs);
public:
	parser(lexer& in_lexer) : linked_lexer(in_lexer) {}
	void parse();
	const ast_vector_t& get_ast_vec() const {return ast_vec;};
};

}   // end of namespace toy_compiler
#endif