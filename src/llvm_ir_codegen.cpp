#include <cassert>
#include <memory>

#include "llvm_ir_codegen.h"
#include "utils.h"

namespace toy_compiler{
using namespace std;

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
	Function *new_func = the_module->getFunction(func_name);
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
	cur_func = the_module->getFunction(func_name);
	assert(cur_func != nullptr);

/*
做其他动作前，创建函数的entry block，设置好插入点。
确保后面的动作能正确在entry block中分配临时变量的alloca
注意这个创建entry_block的动作不能前提到gen_prototype之前，
因为cur_func对应的Function结构是在gen_prototype中创建的。
*/
	bb = BasicBlock::Create(the_context, "entry", cur_func);
	ir_builder.SetInsertPoint(bb);

	//创建args查找map，方便后续variable引用
	named_var.clear();
	for (auto &arg : cur_func->args())
	{
		const string& arg_name = arg.getName().str();
		auto arg_alloca = create_alloca_at_func_entry(cur_func, arg_name);
		ir_builder.CreateStore(&arg, arg_alloca);
		named_var[arg_name] = arg_alloca;
	}

	//3 生成body
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
		proto->get_name(), the_module);

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
			return build_binary_op((const binary_operator_ast *)expr);
		case UNARY_OPERATOR_AST:
			return build_unary_op((const unary_operator_ast *)expr);
		case IF_AST:
			return build_if((const if_ast *)expr);
		case FOR_AST:
			return build_for((const for_ast *)expr);
		default:
			err_print(/*isfatal*/true, "found unknown expr AST, aborting\n");
	}
	return nullptr;
}

Value* LLVM_IR_code_generator::build_call(const call_ast* callee)
{
	// Look up the name in the global module table.
	const string& callee_name = callee->get_callee()->get_name();
	Function *callee_func = the_module->getFunction(callee_name);
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
/*
 named_var 中记录了当前可引用的全部变量。
 为了支持可改写的变量，并保持一致性，所有的变量在初始生成时
 都改为放到stack中去分配。后续会用llvm 的mem2reg优化重新转回寄存器。
 */
	const string& var_name = var->get_name();
	Value *V = named_var[var_name];
	print_and_return_nullptr_if_check_fail(V != nullptr, 
		"Unknown variable name %s\n", var_name.c_str());
	//改为栈分配后，所有栈变量都以其所在的地址表示。返回值需要load一次。
	return ir_builder.CreateLoad(V, var_name.c_str());
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
	Function *user_func;
	const string* op_external_name;
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
		case BINARY_USER_DEFINED:
			op_external_name = &(bin->get_op_external_name());
			user_func = the_module->getFunction(*op_external_name);
			//parser就应该发现未定义的问题，这里再未定义应该是致命异常
			if (user_func == nullptr)
				err_print(true, "can not find prototype of binary operator %s,"
				"aborting\n", op_external_name->c_str()); 

			return ir_builder.CreateCall(user_func, {lhs, rhs}, 
				op_external_name->c_str());
		case BINARY_UNKNOWN:
		default:
			err_print(true, "unknown binary op, aborting\n");
	}
}


Value* LLVM_IR_code_generator::build_unary_op(const unary_operator_ast* unary)
{
	//目前都是自定义的uanry，暂时不做通用的流程
	auto operand = build_expr(unary->get_operand().get());
	print_and_return_nullptr_if_check_fail(operand != nullptr,
		"failed build operand of unary operator\n");

	const string& op_external_name = unary->get_op_external_name();
	Function* user_func = the_module->getFunction(op_external_name);

	//parser就应该发现未定义的问题，这里再未定义应该是致命异常
	if (user_func == nullptr)
		err_print(true, "can not find prototype of unary operator %s,"
			"aborting\n", op_external_name.c_str()); 

	return ir_builder.CreateCall(user_func, {operand},
		op_external_name.c_str());
}


Value* LLVM_IR_code_generator::build_if(const if_ast* if_expr)
{
	Value *cond_val = build_expr(if_expr->get_cond().get());
	print_and_return_nullptr_if_check_fail(cond_val != nullptr,
		"can not build condition expr for if\n");

	// Convert condition to a bool by comparing non-equal to 0.0.
	cond_val = ir_builder.CreateFCmpONE(cond_val,
		ConstantFP::get(the_context, APFloat(0.0)), "ifcond");

	//我们保存到当前正在编译的函数指针在cur_func中，无需下面的语句
	//Function *TheFunction = Builder.GetInsertBlock()->getParent();
/*
下面代码片段的基本思路是先创建三个bb：
THEN_BB:
	执行then分支的expr计算，获得val_then
	goto MERGE_BB;
ELSE_BB:
	执行else分支的expr计算，获得val_else
	fallthrough到下一个BB(由于llvm IR要求所有BB必须以明确的跳转所以这里实际上还是需要一个branch IR指令)
MERGE_BB:
	final_if_val = PHI(val_then, val_else)

然后在THEN_BB的前面(也就是创建3个BB前的当前插入点)，
插入一个条件跳转语句根据cond_val跳入THEN或者ELSE的入口。
整个IF表达式的结果完成了，其值就是final_if_val。
其中，最后一步中的PHI是一个虚拟的函数，其逻辑语义是：
如果控制流从THEN_BB中来，返回val_then
如果控制流从ELSE_BB中来，返回val_else
PHI操作是完成IF逻辑的关键点。在具体实现时，
编译器是通过将final_if_val，val_then，val_else指向同一个寄存器
或者内存区域来完成这个逻辑语义的。

*/
// Create blocks for the then and else cases.  Insert the 'then' block at the
// end of the function.
/*
注意then_bb直接插入cur_func了，其余的两个bb还未与cur_func关联。
猜测原因是then表达式展开时可能生成新的BB（如递归if），如果我们在这里
把else_bb和merge_bb都插入cur_func函数中，那么then_bb中生成的新bb将会
被else_bb和merge_bb隔开（在function的bblist上）。按理说，bblist链表
的顺序并不是很重要，因为bb间的依赖关系是由跳转语句决定的。bblist链表中
的顺序对程序语义没有影响。从性能上看，打开优化后，bb的顺序本来就要重排，
应该也没有影响。
实际测试(直接在这里插入else_bb和merge_bb)显示功能逻辑确实没有区别。
但是，考虑到无论如何bblist有序是更好的，所以维持原示例的做法。
*/
	BasicBlock *then_bb = BasicBlock::Create(the_context, "then", cur_func);
	BasicBlock *else_bb = BasicBlock::Create(the_context, "else");
	BasicBlock *merge_bb = BasicBlock::Create(the_context, "if_final");
//创建条件跳转
	ir_builder.CreateCondBr(cond_val, then_bb, else_bb);

	// emit then_bb中的expr计算指令获取其val
	ir_builder.SetInsertPoint(then_bb);
	Value* then_val = build_expr(if_expr->get_then().get());
	print_and_return_nullptr_if_check_fail(then_val != nullptr,
		"can not build then expr for if\n");
	ir_builder.CreateBr(merge_bb);
/*
	这里重新取then_bb非常重要，因为后面的PHI操作需要明确输入的bb。
	而这里then_bb可能不再与then_val对应的了，如下示例所示。
then_bb:
	expr_eval()	---------------->  eval_bb:
																|
																|
then_final_bb:<--------------------|
	jmp to merge_bb

	关键就在then_val = build_expr(xxx)这句，它可能会创建新的bb。
	因此，我们需要build_expr后，重新设置then_bb到then_final_bb上。
*/
	then_bb = ir_builder.GetInsertBlock();

	//同样处理else分支
	cur_func->getBasicBlockList().push_back(else_bb);
	ir_builder.SetInsertPoint(else_bb);
	Value* else_val = build_expr(if_expr->get_else().get());
	print_and_return_nullptr_if_check_fail(else_val != nullptr,
		"can not build then expr for if\n");
	ir_builder.CreateBr(merge_bb);
// Codegen of 'else' can change the current block, update else_bb for the PHI.
	else_bb = ir_builder.GetInsertBlock();

	// 生成Merge_bb的指令
	cur_func->getBasicBlockList().push_back(merge_bb);
	ir_builder.SetInsertPoint(merge_bb);
	PHINode* PHI_node =
	ir_builder.CreatePHI(Type::getDoubleTy(the_context), 2, "if_phi");
	PHI_node->addIncoming(then_val, then_bb);
	PHI_node->addIncoming(else_val, else_bb);

	return PHI_node;
}

#if 0
/*
原示例的实现逻辑有问题（或者违背了常规设计）。
它是先执行body再来检查end条件是否满足。
这导致下面的循环也会执行一次kout，打印出1来。
很显然这个逻辑与常规语言的for是不一致的。
for x=1:x<1 in
        kout(x)
*/
Value* LLVM_IR_code_generator::build_for(const for_ast* for_expr)
{
	//先发射计算start value的指令，注意此时还未将变量名称注册到查找map中
	Value* start_val = build_expr(for_expr->get_start().get());
	print_and_return_nullptr_if_check_fail(start_val != nullptr, 
		"can not build start value in for_expr\n");

	//新建循环体的入口bb
	BasicBlock* preheader_bb = ir_builder.GetInsertBlock();
	BasicBlock* loop_bb = BasicBlock::Create(the_context, "loop", cur_func);
	// Insert an explicit fall through from the current block to the LoopBB.
	ir_builder.CreateBr(loop_bb);

	// Start insertion in LoopBB.
	ir_builder.SetInsertPoint(loop_bb);
	/* 
	在循环体中，induction var(指示变量)有两个可能的值：
	第一次进入时是start value；
	多次循环时，其值由本次循环体执行完后指示变量名指向的value给出
	这里，所以需要用PHI节点来表示指示变量
	*/
	const string& idt_name = for_expr->get_idt_name();
	PHINode* idt_var = ir_builder.CreatePHI(Type::getDoubleTy(the_context),
										2, for_expr->get_idt_name().c_str());
	idt_var->addIncoming(start_val, preheader_bb);

/*
从这里开始，下面的流程再引用idt_var这个名称，就应该去读取PHI的值。
如果有重名的情况，当前for中的变量名生效。
但是离开当前for的作用域后，需要还原原来的变量value，所以需要save一下。
*/
	Value* old_val = named_var[idt_name];
	named_var[idt_name] = idt_var;

	//for body的value没有被定义，只要不为空表示没有错误就可以
	Value* body = build_expr(for_expr->get_body().get());
	print_and_return_nullptr_if_check_fail(body != nullptr, 
		"can not build bodyin for_exp\n");
	
	//body发射完才是step
	Value* step_val = nullptr;
	if (for_expr->get_step())	//step可选，不设置就是1.0
	{
		step_val = build_expr(for_expr->get_start().get());
		print_and_return_nullptr_if_check_fail(step_val != nullptr, 
			"can not build step value in for_expr\n");
	}
	else 
		step_val = ConstantFP::get(the_context, APFloat(1.0));

	Value* next_idt_val = ir_builder.CreateFAdd(idt_var, step_val, "nextvar");

// Compute the end condition.
	Value* end_cond = build_expr(for_expr->get_end().get());
	print_and_return_nullptr_if_check_fail(end_cond != nullptr,
		"can not build end expr in for_exp\n");

// Convert condition to a bool by comparing non-equal to 0.0.
	end_cond = ir_builder.CreateFCmpONE(
		end_cond, ConstantFP::get(the_context, APFloat(0.0)), "loopcond");

// Create the "after loop" block and insert it.
	BasicBlock* loop_end_bb = ir_builder.GetInsertBlock();
	BasicBlock* after_loop_bb =
		BasicBlock::Create(the_context, "afterloop", cur_func);
//满足循环结束条件fallthrough到after_loop_bb，否则回到循环体开始
	ir_builder.CreateCondBr(end_cond, loop_bb, after_loop_bb);

	//设置插入点到after_loop_bb，后续指令发射就到循环后面了
	ir_builder.SetInsertPoint(after_loop_bb);

	//设置示变量(PHI节点)的另外一个入口和值
	idt_var->addIncoming(next_idt_val, loop_end_bb);

	// Restore the unshadowed variable.
	if (old_val)
		named_var[idt_name] = old_val;
	else
		named_var.erase(idt_name);

	// for expr always returns 0.0.
	return Constant::getNullValue(Type::getDoubleTy(the_context));
}
#endif

Value* LLVM_IR_code_generator::build_for(const for_ast* for_expr)
{
	//先发射计算start value的指令，注意此时还未将变量名称注册到查找map中
	Value* start_val = build_expr(for_expr->get_start().get());
	print_and_return_nullptr_if_check_fail(start_val != nullptr, 
		"can not build start value in for_expr\n");
	/* 
	induction var(指示变量)有两个可能的值：
	第一次进入时是start value；
	多次循环时，其值由本次循环体执行完后指示变量名指向的value给出
	在采用stack地址来表达变量后，无需再用PHI节点表达这两种可能性了。
	因为地址中存放的值本来就是可以有多种的，只需要在用的时候存取就可以了。
	*/
	const string& idt_name = for_expr->get_idt_name();
	AllocaInst * idt_var = create_alloca_at_func_entry(cur_func, idt_name);
	ir_builder.CreateStore(idt_var, start_val);

/*
创建各个基础框架bb，他们的作用分区和作用如下 ： 
preheader_bb:
	计算induction var(指示变量)的初始值
	goto to end_check
end_check:
	计算end expr的值end_val是否为true
	if (end_val)
		goto loop
	else
		goto afterloop

loop:
	计算循环体body expr的值
	induction var += step
	goto end_check

after_loop:
	xxxx后续指令
*/
//	BasicBlock* preheader_bb = ir_builder.GetInsertBlock(); 无PHI不再需要
	BasicBlock* end_check_bb =
		BasicBlock::Create(the_context, "end_check", cur_func);
//参考if的做法，为保持bblist中的顺序一致性，下面两个bb创建后不插入func。
	BasicBlock* loop_bb = BasicBlock::Create(the_context, "loop");
	BasicBlock* after_loop_bb = BasicBlock::Create(the_context, "afterloop");

	//创建preheader到end_check的连接(注意插入点现在还在preheader里)
	ir_builder.CreateBr(end_check_bb);

//现在开始构建循环结束判断bb：
	ir_builder.SetInsertPoint(end_check_bb);

	/*
	发射完start计算后，后续流程再引用idt_name这个名称，就应该
	去读取for中的定义。	但是离开当前for的作用域后，
	需要还原原来的变量value，所以需要save一下。
	*/
	AllocaInst* old_val = nullptr;
	if (auto it = named_var.find(idt_name); it != named_var.end())
		old_val = it->second;
	//从这里开始idt_name这个名称指向for中的定义
	named_var[idt_name] = idt_var;

	// Compute the end condition.
	Value* end_cond = build_expr(for_expr->get_end().get());
	print_and_return_nullptr_if_check_fail(end_cond != nullptr,
		"can not build end expr in for_exp\n");
	//这里end_cond被改写成bool
	end_cond = ir_builder.CreateFCmpONE(
		end_cond, ConstantFP::get(the_context, APFloat(0.0)), "loopcond");
	//循环持续条件为true则跳转到循环体，否则跳出循环
	ir_builder.CreateCondBr(end_cond, loop_bb, after_loop_bb);

//现在开始构建循环body：
	//注意先挂上头部的loop_bb确保中途生成的bb跟在其后
	cur_func->getBasicBlockList().push_back(loop_bb);
	ir_builder.SetInsertPoint(loop_bb);

	//for body的value没有被语言定义，只要不为空表示没有错误就可以
	Value* body = build_expr(for_expr->get_body().get());
	print_and_return_nullptr_if_check_fail(body != nullptr, 
		"can not build bodyin for_exp\n");
	
	//body发射完才是step
	Value* step_val = nullptr;
	if (for_expr->get_step())	//step可选，不设置就是1.0
	{
		step_val = build_expr(for_expr->get_start().get());
		print_and_return_nullptr_if_check_fail(step_val != nullptr, 
			"can not build step value in for_expr\n");
	}
	else 
		step_val = ConstantFP::get(the_context, APFloat(1.0));

	//使用stack来记录和更新idt变量
	Value* idt_var_val = ir_builder.CreateLoad(idt_var);
	Value* next_idt_val = ir_builder.CreateFAdd(idt_var_val, step_val, "nextvar");
	ir_builder.CreateStore(next_idt_val, idt_var);

	ir_builder.CreateBr(end_check_bb);

//设置插入点到after_loop_bb，后续指令发射就到循环后面了
	cur_func->getBasicBlockList().push_back(after_loop_bb);
	ir_builder.SetInsertPoint(after_loop_bb);

	// Restore the unshadowed variable.
	if (old_val != nullptr)
		named_var[idt_name] = old_val;
	else
		named_var.erase(idt_name);

	// for expr always returns 0.0.
	return Constant::getNullValue(Type::getDoubleTy(the_context));
}

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
AllocaInst* LLVM_IR_code_generator::create_alloca_at_func_entry(Function* func,
                                          const std::string& var_name)
{
	IRBuilder<> tmp_builder (&func->getEntryBlock(),
		func->getEntryBlock().begin());
	return tmp_builder.CreateAlloca(Type::getDoubleTy(the_context), 0,
		var_name.c_str());
}

void LLVM_IR_code_generator::print_IR()
{
	the_module->print(outs(), nullptr);
}

void LLVM_IR_code_generator::print_IR_to_str(string& out)
{
	raw_string_ostream out_stream(out);
	the_module->print(out_stream, nullptr);
	out_stream.flush();
}

void LLVM_IR_code_generator::print_IR_to_file(int fd)
{
	raw_fd_ostream out_stream(fd, /*shouldClose*/false);
	the_module->print(out_stream, nullptr);
}

void LLVM_IR_code_generator::print_IR_to_file(string& filename)
{
	std::error_code err;		//这样初始化errcode=0
	raw_fd_ostream out_stream(filename, err);
	if (err)	//err转bool就看里面有设置errcode
	{
		err_print(false, "can not print to %s, reason:%s\n", 
			filename.c_str(), err.message().c_str());
		return;
	}
	the_module->print(out_stream, nullptr);
}



}	//end of toy_compiler