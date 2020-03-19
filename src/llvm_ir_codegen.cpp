#include <cassert>
#include <memory>
#include <filesystem>

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

/*
调试信息应该用选项控制，可在constructor中控制debug_info的初始化
push scope的动作必须在args的生成动作之前，因为现在入参也有store指令。
如果为了debug代码合并到一块，args的store指令调试信息指向的scope会
是错误的上一个函数。为了保持verifyFunction的正常工作，
还是将调试信息的发射代码拆分为两块。scope的创建和push提前。
*/
	if (debug_info)
	{
		auto dbg_builder = debug_info->DBuilder;
		auto cu = debug_info->compile_unit;
/*
fixme!!Unit 似乎不应该重复分配？如果同一个compile_unit中
出现多个文件，如C中的#include，这里应该从ast中取filename和dir。
*/
		DIFile* unit = dbg_builder->createFile(
			cu->getFilename(), cu->getDirectory());
		DIScope* fun_context = unit;
		DIType* double_type = debug_info->double_type;
		unsigned line_no = proto_ptr->get_line();
		unsigned scope_line = line_no;
		auto CreateFunctionType = 
			[double_type, dbg_builder] (unsigned num_args) 
		{
			SmallVector<Metadata *, 8> EltTys;
			// Add the result type.
			EltTys.push_back(double_type);

			for (unsigned i = 0, e = num_args; i != e; ++i)
				EltTys.push_back(double_type);

			return dbg_builder->createSubroutineType(
				dbg_builder->getOrCreateTypeArray(EltTys));
		};

		DISubprogram *sub_prog = dbg_builder->createFunction(
			fun_context, proto_ptr->get_name(), StringRef(), unit, line_no,
			CreateFunctionType(cur_func->arg_size()), scope_line,
			DINode::FlagPrototyped, DISubprogram::SPFlagDefinition);
		cur_func->setSubprogram(sub_prog);
		// Push the current scope.
		debug_info->lexical_blocks.push_back(sub_prog);
/*
我们不想要调试器跳过prologue，所以删去了原示例的下面片段
  // Unset the location for the prologue emission (leading instructions with no
  // location in a function are considered part of the prologue and the debugger
  // will run past them when breaking on a function)
  KSDbgInfo.emitLocation(nullptr);
但是这里必须发射一次，否则ir_builder看到的scope还没改过来
*/
		emit_location(proto_ptr->get_loc());
	}


	//创建args查找map，方便后续variable引用
	named_var.clear();
	for (auto &arg : cur_func->args())
	{
		const string& arg_name = arg.getName().str();
		auto arg_alloca = create_alloca_at_func_entry(cur_func, arg_name);
		ir_builder.CreateStore(&arg, arg_alloca);
		named_var[arg_name] = arg_alloca;
	}

	//为所有的入参准备调试信息
	if (debug_info)
	{
		auto dbg_builder = debug_info->DBuilder;
		auto sub_prog = cur_func->getSubprogram();
		auto line_no = sub_prog->getLine();
		auto unit = sub_prog->getFile();
		auto double_type = debug_info->double_type;
		unsigned int arg_idx = 0;	//arg_idx不能放到for里面，否则始终为0
		for (const auto& arg : named_var)
		{
			auto& name = arg.first;
			auto& alloca = arg.second;
			// Create a debug descriptor for the variable.

			DILocalVariable *des = dbg_builder->createParameterVariable(
				sub_prog, name, ++arg_idx, unit, line_no, double_type, true);

			dbg_builder->insertDeclare(alloca, des,
				dbg_builder->createExpression(),
				DebugLoc::get(line_no, 0, sub_prog), 
				ir_builder.GetInsertBlock());
		}
	}

	//原示例在这里emitLocation(body)是冗余的，每一个ast自己会去emit

	//3 生成body
	//build_expr中会调用ir_builder插入计算expr结果的运算指令
	ret_val = build_expr(func->get_body().get());
	if (ret_val == nullptr)
	{
		err_print(false, "can not generate body for function %s\n",
			proto_ptr->get_name().c_str());
		//在有调试信息时，还需弹出debug 的scope
		goto err_exit_pop_debug_block;
	}

	assert(ir_builder.CreateRet(ret_val) != nullptr);
	
/*
我们的实现为了方便控制调试信息的发射，
将调试信息的发射拆分成了两块。args的调试信息在args的store指令之后发射。
这会导致下面错误的发生。
Expected no forward declarations!
!6 = <temporary!> !{}
  store double %x, double* %x1, !dbg !7
  store double %y, double* %y2, !dbg !7
  call void @llvm.dbg.declare(metadata double* %x1, metadata !8, metadata !DIExpression()), !dbg !9
  call void @llvm.dbg.declare(metadata double* %y2, metadata !10, metadata !DIExpression()), !dbg !9

使用 def foo (x y) x+y即可复现。
看起来dbg.declare需要放到对应store指令的前面。
不过，添加finalizeSubprogram可以自动更正错误。
*/
	if (auto sp = cur_func->getSubprogram(); sp != nullptr)
		debug_info->DBuilder->finalizeSubprogram(sp);

	//verifyFunction检查错误，没有错误时返回false
	if (verifyFunction(*cur_func, &errs()) != false)
	{
		//校验失败视为致命错误，打印IR帮助查看问题
		print_IR();
		abort();
	}

	//只要离开本函数，都应该把cur_func重新设置为空
	cur_func = nullptr;
	//弹出调试信息的scope
	if (debug_info)
		debug_info->lexical_blocks.pop_back();
	return true;

err_exit_pop_debug_block:
//弹出调试信息的scope
	if (debug_info)
		debug_info->lexical_blocks.pop_back();
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
		case VAR_AST:
			return build_var((const var_ast *)expr);
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
	emit_location(callee->get_loc());
	return ir_builder.CreateCall(callee_func, args_vec, "call" + callee_name);
}

Value* LLVM_IR_code_generator::build_number(const number_ast* num)
{
	emit_location(num->get_loc());
	//直接转换为llvm的值就可以了
	return ConstantFP::get(the_context, APFloat(num->get_val()));
}

//当前还未支持局部变量定义和全局变量定义，实际上variable就只有入参
Value* LLVM_IR_code_generator::build_variable(const variable_ast* var)
{
	emit_location(var->get_loc());
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
	if (bin->get_op() == BINARY_ASSIGN)
	{
		// 确保lhs变量存在.
		auto dest_var = (variable_ast *)bin->get_lhs().get();
		const auto& dest_var_name = dest_var->get_name();
		const auto& search_result = named_var.find(dest_var_name);
		print_and_return_nullptr_if_check_fail(
			search_result != named_var.cend(),
			"unknown variable name %s\n", dest_var_name.c_str());
		//生成rhs的值
		Value *val = build_expr(bin->get_rhs().get());
		print_and_return_nullptr_if_check_fail(val != nullptr, 
			"failed to build value for %s =\n", dest_var_name.c_str());
		//赋值的动作属于= operator，需要发射对应的调试信息位置
		emit_location(bin->get_loc());
		//写入rhs的值到lhs的变量中
		ir_builder.CreateStore(val, search_result->second);
		//返回rhs的值作为=表达式的返回值，以支持a=(b=c)这样的赋值
		return val;
	}

//除开=外的binary公用发射模式
	auto lhs = build_expr(bin->get_lhs().get());
	print_and_return_nullptr_if_check_fail(lhs != nullptr,
		"failed build lhs of binary operator\n");
	auto rhs = build_expr(bin->get_rhs().get());
	print_and_return_nullptr_if_check_fail(rhs != nullptr,
		"failed build lhs of binary operator\n");
	Value* cmp;
	Function *user_func;
	const string* op_external_name;
	//binary_op的操作只包含运算部分，调试信息起点在这里
	emit_location(bin->get_loc());
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

	//unaryop本身只有这条call语句
	emit_location(unary->get_loc());
	return ir_builder.CreateCall(user_func, {operand},
		op_external_name.c_str());
}


Value* LLVM_IR_code_generator::build_if(const if_ast* if_expr)
{

	Value *cond_val = build_expr(if_expr->get_cond().get());
	print_and_return_nullptr_if_check_fail(cond_val != nullptr,
		"can not build condition expr for if\n");
/*
上面的build_expr会改变loc，原示例中放函数头部会导致比较和跳转指向cond的loc。
下面的比较和跳转指令是属于if的指令，其loc应该是if表达式的start位置。
*/
	emit_location(if_expr->get_loc());
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
/*
fixme!!这里else_bb和merge_bb都还未挂入func链表，如果这里出错返回了。
会导致else_bb和merge_bb内存泄漏，后续应该考虑修复。
*/
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
/*
fixme!!!
本来这里应该有emit_location();来设置idt_var的位置。
但是idt_var不是ast，直接记录的是string。
考虑idt的声明和赋值一般都在同一行，暂时没有修改。
*/
	ir_builder.CreateStore(start_val, idt_var);
	//发射idt_var的调试信息声明
	if (debug_info)
	{
/*
fixme!!!
下面的算法还有一些小毛病，声明变量时scope用的是函数。
但语言中for声明的变量作用域并不是整个函数。
这可能会导致一些信息失配问题。
不过，扩大后var的作用域也仍在函数内(且对应存储也一定还在栈上)，
这样的失配应该不会导致严重的异常（如试图访问非法区域）。
所以暂时未新增scope的管理。
*/
		auto dbg_builder = debug_info->DBuilder;
		auto sub_prog = cur_func->getSubprogram();
		auto unit = sub_prog->getFile();
		auto double_type = debug_info->double_type;
		auto line_no = for_expr->get_start()->get_line();
		DILocalVariable *des = dbg_builder->createAutoVariable(
				sub_prog, idt_name, unit, line_no, double_type, true);

		dbg_builder->insertDeclare(idt_var, des,
				dbg_builder->createExpression(),
				DebugLoc::get(line_no, 0, sub_prog), 
				ir_builder.GetInsertBlock());
	}

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
	{
		old_val = it->second;
		it->second = idt_var;
	}
	else
		named_var[idt_name] = idt_var;
	//修改named_var后，idt_name这个名称现在指向for中的定义

	// Compute the end condition.
	Value* end_cond = build_expr(for_expr->get_end().get());
	print_and_return_nullptr_if_check_fail(end_cond != nullptr,
		"can not build end expr in for_exp\n");

/*
比较、跳转和操作idt 变量都是从属于for表达式的指令。
原示例放到函数头部去emit，会导致这些指令从属于cond等表达式。
*/
	emit_location(for_expr->get_loc());
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
		"can not build body of for_exp\n");
	
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

	//更新idt_var的动作属于for表达式
	emit_location(for_expr->get_loc());
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

/*
var表达式是用于声明变量的。
主要的逻辑包含如下部分：
1 发射计算变量初始值的代码
2 为声明的变量分配stack空间，并写入初始值
3 更新named_var，使得后续流程可以使用新声明的变量
4 发射body语句
5 移除本var声明的语句，恢复named_var中被重名覆盖的变量
*/
Value* LLVM_IR_code_generator::build_var(const var_ast* var_expr)
{
	vector<AllocaInst*> var_allocas;
	const vector<expr_t>& value_vec = var_expr->get_var_values();
	const vector<string>& name_vec = var_expr->get_var_names();
	assert(value_vec.size() == name_vec.size());
/*
根据语义定义，变量初始化时是立即进行shadow。
例如def ff (x) var x=2:y=x+1中，y的初始值应该是3。
实际测试c语言也是这样的语义逻辑。
*/

/*
map没有提供浅拷贝的实现。
如果不能接受对string的反复拷贝，只有缓存被shadow的条目。
这里直接记录了被shadow 变量的alloca地址，后续恢复时就无需再查找了。
*/
	vector<std::pair<AllocaInst **, AllocaInst *>> saved_name_vec;
	for (size_t i = 0; i < value_vec.size(); ++i)
	{
		Value* var_value = build_expr(value_vec[i].get());
		const string& var_name = name_vec[i];
		print_and_return_nullptr_if_check_fail(var_value != nullptr,
			"failed to get the start value of %s\n", var_name.c_str());
		auto var_alloca = create_alloca_at_func_entry(cur_func, name_vec[i]);
		print_and_return_nullptr_if_check_fail(var_alloca != nullptr,
			"failed to allocate stack for %s\n", var_name.c_str());
		//发射变量初始值的行号位置
		emit_location(value_vec[i]->get_loc());
		ir_builder.CreateStore(var_value, var_alloca);
		var_allocas.push_back(var_alloca);
		if (auto it = named_var.find(var_name); it != named_var.end())
		{
			saved_name_vec.push_back(
				std::make_pair(&(it->second), it->second));
			it->second = var_allocas[i];
		}
		else
			named_var[var_name] = var_allocas[i];
	}

	//如果需要发射调试信息，var中的局部变量需要声明
	if (debug_info)
	{
/*
fixme!!!
下面的算法还有一些小毛病，声明变量时scope用的是函数。
但语言中var声明的变量作用域并不是整个函数。
这可能会导致一些信息失配问题。
不过，扩大后var的作用域也仍在函数内(且对应存储也一定还在栈上)，
这样的失配应该不会导致严重的异常（如试图访问非法区域）。
所以暂时未新增scope的管理。
*/
		auto dbg_builder = debug_info->DBuilder;
		auto sub_prog = cur_func->getSubprogram();
		auto unit = sub_prog->getFile();
		auto double_type = debug_info->double_type;
		for (size_t i = 0; i < value_vec.size(); ++i)
		{
			auto line_no = value_vec[i]->get_line();
			DILocalVariable *des = dbg_builder->createAutoVariable(
				sub_prog, name_vec[i], unit, line_no, double_type, true);

			dbg_builder->insertDeclare(var_allocas[i], des,
				dbg_builder->createExpression(),
				DebugLoc::get(line_no, 0, sub_prog), 
				ir_builder.GetInsertBlock());
		}
	}

/*
原示例这里的emit_location无意义，build_expr进入会发射
自己的location位置。
*/
	auto body =  build_expr(var_expr->get_body().get());
	print_and_return_nullptr_if_check_fail(body != nullptr, 
		"failed to build body for var ast\n");

//恢复named_var
	for (size_t i = 0; i < saved_name_vec.size(); ++i)
	{
		auto old_alloca_addr = saved_name_vec[i].first;
		*old_alloca_addr = saved_name_vec[i].second;
	}
/*
fixme!! 
返回body的值作为var表达式的值，这个应该属于语义层面的定义。
但是目前还没很好的方法在parser和ast中去定义。
*/
	return body;
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

llvm_debug_info::llvm_debug_info(Module* mod, const string& source)
{
	DBuilder = new DIBuilder(*mod);
	namespace fs = std::filesystem;
	fs::path src_path(source);
	std::error_code ec;
	src_path = fs::canonical(src_path, ec);
	string src_dir = "_uninitilized_";
	string src_file = "_uninitilized_";
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

void LLVM_IR_code_generator::emit_location(const source_location& loc)
{
	if (!debug_info)
		return;

	DIScope* scope;

	if (debug_info->lexical_blocks.empty())
		scope = debug_info->compile_unit;
	else
		scope = debug_info->lexical_blocks.back();
	auto ast_line = loc.line;
	auto ast_col = loc.col;
	ir_builder.SetCurrentDebugLocation(
		DebugLoc::get(ast_line, ast_col, scope));

}

llvm_debug_info::~llvm_debug_info()
{
	delete DBuilder;
}

}	//end of toy_compiler