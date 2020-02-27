#include "llvm_optimizer.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Host.h"  //for native target
#include "llvm/Support/TargetRegistry.h"    //for lookupTarget
#include "llvm/Support/TargetSelect.h" 	//for InitializeNativeTarget...
#include "llvm/Target/TargetOptions.h"	//for TargetOptions
#include "llvm/CodeGen/CommandFlags.inc"	//for InitTargetOptionsFromCodeGenFlags
#include "llvm/Transforms/Scalar/Reassociate.h" //for ReassociatePass

#include "llvm/Transforms/Scalar/GVN.h"	//for createGVNPass
#include "llvm/Transforms/Scalar/SimplifyCFG.h" //for SimplifyCFGPass
#include "llvm/Transforms/Scalar.h"	//for createReassociatePass and createCFGSimplificationPass
#include "llvm/Transforms/InstCombine/InstCombine.h"	//for createInstructionCombiningPass

#include "llvm/Analysis/OptimizationRemarkEmitter.h" //for OptimizationRemarkEmitterAnalysis
/*
本文件用于建模调用LLVM的optimizer优化(codegen生成的)LLVM-IR的行为。
参考https://llvm.org/docs/WritingAnLLVMPass.html :
LLVM提供了多种类别的optimizer。
类别主要按照作用范围进行分类，例如ModulePass 可以修改整个Module中的数据，
FunctionPass就只能修改单个函数（就是它的入参）内的数据。

参考llvm/Passes/PassBuilder.h：
LLVM提供了多种预制的组合，将各个层级各种功能（pass不都是optimizer，
也有分析打印等）的pass有机串连起来，对外提供O2等标准优化包。

本文件实现了两个基本功能：
optimize_module 用于对当前module进行LLVM的O2优化
optimize_function 用于重现原示例中的简单函数级组合优化
*/
namespace toy_compiler{

//为了简单，我们当前只支持本地机器
TargetMachine * get_target_machine()
{
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();
/*
使用LLVM提供的接口，构建出native的target machine
参考了https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl08.html
*/
	const string& native_triple = sys::getDefaultTargetTriple();
	string tgt_err;
	const Target *tgt = TargetRegistry::lookupTarget(native_triple, tgt_err);
	assert(tgt != nullptr);
	//原示例代码有一些小问题，例如没有初始化Options，RM传参没意义；
	//所以从llvm的opt.cpp中抄写了下面部分，
	const TargetOptions Options = InitTargetOptionsFromCodeGenFlags();
	//before using getCPUStr() and getFeaturesStr() ，设置cpu
	MCPU = "native";

	TargetMachine * ret = tgt->createTargetMachine(native_triple, getCPUStr(), getFeaturesStr(), Options, None);
	assert(ret != nullptr);
	return ret;
}

void llvm_optimizer::optimize_module(Module& mod , int opt_level)
{
/*
使用PassBuilder提供的buildPerModuleDefaultPipeline接口,
提供优化级别O2，就可以获得我们需要的优化Pass组合。
初始化PassBuilder需要提供TargetMachine *作为入参(描述生成代码针对的架构机器)。
所以整个函数的流程就是：
先创建TargetMachine *；
再初始化PassBuilder；
最后调用PassBuilder.buildPerModuleDefaultPipeline()构建出ModulePassManager
调用ModulePassManager.run(mod)完成优化
*/

	TargetMachine * target_machine = get_target_machine();
	/*原示例第8章提到设置一下有助于优化*/
	mod.setDataLayout(target_machine->createDataLayout());
	mod.setTargetTriple(sys::getDefaultTargetTriple());

/*新的PassManger用法与原示例有较大区别，下面片段来自
	tools/opt/NewPMDriver.cpp */
	class PassBuilder opt_builder(target_machine);
	LoopAnalysisManager LAM;
	FunctionAnalysisManager FAM;
	CGSCCAnalysisManager CGAM;
	ModuleAnalysisManager MAM;


	// Register all the basic analyses with the managers.
	opt_builder.registerModuleAnalyses(MAM);
	opt_builder.registerCGSCCAnalyses(CGAM);
	opt_builder.registerFunctionAnalyses(FAM);
	opt_builder.registerLoopAnalyses(LAM);
	opt_builder.crossRegisterProxies(LAM, FAM, CGAM, MAM);
	//llvm的优化级别分别是O0到O3，再加上Os和Oz
	assert(0 <= opt_level && opt_level <= 5);
	llvm::PassBuilder::OptimizationLevel opt;
	switch(opt_level)
	{
		case 0: opt = llvm::PassBuilder::OptimizationLevel::O0; break;
		case 1: opt = llvm::PassBuilder::OptimizationLevel::O1; break;
		case 2: opt = llvm::PassBuilder::OptimizationLevel::O2; break;
		case 3: opt = llvm::PassBuilder::OptimizationLevel::O3; break;
		case 4: opt = llvm::PassBuilder::OptimizationLevel::Os; break;
		case 5: opt = llvm::PassBuilder::OptimizationLevel::Oz; break;
	}
	ModulePassManager mod_optimizer = 
		opt_builder.buildPerModuleDefaultPipeline(opt);
/*
新版本的PassManager没有提供doInitialization等方法，所以直接run
FPM.doInitialization();
FPM.run(*NonConstF);
FPM.doFinalization();
*/

	mod_optimizer.run(mod, MAM);
	delete target_machine;
	return;
}

void llvm_optimizer::optimize_function(Function& func)
{
/*
这里前面的PassBuilder没有直接用于构建func_optimizer.
但是没有其提供的registerFunctionAnalyses等接口注册分析pass，
直接运行func_optimizer会有运行错误。原因推测是这些优化pass需要
特定分析pass的支持(理论上这个要求是合理的，但需要进一步明确)。
参考NewPMDriver.cpp中注册全部分析pass的更为稳妥。
*/
	TargetMachine * target_machine = get_target_machine();
/*新的PassManger用法与原示例有较大区别，下面片段来自
	tools/opt/NewPMDriver.cpp*/
	class PassBuilder opt_builder(target_machine);
	LoopAnalysisManager LAM;
	FunctionAnalysisManager FAM;
	CGSCCAnalysisManager CGAM;
	ModuleAnalysisManager MAM;


	// Register all the basic analyses with the managers.
	opt_builder.registerModuleAnalyses(MAM);
	opt_builder.registerCGSCCAnalyses(CGAM);
	opt_builder.registerFunctionAnalyses(FAM);
	opt_builder.registerLoopAnalyses(LAM);
	opt_builder.crossRegisterProxies(LAM, FAM, CGAM, MAM);

/*
新版本的PassManager没有提供doInitialization等方法，所以直接run
FPM.doInitialization();
FPM.run(*NonConstF);
FPM.doFinalization();
*/
	FunctionPassManager func_optimizer;
	func_optimizer.addPass(InstCombinePass());
	func_optimizer.addPass(ReassociatePass());
	func_optimizer.addPass(GVN());
	func_optimizer.addPass(SimplifyCFGPass());
	func_optimizer.run(func, FAM);

	delete target_machine;//释放资源否则asan会报大量leak
}


#if 0
/*
下面函数基于试错注册了分析pass，也能完成优化功能。
但是参考llvm的代码，registerFunctionAnalyses除了注册pass外，
还调用了相关的回调函数。这样看起来，
正确的用法还是应该用用builder来注册所有分析pass。故而废弃下面实现。
*/
void llvm_optimizer::optimize_function_try_error(Function& func)
{
/*
fixme!!
如果没有设置Module的target和layout就直接调用本函数，可能会影响优化效果。
但是Function没有单独设置target和layout的接口，去修改Module就违背了分层语意。
一个比较好的方法，可能是要求调用者自己调用一些init函数，再来优化。
*/
	FunctionPassManager func_optimizer;
//采用的优化pass照抄原示例，以便进行对比
/*
	// Do simple "peephole" optimizations and bit-twiddling optzns.
	func_optimizer.addPass(createInstructionCombiningPass());
	// Reassociate expressions.
	func_optimizer.addPass(createReassociatePass());
	// Eliminate Common SubExpressions.
	func_optimizer.addPass(createGVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
	func_optimizer.addPass(createCFGSimplificationPass());
*/

	/*
	新接口的Passmanager需要注册一些分析的analysis再调用run。
	这种用法参考了lib/FuzzMutate/IRMutator.cpp中的eliminateDeadCode函数。
	具体注册的分析内容，是通过试错方式(没有会有运行错误)足以添加的。
	PassBuilder::registerFunctionAnalyses可以一次注册所有的分析pass，
	并且还会调用对应pass的callback，应该更为规范一些。
	*/
	FunctionAnalysisManager dummy_FAM;
//	PassBuilder::registerFunctionAnalyses(dummy_FAM);
	dummy_FAM.registerPass([&] { return AssumptionAnalysis(); });
	dummy_FAM.registerPass([&] { return DominatorTreeAnalysis(); });
	dummy_FAM.registerPass([&] { return TargetLibraryAnalysis(); });
	dummy_FAM.registerPass([&] { return OptimizationRemarkEmitterAnalysis(); });
	dummy_FAM.registerPass([&] { return LoopAnalysis(); });
	dummy_FAM.registerPass([&] { return AAManager(); });
	dummy_FAM.registerPass([&] { return MemoryDependenceAnalysis(); });
	dummy_FAM.registerPass([&] { return TargetIRAnalysis(); });
	
	func_optimizer.addPass(InstCombinePass());
	func_optimizer.addPass(ReassociatePass());
	func_optimizer.addPass(GVN());
	func_optimizer.addPass(SimplifyCFGPass());

	//从lib/Transforms/Scalar/LowerAtomic.cpp中的抄写，我们暂时不需要FAM的信息

	func_optimizer.run(func, dummy_FAM);
}

//下面函数使用了builder，但是无法自己定义优化所用的pass，故废弃。
void llvm_optimizer::optimize_function_use_builder(Function& func)
{
	TargetMachine * target_machine = get_target_machine();

/*新的PassManger用法与原示例有较大区别，下面片段来自
	tools/opt/NewPMDriver.cpp*/
	class PassBuilder opt_builder(target_machine);
	LoopAnalysisManager LAM;
	FunctionAnalysisManager FAM;
	CGSCCAnalysisManager CGAM;
	ModuleAnalysisManager MAM;


	// Register all the basic analyses with the managers.
	opt_builder.registerModuleAnalyses(MAM);
	opt_builder.registerCGSCCAnalyses(CGAM);
	opt_builder.registerFunctionAnalyses(FAM);
	opt_builder.registerLoopAnalyses(LAM);
	opt_builder.crossRegisterProxies(LAM, FAM, CGAM, MAM);

	auto opt = (llvm::PassBuilder::OptimizationLevel)2;
	FunctionPassManager func_optimizer = std::move(
		opt_builder.buildFunctionSimplificationPipeline(opt, llvm::PassBuilder::ThinLTOPhase::None));
/*
新版本的PassManager没有提供doInitialization等方法，所以直接run
FPM.doInitialization();
FPM.run(*NonConstF);
FPM.doFinalization();
*/

	func_optimizer.run(func, FAM);
}
#endif

}