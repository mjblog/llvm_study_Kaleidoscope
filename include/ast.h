#ifndef _AST_H_
#define _AST_H_
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>
#include "lexer.h"
namespace toy_compiler{
using namespace std;
class generic_ast;
class expr_ast;
class prototype_ast;
using ast_vector_t = vector<shared_ptr<generic_ast>>;
using expr_vector = vector<shared_ptr<expr_ast>>;
using expr_t = shared_ptr<expr_ast>;
using prototype_t = shared_ptr<prototype_ast>;


/*目前只有三大类AST：
表达式                          expr_ast
函数原形                    prototype_ast
函数                            function_ast

表达式ast涵盖了变量定义，字面数字常量，运算等等
它是负责实际动作的主要大类。
函数原形prototype相当于是声明，其实也可以
看做是没有函数体的函数。
*/
typedef enum ast_type
{
	GENERIC_AST,
	EXPR_AST,
	PROTOTYPE_AST,
	FUNCTION_AST,
	NUMBER_AST,
	VARIABLE_AST,
	BINARY_OPERATOR_AST,
	CALL_AST,
	IF_AST,
	FOR_AST,
	UNKNOWN_AST
} ast_t;

/* 设计genrice_ast的目的是为每一个ast提供唯一的id和统一的type接口*/
class generic_ast
{
	uint64_t id = 0;
/*
created_ast_num是从0开始自增的整数，表达当前已经建立的ast数目。
一个简单直接的方案是写下面注释掉的 created_ast_num = 0;
但是编译时会有错误 ISO C++ forbids in-class initialization of non-const static member
参考https://stackoverflow.com/questions/20310000/error-iso-c-forbids-in-class-initialization-of-non-const-static-member
可以另外写一个ast.cpp,然后uint64_t  toy_compiler::generic_ast::created_ast_num= 0;
更简单的方案是把cur_ast_num放入static成员函数中，如下面代码所示。
*/
//static uint64_t created_ast_num = 0;
	uint64_t & get_created_ast_num()
	{
		static uint64_t created_ast_num = 0;
		return created_ast_num;
	}
public:
	virtual ~generic_ast() {}
	generic_ast()
	{
	/*
	ast_num的修改不考虑多线程并发，语法有前后相关性不太可能并发。
	修改也不必考虑整数溢出，64位整数溢出不可能发生，早就OOM了。
	为了保证唯一性，析构时不--
	*/
		id = get_created_ast_num()++;
		type = GENERIC_AST;
	}
	uint64_t get_id() const {return id;}
	void set_id(uint64_t in_id) {id = in_id;}
	ast_t get_type() const {return type;}
protected:
	ast_t type;
};


class expr_ast : public generic_ast
{
public:
	expr_ast()  {type = EXPR_AST;}
	virtual ~expr_ast() {}
};

/*
逻辑上看，用户自定义的operator应该作为prototype_ast的子类来建模。
但是由于operator的特有之处太少(就是多了一个优先级)，
专门定义一个class会引入额外的类型转换，不太值得。
所以还是维持原示例的设计，将operator放入prototype中。
*/
class prototype_ast : public generic_ast, 
	public enable_shared_from_this<prototype_ast>
{
	string name;
/* args理论上应该指向ast以表达类型信息。
这个玩具语言只有double型，所以用string也可以。 */
	vector<string> args;
	//为支持operator增加两个字段
	bool is_operator = false;
	int priority_for_binary = -1;
public:
	prototype_t get_shared_ptr()  {return shared_from_this();}
	prototype_ast(const string& name, vector<string> args,
		bool is_operator = false, int priority_for_binary = -1) :
		name(std::move(name)), args(std::move(args)),
		is_operator(is_operator), priority_for_binary(priority_for_binary)
		{
			type = PROTOTYPE_AST;
		}
	const string& get_name() const { return name; }
	const vector<string>& get_args() const {return args;}

/*
	由于操作符命名错误较为少见，且出错时通常会导致难以察觉的行为异常。
	当前将所有的这类错误视为致命错误，出错时直接abort。
*/
	static bool verify_operator_sym(const string& sym,
		const bool is_fatal = true)
	{
		bool is_wrong = false;
		int sym_len = sym.length();

		//不支持超过2字符的operator
		if (sym_len > 2)
		{
			err_print(is_fatal, "operator %s is longer than 2,"
				"which is not supported\n", sym.c_str());
			is_wrong = true;
		}

		//不能覆盖builtin的操作符，这样的定义无效
		if (sym_len == 1
			&& lexer::find_protected_char_token(sym[0]) != TOKEN_UNDEFINED)
		{
			err_print(is_fatal, "%s is a protected char, "
				" shoud not be redefined\n", sym.c_str());
			is_wrong = true;
		}

		//不支持使用数字或者字符，避免与函数调用混淆
		if (isalnum(sym[0]) || 
			(sym.length() ==2 && isalnum(sym[1])))
		{
			err_print(is_fatal, "operator %s should not contain alphabetic"
				"character or digit\n", sym.c_str());
			is_wrong = true;
		}
		return is_wrong;
	}

/*
为operator构建统一的(包含操作数个数和优先级信息)全局链接名称。
以便库方式定义的operator能够正常，且能正确地被引用(优先级一致)。
*/
	static string build_operator_external_name(int op_num,
		const string& sym, int prio = 0)
	{
		assert(op_num == 1 || op_num == 2);
		const string prefix = op_num>1 ? "_binary_" : "_unary_" ;
		//名称中植入优先级，库定义与extern声明不一致时会链接失败
			return prefix + sym + "_with_prio_" + to_string(prio);
	}


};

class function_ast : public generic_ast
{
	prototype_t prototype;
	expr_t body;
public:
	function_ast(prototype_t  prototype, expr_t body) :
		prototype(std::move(prototype)), body(std::move(body))
	{
		type = FUNCTION_AST;
	}
	const expr_t& get_body() const{return body;}
	const prototype_t& get_prototype() const{return prototype;}
};

/*
expr_ast目前有如下几种类型：
数值常量				number_ast
变量						variable_ast
二元运算				binary_operator_ast
函数调用				call_ast
*/
class number_ast : public expr_ast
{
	double val;
public:
	number_ast(double val) : val(val) { type = NUMBER_AST;}
	inline double get_val() const {return val;}
};

class variable_ast : public expr_ast
{
	string name;
public:
	variable_ast(const string & name) : name(name) { type = VARIABLE_AST;}
	const string& get_name() const {return  name;}
};


enum binary_operator_type : unsigned char
{
	BINARY_ADD = 0,
	BINARY_SUB,
	BINARY_MUL,
	BINARY_LESS_THAN,
	BINARY_USER_DEFINED,		//用户自定义扩展的操作符
	BINARY_UNKNOWN
};
using binary_operator_t = enum binary_operator_type;
class binary_operator_ast : public expr_ast
{
	binary_operator_t op;
	expr_t LHS;
	expr_t RHS;
//发射call的时候需要组装名称，需要原始名称和prio
	string raw_str;
	int prio;
public:
	binary_operator_ast(binary_operator_t op, expr_t LHS,
		expr_t RHS, const string& str, int prio) : op(op), LHS(std::move(LHS)),
			RHS(std::move(RHS)), raw_str(str), prio(prio)
	{
		type = BINARY_OPERATOR_AST;
	}

	static int get_priority(binary_operator_t in_op)
	{
		//确保这里的优先级数值与binary_operator_type保持一致
		static const int16_t operator_priority_array[BINARY_UNKNOWN + 1] = 
			{20, 20, 30, 10, -1, -1};
		assert(in_op <= BINARY_UNKNOWN);
		return operator_priority_array[in_op];
	}
	int get_priority() const {return prio;}
	inline binary_operator_t get_op() const {return op;}
	inline const expr_t& get_lhs() const {return LHS;}
	inline const expr_t& get_rhs() const {return RHS;}
	static binary_operator_t get_binary_op_type(token &in)
	{
		if (in.get_str().size() > 2 || !is_binary_operator_token(in))
			return BINARY_UNKNOWN;

		if (in == TOKEN_BINARY_OP)	//内置运算符
		{
			switch (in.get_str()[0])
			{
				case '+':
					return BINARY_ADD;
				case '-':
					return BINARY_SUB;
				case '*':
					return BINARY_MUL;
				case '<':
					return BINARY_LESS_THAN;
			}
			return BINARY_UNKNOWN;
		}

		if (in == TOKEN_USER_DEFINED_BINARY_OPERATOR)
			return BINARY_USER_DEFINED;
		
		return BINARY_UNKNOWN;
	}
};

class call_ast : public expr_ast
{
/* 
原示例中直接记录了callee的name。
更好的方案应该是以prototype_ast 指针来表达被调用者。
这样多次调用同一函数不会产生重复的string数据，
并且正确性检查也更为简单（例如类型是否兼容）。
缺点就是实现更复杂一些，需要做hash来查找prototype_ast。
*/
	prototype_t callee;
//这里的args是调用处传入的实际参数
	expr_vector args;
public:
	call_ast (prototype_t in_callee, expr_vector in_args) 
	{
		callee = std::move(in_callee);
		args = std::move(in_args);
		type = CALL_AST;
	}
	const prototype_t& get_callee() const {return callee;}
	const expr_vector& get_args() const {return args;}
};

//if语句的格式：IF cond_expr THEN then_expr ELSE else_expr
class if_ast : public expr_ast
{
	expr_t cond_expr;
	expr_t then_expr;
	expr_t else_expr;
public:
	if_ast(expr_t c, expr_t t, expr_t e) : cond_expr(std::move(c)), 
		then_expr(std::move(t)), else_expr(std::move(e)) {type = IF_AST;}
	const expr_t& get_cond() const{ return cond_expr;}
	const expr_t& get_then() const{ return then_expr;}
	const expr_t& get_else() const{ return else_expr;}
};

/*
	原示例中，for表达式的格式：
	for i = 1, i < n, 1.0 in
		expr_xxx
	所以，ast中需要记录的内容包括
	指示变量的名称				i
	指示变量的初始值		1
	指示变量的终止值		n
	循环步长						1.0
	循环体							expr_xxx
*/
class for_ast : public expr_ast
{
	string induction_var_name;
	expr_t start;
	expr_t end;
	expr_t step;
	expr_t body;
public:
	for_ast(const string& name, expr_t in_start, 
		expr_t in_end, expr_t in_step, expr_t in_body) : 
		induction_var_name(std::move(name)), start(std::move(in_start)), 
		end(std::move(in_end)), step(std::move(in_step)),
		body(std::move(in_body))
	{type = FOR_AST;}

	const string& get_idt_name() const{ return induction_var_name;}
	const expr_t& get_start() const{ return start;}
	const expr_t& get_end() const{ return end;}
	const expr_t& get_step() const{ return step;}
	const expr_t& get_body() const{ return body;}
};

} // end of toy_compiler
#endif