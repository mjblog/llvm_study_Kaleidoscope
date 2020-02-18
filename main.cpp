#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "llvm_ir_codegen.h"
#include "llvm_optimizer.h"
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
	t_parser.parse();
	const auto& ast_vec = t_parser.get_ast_vec();
	LLVM_IR_code_generator code_generator;
	code_generator.codegen(ast_vec);
	Module* module = code_generator.get_module();
	llvm_optimizer::optimize_module(*module);
	code_generator.print_IR();
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
	t_parser.parse();
	const auto& ast_vec = t_parser.get_ast_vec();
	LLVM_IR_code_generator code_generator;
	code_generator.codegen(ast_vec);
	Module* module = code_generator.get_module();
	llvm_optimizer::optimize_module(*module);
	string outfile = infile + string(".ll");
	code_generator.print_IR_to_file(outfile);
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