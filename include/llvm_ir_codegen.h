#ifndef _LLVM_IR_CODEGEN_H_
#define _LLVM_IR_CODEGEN_H_

#include <filesystem>
#include "codegen.h"
#include "ast.h"

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
public:
	llvm_debug_info(Module* mod, const string& source)
	{
		DBuilder = new DIBuilder(*mod);
		namespace fs = std::filesystem;
		fs::path src_path(source);
		std::error_code ec;
		src_path = fs::canonical(src_path, ec);
		string_view src_dir = "";
		string_view src_file = "";
		if (!ec)
		{
			src_dir = src_path.parent_path().native();
			src_file = src_path.filename().native();
		}

/*
没有设置语言abi的情况下，LLVM默认按照C方式配置ABI，第一个选项为DW_LANG_C。
第四个选项不是指有没有开启编译优化，应该是给调试器用的信息(
参考https://reviews.llvm.org/D41985)。所以维持原示例的false设置。
第五个runtime version还不存在，所以设置为0.
*/
		compile_unit = DBuilder->createCompileUnit(dwarf::DW_LANG_C,
			DBuilder->createFile(src_file, src_dir), "Kaleidoscope Compiler",
			false, "", 0);
		double_type = DBuilder->createBasicType("double",
			64, dwarf::DW_ATE_float);
		assert(compile_unit != nullptr);
		assert(double_type != nullptr);
	}

	~llvm_debug_info()
	{
		delete DBuilder;
	}
};

class LLVM_IR_code_generator final : public code_generator<Value *>
{
	LLVMContext the_context;
	IRBuilder<> ir_builder;
	Module* the_module;
	Function* cur_func;
	llvm_debug_info* debug_info;
	std::map<std::string, AllocaInst *> named_var;
	AllocaInst* create_alloca_at_func_entry(Function* func, 
		const string& var_ame);
public:
	LLVM_IR_code_generator(StringRef file_name = "unamed") 
		: ir_builder(the_context) , cur_func(nullptr)
	{
		the_module = new(Module)(file_name, the_context);
		debug_info = new(llvm_debug_info)(the_module, file_name.str());
	}

	~LLVM_IR_code_generator()
	{
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
	void finalize() {debug_info->DBuilder->finalize();}
};
/*
class MAPLE_IR_code_generator : public code_generator
{

};
*/
}   // end of namespace toy_compiler

#endif