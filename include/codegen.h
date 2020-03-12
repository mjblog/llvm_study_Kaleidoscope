#ifndef _CODEGEN_H_
#define _CODEGEN_H_
#include <iostream>
#include <fstream>
#include <string>

#include "ast.h"
#include "utils.h" /* for err_print*/

namespace toy_compiler{
/* 
从ast读出数据转换为IR的流程是基本固定的，就是遍历结构，逐步向下细化；
由于这个遍历过程是基本稳定的，可以将其抽象为统一的接口。
*/
template <typename VAL_PTR>
class code_generator
{
public:
	virtual ~code_generator(){};
	virtual bool gen_function(const function_ast* func) = 0;
	virtual bool gen_prototype(const prototype_ast* proto) = 0;
	virtual VAL_PTR build_expr(const expr_ast* expr) = 0;
	virtual VAL_PTR build_call(const call_ast* callee) = 0;
	virtual VAL_PTR build_number(const number_ast* num) = 0;
	virtual VAL_PTR build_variable(const variable_ast* var) = 0;
	virtual VAL_PTR build_binary_op(const binary_operator_ast* binary) = 0;
	virtual VAL_PTR build_unary_op(const unary_operator_ast* unary) = 0;
	virtual VAL_PTR build_if(const if_ast* if_expr) = 0;
	virtual VAL_PTR build_for(const for_ast* for_expr) = 0;
	virtual VAL_PTR build_var(const var_ast* var_expr) = 0;
	virtual void finalize() = 0;
	virtual bool codegen(const ast_vector_t& global_vec) 
	{
		for (auto ast : global_vec)
		{
			auto ast_type = ast->get_type();
			switch (ast_type)
			{
				//extern 声明
				case PROTOTYPE_AST:
					gen_prototype((const prototype_ast*)ast.get());
					break;
				//def函数定义
				case FUNCTION_AST:
					gen_function((const function_ast*)ast.get());
					break;
				default:
/*
原示例中的全局表达式处理更像一个解释器而不是一个常规编译器的处理方法。
按照通常的定义，应该在全局只允许定义全局变量、定义函数、声明extern。
为了简单，暂时不处理全局的其他表示。转而和c一样定义main函数作为程序入口。
*/
					err_print(false, "can not handle "
					"global ast other than def/extern\n");
			}
		}
/*
成功完成生成后调用一次finalize，可以做一些收尾工作。
如LLVM调试信息生成的DBuilder，就需要finalize。
*/
		finalize();
		return true;
	};
	virtual void print_IR() = 0;
	virtual void print_IR_to_str(string& out) = 0;
};

/*
class MAPLE_IR_code_generator : public code_generator
{

};
*/
}   // end of namespace toy_compiler

#endif