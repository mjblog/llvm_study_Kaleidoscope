#ifndef _LLVM_OPTIMIZER_H_
#define _LLVM_OPTIMIZER_H_

#include "codegen.h"
#include "ast.h"

#include "llvm/IR/Type.h"
#include "llvm/IR/PassManager.h"	/*for buildPerModuleDefaultPipeline*/
#include "llvm/IR/Verifier.h"
#include "utils.h" /* for err_print*/

namespace toy_compiler{
using namespace llvm;
class llvm_optimizer final
{
/*
这里的返回值意义与llvm的optimizer对齐，新版本的PassManager不再返回bool
*/
public:
	static void optimize_module(Module& mod, int opt_level = 2);
	static void optimize_function(Function& func);
};
/*
class MAPLE_IR_code_generator : public code_generator
{

};
*/
}   // end of namespace toy_compiler

#endif