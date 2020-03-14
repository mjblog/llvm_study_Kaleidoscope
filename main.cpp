#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "llvm_ir_codegen.h"
#include "llvm_optimizer.h"
#include "flags.h"
using namespace toy_compiler;
using namespace std;
static void print_help()
{
	cout << "use stdin as input , stdout as output: ./compile " << endl;
	cout << "use file_xx as input , file_xx.ll output: ./compile " << endl;
}

static void stdin_stdout_compile()
{
	lexer t_lexer;
	parser t_parser(t_lexer);
/*
prepare_builtin_operator是个临时解决方案，用于导入一系列
extern声明，使得用户可以直接使用以库方式实现的operator。
更完善的方案可能是实现c中的#include机制，把extern放到头文件中。
*/
	t_parser.prepare_builtin_operator();
	t_parser.parse();
	const auto& ast_vec = t_parser.get_ast_vec();
	LLVM_IR_code_generator code_generator;
	code_generator.codegen(ast_vec);
	Module* module = code_generator.get_module();
	if (!global_flags.optimization)
		llvm_optimizer::optimize_module(*module);
	code_generator.print_IR();
}

namespace toy_compiler{
extern bool build_object(string& object_name, Module* module);
}

static bool file_compile(const char* infile)
{
	lexer t_lexer(infile);
	if (!t_lexer.is_ok)
	{
		err_print(false, "can not open input %s\n", infile);
		return false;
	}
	parser t_parser(t_lexer);
/*
prepare_builtin_operator是个临时解决方案，用于导入一系列
extern声明，使得用户可以直接使用以库方式实现的operator。
更完善的方案可能是实现c中的#include机制，把extern放到头文件中。
*/
	t_parser.prepare_builtin_operator();
	t_parser.parse();
	const auto& ast_vec = t_parser.get_ast_vec();
	LLVM_IR_code_generator code_generator(infile);
	code_generator.codegen(ast_vec);
	Module* module = code_generator.get_module();
	if (!global_flags.optimization)
		llvm_optimizer::optimize_module(*module);
	string outfile = infile + string(".o");
	toy_compiler::build_object(outfile, module);
	if (global_flags.save_temps)
	{
		string out_llvm_ir_file = outfile + ".ll";
		code_generator.print_IR_to_file(out_llvm_ir_file);
	}
	return true;
}

int main(int argc, char* argv[])
{
	switch (argc)
	{
		case 1:
			stdin_stdout_compile();
			return 0;
		case 2:
			return file_compile(argv[1]);
			break;
		default:
			cout << "wrong input found, exiting\n" << endl;
			print_help();
			return 1;
	}

	return 0;
}