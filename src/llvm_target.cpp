#include "llvm/Support/Host.h"  //for native target
#include "llvm/Support/TargetRegistry.h"    //for lookupTarget
#include "llvm/Support/TargetSelect.h" 	//for InitializeNativeTarget...
#include "llvm/Target/TargetOptions.h"	//for TargetOptions
#include "llvm/CodeGen/CommandFlags.inc"	//for InitTargetOptionsFromCodeGenFlags
#include "llvm/IR/PassManager.h"
#include "llvm_target.h"
#include "utils.h" /* for err_print*/

namespace toy_compiler{
using namespace llvm;
using namespace std;

//为了简单，我们当前只支持本地机器
TargetMachine* llvm_target::get_native_target()
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

/*
class MAPLE_IR_code_generator : public code_generator
{

};
*/
}   // end of namespace toy_compiler
