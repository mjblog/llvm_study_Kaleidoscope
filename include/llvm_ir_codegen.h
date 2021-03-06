#ifndef _LLVM_IR_CODEGEN_H_
#define _LLVM_IR_CODEGEN_H_

#include "codegen.h"
#include "ast.h"
#include "flags.h" //for global_flags.debug_info
#include "llvm/IR/Type.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/DIBuilder.h" //for DIBuilder
#include "utils.h" /* for err_print*/

namespace toy_compiler{
using namespace llvm;
struct llvm_debug_info
{
	DIBuilder* DBuilder;
	DICompileUnit* compile_unit;
	DIType* double_type;
	std::vector<DIScope*> lexical_blocks;
public:
	llvm_debug_info(Module* mod, const string& source);
	~llvm_debug_info();
};

class LLVM_IR_code_generator final : public code_generator<Value *>
{
	LLVMContext the_context;
	IRBuilder<> ir_builder;
	Module* the_module;
	Function* cur_func = nullptr;
	llvm_debug_info* debug_info = nullptr;
	std::map<std::string, AllocaInst *> named_var;
	AllocaInst* create_alloca_at_func_entry(Function* func, 
		const string& var_ame);
public:
	LLVM_IR_code_generator(StringRef file_name = "unamed") 
		: ir_builder(the_context)
	{
		the_module = new(Module)(file_name, the_context);
		if (global_flags.debug_info)
			debug_info = new(llvm_debug_info)(the_module, file_name.str());
	}

	~LLVM_IR_code_generator()
	{
		if (debug_info)
			delete debug_info;
		delete the_module;
	};
	bool gen_function(const function_ast* func) override;
	bool gen_prototype(const prototype_ast* proto) override;
	Value* build_expr(const expr_ast* expr) override;
	Value* build_call(const call_ast* callee) override;
	Value* build_number(const number_ast* num) override;
	Value* build_variable(const variable_ast* var) override;
	Value* build_binary_op(const binary_operator_ast* binary) override;
	Value* build_unary_op(const unary_operator_ast* unary) override;
	Value* build_if(const if_ast* if_expr) override;
	Value* build_for(const for_ast* for_expr) override;
	Value* build_var(const var_ast* var_expr) override;
//	bool codegen(ast_vector_t& global_vec) override;

	void print_IR() override;
	void print_IR_to_str(string& out) override;
	void print_IR_to_file(int fd);
	void print_IR_to_file(string& filename);
	Module* get_module(){return the_module;}
	void finalize()
	{
		if (debug_info)
			debug_info->DBuilder->finalize();
	}
	void emit_location(const source_location& log);
};
/*
class MAPLE_IR_code_generator : public code_generator
{

};
*/
}   // end of namespace toy_compiler

#endif