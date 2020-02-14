#ifndef _LLVM_IR_CODEGEN_H_
#define _LLVM_IR_CODEGEN_H_

#include "codegen.h"
#include "ast.h"

#include "llvm/IR/Type.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Verifier.h"
#include "utils.h" /* for err_print*/

namespace toy_compiler{
using namespace llvm;
class LLVM_IR_code_generator final : public code_generator<Value *>
{
	LLVMContext the_context;
	IRBuilder<> ir_builder;
	Module the_module;
	Function* cur_func;
	std::map<std::string, Value *> cur_func_args;
public:
	LLVM_IR_code_generator(StringRef name = "unamed") : ir_builder(the_context) , 
		the_module(name, the_context) , cur_func(nullptr){}
	~LLVM_IR_code_generator(){};
	bool gen_function(const function_ast* func) override;
	bool gen_prototype(const prototype_ast* proto) override;
	Value* build_expr(const expr_ast* expr) override;
	Value* build_call(const call_ast* callee) override;
	Value* build_number(const number_ast* num) override;
	Value* build_variable(const variable_ast* var) override;
	Value* build_binary_op(const binary_operator_ast* var) override;
//	bool codegen(ast_vector_t& global_vec) override;
	void print_IR() override;
};
/*
class MAPLE_IR_code_generator : public code_generator
{

};
*/
}   // end of namespace toy_compiler

#endif