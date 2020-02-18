#include <sstream>
#include "lexer.h"
#include "parser.h"
namespace toy_compiler{
class prepare_parser_for_test_string
{
	stringstream test_input;
	streambuf *backbuf;
	lexer test_lexer;

public:
	parser test_parser;
	prepare_parser_for_test_string(const char * input) : 
		test_input(input), test_lexer(), test_parser(test_lexer)
	{
		backbuf = cin.rdbuf(test_input.rdbuf());
		test_parser.parse();
	}
	~prepare_parser_for_test_string()
	{
		cin.rdbuf(backbuf);
		//必须clear一下，否则前一个stream eof了，后面就无法再用了
		cin.clear();
	}
	auto& get_ast_vec() const {return test_parser.get_ast_vec();}
};

}
