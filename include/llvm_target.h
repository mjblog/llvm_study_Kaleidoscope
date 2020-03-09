#ifndef _LLVM_TARGET_H_
#define _LLVM_TARGET_H_
#include "llvm/Target/TargetMachine.h" 	//for InitializeNativeTarget...
#include "utils.h" /* for err_print*/

namespace toy_compiler{
using namespace llvm;
using namespace std;
class llvm_target final
{
/*
这里的返回值意义与llvm的optimizer对齐，新版本的PassManager不再返回bool
*/
public:
//为了简单，我们当前只支持本地机器
	static TargetMachine* get_native_target();
};
/*
class MAPLE_IR_code_generator : public code_generator
{

};
*/
}   // end of namespace toy_compiler

#endif