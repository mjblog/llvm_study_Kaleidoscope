#include "llvm_target.h"	//for  get_native_target
#include "llvm/IR/LegacyPassManager.h"	//for llvm::legacy::PassManager
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
/*
本文件用于将Module对应的IR生成到object文件中
*/
namespace toy_compiler{
using namespace std;

bool build_object(string& object_name, Module* module)
{
	std::error_code err;		//这样初始化errcode=0
	raw_fd_ostream out_stream(object_name, err);
	if (err)	//err转bool就看里面有设置errcode
	{
		err_print(false, "can not open %s, reason:%s\n", 
			object_name.c_str(), err.message().c_str());
		return false;
	}

	auto target = llvm_target::get_native_target();
/*
必须设置（尤其是setTargetTriple）。
在无优化情况下，module没有设置过TargetTriple，codegen会报错。
*/
	module->setDataLayout(target->createDataLayout());
	module->setTargetTriple(target->getTargetTriple().str());
	llvm::legacy::PassManager pass;
	auto FileType = CGFT_ObjectFile;

	if (target->addPassesToEmitFile(pass, out_stream, nullptr, FileType)) {
		errs() << "TargetMachine can't emit a file of this type";
		return false;
	}

	pass.run(*module);
	out_stream.flush();
	delete target;
	return true;
}


}