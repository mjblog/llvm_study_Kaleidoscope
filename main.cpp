#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "llvm_ir_codegen.h"
using namespace toy_compiler;
int main()
{

    lexer t_lexer;
    parser t_parser(t_lexer);
    t_parser.parse();
    const auto& ast_vec = t_parser.get_ast_vec();
    LLVM_IR_code_generator code_generator;
    code_generator.codegen(ast_vec);
    code_generator.print_IR();
    return 0;
}