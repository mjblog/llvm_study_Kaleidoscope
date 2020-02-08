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
	vector<shared_ptr<generic_ast>> ast_vec;	//存放所有已经创建的ast
	lexer& linked_lexer;
	const token& get_cur_token() {return linked_lexer.get_cur_token();}
	const token& get_next_token() {return linked_lexer.get_next_token();}
	void handle_toplevel_expression();
	void handle_definition();
	void handle_extern();
	shared_ptr<function_ast> parse_definition();
	shared_ptr<prototype_ast> parse_extern();
	shared_ptr<prototype_ast> parse_prototype();
	shared_ptr<expr_ast> parse_expr();
	shared_ptr<expr_ast> parse_primary_expr();
	shared_ptr<expr_ast> parse_number();
	shared_ptr<expr_ast> parse_identifier();
	shared_ptr<expr_ast> parse_paren();
/*
	shared_ptr<expr_ast> parse_binary_op_rhs(
		shared_ptr<expr_ast> lhs,  binary_operator_t prev_op_type, 
		int prev_op_prio);
*/
//	shared_ptr<expr_ast> parser::parse_binary_op_rhs(int prev_op_prio);
	shared_ptr<expr_ast> parse_binary_expr(int prev_op_prio, 
			shared_ptr<expr_ast> lhs);
public:
	parser(lexer& in_lexer) : linked_lexer(in_lexer) {}
	void parse();
	const vector<shared_ptr<generic_ast>>& get_ast_vec() const {return ast_vec;};
};

}   // end of namespace toy_compiler
#endif