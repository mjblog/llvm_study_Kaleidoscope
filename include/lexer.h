#ifndef _LEXER_H_
#define _LEXER_H_
#include <iostream>
#include <fstream>
#include <string>
#include "utils.h" /* for err_print*/

namespace toy_compiler{

enum token_type
{
	TOKEN_DEF,
	TOKEN_EXTERN,
	TOKEN_IDENTIFIER,
	TOKEN_NUMBER,
	TOKEN_LEFT_PAREN,
	TOKEN_RIGHT_PAREN,
	TOKEN_BINARY_OP,
	TOKEN_IF,
	TOKEN_THEN,
	TOKEN_ELSE,
	TOKEN_EOF,
	TOKEN_WRONG
};

typedef enum token_type token_type_t;
class token
{
	std::string raw_str;
/*
	丢弃了原示例中的NumVal，token应该只管str级别的数据
	另外由于单个token只用一次，用的时候再转也并不会影响效率
*/
public:
	token_type_t type;
	inline token_type_t get_type() const {return type;}
	static inline token_type_t identifier_string_to_type (const std::string &input)
	{
		if (input == "def")
			return TOKEN_DEF;

		if (input == "extern")
			return TOKEN_EXTERN;

		if (input == "if")
			return TOKEN_IF;
		if (input == "then")
			return TOKEN_THEN;
		if (input == "else")
			return TOKEN_ELSE;

		//关键字排除后，作为名称标识
		return TOKEN_IDENTIFIER;
	}
	inline std::string & get_str() {return raw_str;}
	/*
	提供下面的const重载版本，以便一个const的token可以使用get_str方法；
	否则会遇到error: passing xxx as 'this' argument of xxx discards qualifiers
	*/
	inline const std::string & get_str() const {return raw_str;}
	inline const char* get_cstr() const {return raw_str.c_str();}
/*重载了token到type的转换后就不再需要重载==之类
	inline bool operator == (token_type_t right_val)
	{
		if (type == right_val)
			return true;
		else
			return false;
	}
	inline bool operator != (token_type_t right_val)
	{
		if (type != right_val)
			return true;
		else
			return false;
	}
*/
	inline operator token_type_t() const  {return type;}
};

class lexer
{
	std::istream *input_stream = nullptr;
	std::string error_msg;
	token  cur_token;
	static inline int get_next_char(std::istream *in)
	{
		return in->get(); 
	}
	static inline void skip_spaces(std::istream *in, int & cur_char)
	{
		if (isspace(cur_char))
		{
			do
			{
				cur_char = get_next_char(in);
			}while (isspace(cur_char));
		}
	}
	static inline void process_comments(std::istream *in, int & cur_char)
	{
		if (cur_char == '#')
		{
			do
			{
				cur_char = get_next_char(in);
			}while (cur_char != EOF && cur_char != '\r' && cur_char != '\n' );
		}
	}

	inline bool get_identifier(std::istream *in, int & cur_char)
	{
		if (isalpha(cur_char))
		{
			std::string& cur_str = cur_token.get_str();
			cur_str.clear();
			cur_str += cur_char;
			cur_char = get_next_char(in);
			while (isalnum(cur_char))
			{
				cur_str += cur_char;
				cur_char = get_next_char(in);
			}
			cur_token.type = cur_token.identifier_string_to_type(cur_str);
			return true;
		}
		else
			return false;
	}

	inline bool get_number(std::istream *in, int & cur_char)
	{
		//改进原示例，数字不能以 ' . ' 开头，只能出现一次' . ' 
		if (isdigit(cur_char))
		{
			std::string& cur_str = cur_token.get_str();
			cur_token.type = TOKEN_NUMBER;
			cur_str.clear();
			cur_str += cur_char;
			cur_char = get_next_char(in);
			bool seen_dot = false;
			while (isdigit(cur_char) || (cur_char == '.'))
			{
				if (cur_char == '.') 
				{
					if (seen_dot)
						err_print(false,  "a number shoud not contain multiple dots\n");
					else
						seen_dot = true;
				}

				cur_str += cur_char;
				cur_char = get_next_char(in);
			}
		}
		else
			return false;

		return true;
	}

	inline bool get_paren(std::istream *in, int& cur_char)
	{
		if (cur_char == '(')
		{
			cur_token.get_str().clear();
			cur_token.get_str() = "(";	//解析不需要，但是错误打印可能需要
			cur_token.type = TOKEN_LEFT_PAREN;
			cur_char = get_next_char(in);
			return true;
		}

		if (cur_char == ')')
		{
			cur_token.get_str().clear();
			cur_token.get_str() = ")";
			cur_token.type = TOKEN_RIGHT_PAREN;
			cur_char = get_next_char(in);
			return true;
		}

		return false;
	}

	inline bool get_binary_op(std::istream *in, int& cur_char)
	{
		if (cur_char == '+' || cur_char == '-' || cur_char == '*'
			|| cur_char == '<' )
		{
			cur_token.get_str().clear();
			cur_token.get_str() = cur_char;
			cur_token.type = TOKEN_BINARY_OP;
			cur_char = get_next_char(in);
			return true;
		}
		else
			return false;
	}

	//从输入流中取数据，解析好后放入cur_token中
	void update_cur_token()
	{
		/*
		语义上cur_char指向待解析token的第一个字符。
		但是，在初次进入时这个值是未知的，我们用一个无害的空格作为开始
		*/
		static int cur_char = ' ';
		while (cur_char != EOF)
		{
			process_comments(input_stream, cur_char);
			skip_spaces(input_stream, cur_char);
			//尝试解析当前token为关键字或者变量
			if (get_identifier(input_stream, cur_char))
				return;
			//尝试解析当前token为数字
			if (get_number(input_stream, cur_char))
				return;
			//尝试解析'('、')'
			if (get_paren(input_stream, cur_char))
				return;

			//尝试解析二元操作符
			if (get_binary_op(input_stream, cur_char))
				return;

			//上面模式处理可能会把cur_char更新为eof，先判断再做非法告警
			if (cur_char != EOF)
			{
				//所有合法的模式走完，报错后，吃掉当前char继续
				err_print(/*isfatal*/false, "unknown char %c\n", cur_char);
				cur_char = get_next_char(input_stream);
			}
		}
/*
到这里一定是eof了，lexer本次工作结束了。
为了lexer还能再次重新工作，这里把static的cur_char重新设置为' '。
否则下次初始化后cur_char还是eof，流程无法向前了。
*/
		cur_token.get_str().clear();
		cur_token.type = TOKEN_EOF;
		cur_char = ' ';
	}

public:
		bool is_ok = true;
		inline const token & get_cur_token() const {return cur_token;}
		inline const token & get_next_token()
		{
			update_cur_token();
			return cur_token;
		}

		lexer(const std::string & filename)
		{
			auto *fstream  = new std::ifstream;
			fstream->open(filename, std::ios_base::in);
			if (fstream->is_open())
			{
				input_stream = fstream;
			}
			else
			{
				error_msg = "can not open input file" + filename;
				is_ok = false;
			}
		}
		//通常情况下应该只有测试流程会用该种初始化
		lexer()
		{
			input_stream = &std::cin;
		}
		~lexer()
		{
			//依赖stream的析构close
			if (input_stream != &std::cin && input_stream)
				delete input_stream;
		}
};

}   // end of namespace toy_compiler

#endif