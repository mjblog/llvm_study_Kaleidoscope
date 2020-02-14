#include <cassert>
#include <memory>

#include "llvm_ir_codegen.h"
#include "utils.h"

namespace toy_compiler{
using namespace std;
/*
	void gen_function(const function_ast* func) override;
	void gen_prototype(const prototype_ast* expr) override;
	void gen_expr(const expr_ast* expr) override;
	void gen_call(const call_ast* callee) override;
	void gen_number(const number_ast* num) override;
	void gen_varaible(const variable_ast* var) override;
*/

/*
这里的很多骨架逻辑是可以抽象到父类中去的。
如检查重复定义，先gen_prototype然后处理body。
抽象后，可以把通用的逻辑放到父类的handle_function中，
子类中提供具体的接口功能实现。
考虑目前只有一种实现，既没有必要也很难抽象准确，暂时不实施这个改进。
*/
bool LLVM_IR_code_generator::gen_function(const function_ast* func)
{
	//1 重复定义检查
	auto proto = func->get_prototype();
	auto proto_ptr = proto.get();
	assert(proto_ptr != nullptr && cur_func == nullptr);
	const string &func_name = proto_ptr->get_name();
	Function *new_func = the_module.getFunction(func_name);
	if (new_func)
	{
		err_print(false, "found multidef for function %s\n",
			func_name.c_str());
		return false;
	}

/*
前移两个指针定义到这里，规避goto err_exit引起的告警
transfer of control bypasses initialization of:
*/
	BasicBlock *bb;
	Value* ret_val;
	//2 生成prototype
	if (!gen_prototype(proto_ptr))
	{
		err_print(false, "can not generate prototype for function %s\n",
			proto_ptr->get_name().c_str());
		goto err_exit;
	}

	//gen_prototype会创建llvm中的函数声明
	cur_func = the_module.getFunction(func_name);
	assert(cur_func != nullptr);
	//创建args查找map，方便后续variable引用
	cur_func_args.clear();
	for (auto &arg : cur_func->args())
		cur_func_args[arg.getName()] = &arg;

	//3 生成body
	//设置好插入点，调用build_expr函数获得expr的返回值，构建返回指令
	bb = BasicBlock::Create(the_context, "entry", cur_func);
	ir_builder.SetInsertPoint(bb);
	//build_expr中会调用ir_builder插入计算expr结果的运算指令
	ret_val = build_expr(func->get_body().get());
	if (ret_val == nullptr)
	{
		err_print(false, "can not generate body for function %s\n",
			proto_ptr->get_name().c_str());
		goto err_exit;
	}

    assert(ir_builder.CreateRet(ret_val) != nullptr);
    // 检查错误，没有错误时返回false
    assert(verifyFunction(*cur_func, &errs()) == false);
	//只要离开本函数，都应该把cur_func重新设置为空
	cur_func = nullptr;
	return true;

err_exit:
  //remove  function which is incompleted
	cur_func->eraseFromParent();
	cur_func = nullptr;
	return false;
}

//gen_prototype的主要任务是构建llvm的函数声明 
bool LLVM_IR_code_generator::gen_prototype(const prototype_ast* proto)
{
	// 入参全是double型
	auto double_type = Type::getDoubleTy(the_context);
	std::vector<Type *> arg_vec(proto->get_args().size(), double_type);
	FunctionType *FT = FunctionType::get(double_type, arg_vec, false);

	Function *F = Function::Create(FT, Function::ExternalLinkage, 
		proto->get_name(), &the_module);

	// Set names for all arguments.
	unsigned idx = 0;
	const auto& arg_str_vec = proto->get_args();
	for (auto& arg : F->args())
		arg.setName(arg_str_vec[idx++]);

//fixme!! 这里的所有操作都一定能成功么？？
	return true;
}

Value* LLVM_IR_code_generator::build_expr(const expr_ast* expr)
{
	switch (expr->get_type())
	{
		case CALL_AST:
			return build_call((const call_ast*) expr);
		case NUMBER_AST:
			return build_number((const number_ast*) expr);
		case VARIABLE_AST:
			return build_variable((const variable_ast*) expr);
		case BINARY_OPERATOR_AST:
			return build_binary_op((binary_operator_ast *)expr);
		default:
			;
			err_print(/*isfatal*/true, "found unknown expr AST, aborting\n");
	}
	return nullptr;
}

Value* LLVM_IR_code_generator::build_call(const call_ast* callee)
{
	// Look up the name in the global module table.
	const string& callee_name = callee->get_callee()->get_name();
	Function *callee_func = the_module.getFunction(callee_name);
	print_and_return_nullptr_if_check_fail(callee_func != nullptr, 
		"unknown function %s referenced\n", callee_name.c_str());

	//检查传入点和定义点的参数个数是否一致，类型都是double无需检查
	auto def_arg_size = callee_func->arg_size();
	auto passed_arg_vec = callee->get_args();
	auto passed_arg_size = passed_arg_vec.size();
	print_and_return_nullptr_if_check_fail(def_arg_size == passed_arg_size,
		"expected %lu args but passed %lu\n", def_arg_size, passed_arg_size);
	//为llvm的调用指令准备参数vector
	std::vector<Value *> args_vec;
	for (unsigned int idx = 0; idx < passed_arg_size; idx++)
	{
		auto arg_val = build_expr(passed_arg_vec[idx].get());
		print_and_return_nullptr_if_check_fail(arg_val != nullptr, 
			"can not get value when passing arg %d  for calling %s\n",
			idx, callee_name.c_str());
		args_vec.push_back(arg_val);
	}

	return ir_builder.CreateCall(callee_func, args_vec, "call" + callee_name);
}

Value* LLVM_IR_code_generator::build_number(const number_ast* num)
{
	//直接转换为llvm的值就可以了
	return ConstantFP::get(the_context, APFloat(num->get_val()));
}

//当前还未支持局部变量定义和全局变量定义，实际上variable就只有入参
Value* LLVM_IR_code_generator::build_variable(const variable_ast* var)
{
	// cur_func_args 在gen_prorotype时准备好
	const string& var_name = var->get_name();
	Value *V = cur_func_args[var_name];
	print_and_return_nullptr_if_check_fail(V != nullptr, 
		"Unknown variable name %s\n", var_name.c_str());

	return V;
}

Value* LLVM_IR_code_generator::build_binary_op(const binary_operator_ast* bin)
{
	auto lhs = build_expr(bin->get_lhs().get());
	print_and_return_nullptr_if_check_fail(lhs != nullptr,
		"failed build lhs of binary operator\n");
	auto rhs = build_expr(bin->get_rhs().get());
	print_and_return_nullptr_if_check_fail(rhs != nullptr,
		"failed build lhs of binary operator\n");
	Value* cmp;
	switch (bin->get_op())
	{
		case BINARY_ADD:
			return ir_builder.CreateFAdd(lhs, rhs, "addtmp");
		case BINARY_SUB:
			return ir_builder.CreateFSub(lhs, rhs, "subtmp");
		case BINARY_MUL:
			return ir_builder.CreateFMul(lhs, rhs, "multmp");
		case BINARY_LESS_THAN:
			cmp = ir_builder.CreateFCmpULT(lhs, rhs, "cmptmp");
			// Convert bool 0/1 to double 0.0 or 1.0
			return ir_builder.CreateUIToFP(cmp,
				Type::getDoubleTy(the_context), "booltmp");
		case BINARY_UNKNOWN:
		default:
			err_print(true, "unknown binary op, aborting\n");
	}
}

void LLVM_IR_code_generator::print_IR()
{
	the_module.print(outs(), nullptr);
}
}	//end of toy_compiler