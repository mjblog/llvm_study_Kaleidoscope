#ifndef _LEXER_H_
#define _LEXER_H_
#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <unordered_map>
#include "utils.h" /* for err_print*/

namespace toy_compiler{
struct source_location
{
	std::string file_name;
	int64_t col;
	int64_t line;
public:
	source_location(const std::string& name, int64_t col = 0, int64_t line = 1)
		: file_name(name), col(col), line(line)
	{}
	source_location(): file_name(" "), col(-1), line(1) {}
};

/*
反汇编发现这里的enum类型使用了long表示。
我们在find_protected_char_token中建立了一个
256个元素的查找表，long会浪费很多空间。
这里用新的c++语法强制储存类型为char。
*/
typedef  enum token_type: unsigned char {
//0值设置为非法，find_protected_char_token未初始化的查找表项需要用到
	TOKEN_UNDEFINED = 0,
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
	TOKEN_FOR,
	TOKEN_COLON,
	TOKEN_IN,
	TOKEN_BINARY,
	TOKEN_UNARY,
	TOKEN_VAR,
	TOKEN_USER_DEFINED_BINARY_OPERATOR,
	TOKEN_USER_DEFINED_UNARY_OPERATOR,
	TOKEN_EOF,
	TOKEN_WRONG
} token_type_t;

class token
{
	std::string raw_str;
	source_location loc;
/*
	丢弃了原示例中的NumVal，token应该只管str级别的数据
	另外由于单个token只用一次，用的时候再转也并不会影响效率
*/
public:
	token_type_t type;
	inline token_type_t get_type() const {return type;}
	source_location& get_loc() {return loc;}
	const source_location& get_loc() const {return loc;}
	static inline token_type_t identifier_str_to_type(const std::string& input)
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

		if (input == "for")
			return TOKEN_FOR;

		if (input == "in")
			return TOKEN_IN;

		if (input == "binary")
			return TOKEN_BINARY;
		if (input == "unary")
			return TOKEN_UNARY;

		if (input == "var")
			return TOKEN_VAR;
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
	std::istream* input_stream = nullptr;
	std::string error_msg;
	token  cur_token;
	/*
	语义上cur_char指向待解析token的第一个字符。
	但是，在初次进入时这个值是未知的，我们用一个无害的空格作为开始
	*/
	int cur_char = ' ';
	source_location loc;
	inline int get_next_char(std::istream *in)
	{
		int new_char = in->get();
		if (new_char == '\r' || new_char == '\n')
		{
			++loc.line;
			loc.col = 0;
		}
		else
			++loc.col;
		
		return new_char;
	}

	inline void skip_spaces(std::istream *in, int & cur_char)
	{
		while (isspace(cur_char))
			cur_char = get_next_char(in);
	}

	bool process_comments(std::istream *in, int & cur_char)
	{
		if (cur_char == '#')
		{
			do
			{
				cur_char = get_next_char(in);
			}while (cur_char != EOF && cur_char != '\r' && cur_char != '\n' );
			return true;
		}
		return false;
	}

	inline bool get_identifier(std::istream *in, int & cur_char)
	{
		if (isalpha(cur_char))
		{
			auto start_loc = loc;
			std::string& cur_str = cur_token.get_str();
			cur_str.clear();
			cur_str += cur_char;
			cur_char = get_next_char(in);
			while (isalnum(cur_char))
			{
				cur_str += cur_char;
				cur_char = get_next_char(in);
			}
			cur_token.type = cur_token.identifier_str_to_type(cur_str);
			cur_token.get_loc() = start_loc;
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
			auto start_loc = loc;
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
			cur_token.get_loc() = start_loc;
		}
		else
			return false;

		return true;
	}

//解析单字符token包括 ：'(' 、')'  、':' 、'=' 和 builtin的单字符操作符
	inline bool get_protected_char(std::istream *in, int& cur_char)
	{
		token_type_t token_type = find_protected_char_token(cur_char);
		if (token_type != TOKEN_UNDEFINED)
		{
			auto start_loc = loc;
			cur_token.get_str().clear();
			cur_token.get_str() = cur_char;
			cur_token.type = token_type;
			cur_char = get_next_char(in);
			cur_token.get_loc() = start_loc;
			return true;
		}
		else
			return false;
	}

	std::unordered_map<std::string, token_type_t> user_defined_op;
	inline bool install_user_defined_operator(std::istream *in, int& cur_char)
	{
		auto start_loc = loc;
		std::string& cur_str = cur_token.get_str();
		if (cur_token.type == TOKEN_BINARY)
			cur_token.type = TOKEN_USER_DEFINED_BINARY_OPERATOR;
		else
		{
			assert(cur_token.type == TOKEN_UNARY);
			cur_token.type = TOKEN_USER_DEFINED_UNARY_OPERATOR;
		}
		
		cur_str.clear();
//要求binary和unary定义完成后一定要以空格分割。这是合理的要求。
		while (!isspace(cur_char) && (cur_char != EOF))
		{
			cur_str += cur_char;
			cur_char = get_next_char(in);
		}
		//可能会插入失败，这里不做检查
		user_defined_op.insert(make_pair(cur_str, cur_token.type));
		//正确性检查放到AST去做，更容易做错误处理，这里都返回成功。
		cur_token.get_loc() = start_loc;
		return true;
	}

/*
该函数的工作逻辑：
1 从instream中抽取字符直到遇到终止信号：字符、数字或者(
2 根据抽取出的string_op，查询user_defined_op是否存在定义
3 如果存在定义，就更新cur_token后返回true
4 如果不存在定义，将string_op尾部的char放回instream中，然后回到2
5 如果string_op已经为空，返回false
*/
	bool get_user_defined_operator(std::istream *in, int& cur_char)
	{
		auto start_loc = loc;
		std::string op_str;
		//遇到identifier、number、'('也需要停止，以便支持!x这样的写法
		while (!isalnum(cur_char) && !isspace(cur_char) 
			&& (cur_char != EOF) && cur_char != '(')
		{
			op_str += cur_char;
			cur_char = in->get();
/* 
使用裸的get，不去扰乱loc。operator一定不会导致换行。
最后只需要手动更新loc.col就可以了。
*/
			//cur_char = get_next_char(in);
		}

/*
走到这里后最终的loc只需考虑两种情况。
第一种是匹配到op：
如果cur_char是换行，则需要补偿换行动作，因为前面我们绕过了get_next_char。
end_loc.col = 1,  end_loc.line = start_loc.line + 1;
否则在同一行中移动
end_loc.col = start_loc.col + op_str.size();

第二种是没有匹配op，所有从stream中取出的字符都放回去了
end_loc = start_loc
*/
		while(op_str.size())
		{
			auto op = user_defined_op.find(op_str);
			if (op != user_defined_op.cend())
			{
				cur_token.type = op->second;
				cur_token.get_str() = op_str;
				cur_token.get_loc() = start_loc;
				//本次成功吃掉了op_str，手动更新col
				if (cur_char == '\r' || cur_char == '\n')
				{
					loc.col = 0;
					loc.line = start_loc.line + 1;
				}
				else
					loc.col = start_loc.col + op_str.size();
				return true;
			}
			else
			{
				in->putback(cur_char);
				//注意cur_char相当于insteam的单字节缓冲，需要一并更新
				cur_char = op_str.back();
				op_str.pop_back();
			}
		}
		//匹配失败，loc退回到开始的位置
		loc = start_loc;
		return false;
	}

	//从输入流中取数据，解析好后放入cur_token中
	void update_cur_token()
	{
		while (cur_char != EOF)
		{
			skip_spaces(input_stream, cur_char);
			//吃掉注释后，需要从新的行开始，吃掉可能存在的空白字符
			if (process_comments(input_stream, cur_char))
				continue;
//下面的解析顺序总体上需要遵从先长后短的规则
//这样当单个字符产生冲突时，长串才能获得正确的结果。
//如 = 和 != 需要先识别两字符的!=模式

			//尝试解析当前token为关键字或者变量
			if (get_identifier(input_stream, cur_char))
				return;
			//尝试解析当前token为数字
			if (get_number(input_stream, cur_char))
				return;

//operator可能为2字符，如==, !=, +=等。所以先于protected_char解析
			//允许将binary/unary关键字后的字符解析为自定义的operator
			if (cur_token == TOKEN_BINARY || cur_token == TOKEN_UNARY)
			{
				if (install_user_defined_operator(input_stream, cur_char))
					return;
			}

			if (get_user_defined_operator(input_stream, cur_char))
				return;

			//尝试解析保留的关键char，包括 '(' 、')'  、':' 、'='和builtin的操作符
			if (get_protected_char(input_stream, cur_char))
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
*/
		cur_token.get_str().clear();
		cur_token.type = TOKEN_EOF;
	}

public:
	bool is_ok = true;
	const source_location& get_loc() const {return loc;}
	inline const token & get_cur_token() const {return cur_token;}
	inline const token & get_next_token()
	{
		update_cur_token();
		return cur_token;
	}

	static token_type_t find_protected_char_token(char input)
	{
/*
字符判断是高频流程，使用数组性能更好。
下面的Designated initializers特性是C99的特性，当前C++还不支持(截止c++20)，
参考https://en.cppreference.com/w/cpp/language/aggregate_initialization。
clang作为一个c99的扩展支持了该特性。g++直到10版本都还未支持。
使用clang可以直接用下面的代码片段简洁地完成（尚未规避sign char可能导致的越界 ）。
		const static token_type_t search_tab[255] = 
		{

			['+'] = TOKEN_BINARY_OP,
			['-'] = TOKEN_BINARY_OP,
			['*'] = TOKEN_BINARY_OP,
			['<'] = TOKEN_BINARY_OP,

			['('] = TOKEN_LEFT_PAREN,
			[')'] = TOKEN_RIGHT_PAREN,
			[':'] = TOKEN_COLON,
			['='] = TOKEN_ASSIGN
		};
		return search_tab[(int)input];

为了使得代码兼容C++标准，下面的数组初始化通过c++标准
支持的constexpr方式规避。示例代码如下。
*/
		struct stupid_wrapper
		{
			token_type_t search_tab[256];
		};
		auto stupid_constructor = [] () constexpr {
			stupid_wrapper tmp = {
				.search_tab = {TOKEN_UNDEFINED}
			};
			int add_num = 0;
			if constexpr (std::is_signed<char>::value)
				add_num = 128;
			else
				add_num = 0;
			token_type_t* tab_start = &(tmp.search_tab[add_num]);
			tab_start[(int)'+'] = TOKEN_BINARY_OP;
			tab_start[(int)'-'] = TOKEN_BINARY_OP;
			tab_start[(int)'*'] = TOKEN_BINARY_OP;
			tab_start[(int)'<'] = TOKEN_BINARY_OP;
			tab_start[(int)'='] = TOKEN_BINARY_OP;

			tab_start[(int)'('] = TOKEN_LEFT_PAREN;
			tab_start[(int)')'] = TOKEN_RIGHT_PAREN;
			tab_start[(int)':'] = TOKEN_COLON;

			return tmp;
		};
		const static stupid_wrapper data = stupid_constructor();
		const token_type_t* search_tab;
		if constexpr (std::is_signed<char>::value)
			search_tab = &(data.search_tab[128]);
		else
			search_tab = &(data.search_tab[0]);
		
		return search_tab[int(input)];
	}

	lexer(const std::string& filename) : loc(filename)
	{
		auto *fstream  = new std::ifstream;
		//必须先赋值，否则打开失败的情况下析构无法释放fstream
		input_stream = fstream;
		fstream->open(filename, std::ios_base::in);
		if (!fstream->is_open())
		{
			error_msg = "can not open input file" + filename;
			is_ok = false;
		}
	}

	//通常情况下应该只有测试流程会用该种初始化
	lexer()	
		: input_stream(&std::cin), loc("_std::cin_")
	{}

	~lexer()
	{
		//依赖stream的析构close
		if (input_stream != &std::cin && input_stream)
			delete input_stream;
	}
	
/*
!!!fixeme为了将以库的方式实现的operator 声明植入。
这里提供了一个临时的入口，用于替换input_stream的buf。
注意这不应该是常规操作。
*/
	auto get_input_stream()
	{
		return input_stream;
	}
	//解析完builtin operator声明后，重置cur_char，重置source location
	void reset_lexer()
	{
		cur_char = ' ';
		loc.col = 0;
		loc.line = 1;
	}
};

static inline bool is_binary_operator_token(const token &in)
{
	if (in == TOKEN_BINARY_OP || in == TOKEN_USER_DEFINED_BINARY_OPERATOR)
		return true;
	else
		return false;
}

static inline bool is_unary_operator_token(const token &in)
{
	if (in == TOKEN_USER_DEFINED_UNARY_OPERATOR)
		return true;
	else
		return false;
}

static inline double get_double_from_number_token(const token& num_token)
{
	assert(num_token == TOKEN_NUMBER);
	double num_d;
	const std::string& num_str = num_token.get_str();
//使用异常返回错误感觉不如c的strtod简洁明了...
	try{
		num_d = std::stod(num_str);
	}
	catch (std::exception& exp)
	{
		num_d = 0;
		err_print(false, "fail to parse a number token %s, because of  \
			exception %s, set number to 0\n", num_str.c_str(), exp.what());
	}
	return num_d;
}

}   // end of namespace toy_compiler

#endif