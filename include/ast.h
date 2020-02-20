#ifndef _AST_H_
#define _AST_H_
#include <cstdint>
#include <string>
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

class prototype_ast : public generic_ast, 
	public enable_shared_from_this<prototype_ast>
{
	string name;
/* args理论上应该指向ast以表达类型信息。
这个玩具语言只有double型，所以用string也可以。 */
	vector<string> args;
//所有的原型都放在这里，以便call的时候查找，算是函数符号表了
	static unordered_map<string, prototype_ast*>& get_proto_tab()
	{
		static unordered_map<string, prototype_ast*> prototype_tab;
		return prototype_tab;
	}
public:
	prototype_t get_shared_ptr()  {return shared_from_this();}
	prototype_ast(const string& in_name, vector<string> in_args) :
		name(std::move(in_name)), args(std::move(in_args))
		{ 
			type = PROTOTYPE_AST;
/*
这里如果用shared_ptr，会导致this指针被两个shared_ptr持有，会有重复释放
解决shared_ptr使用this指针需要用到特殊的模板，参考https://en.cppreference.com/w/cpp/memory/enable_shared_from_this/enable_shared_from_this
注意shared_from_this调用时，this必须已经被一个shared_ptr持有，因此不能直接在constructor中使用get_shared_ptr
*/
			get_proto_tab().insert(make_pair(name, this));
		}
	const string& get_name() const { return name; }
	const vector<string>& get_args() const {return args;}
	static inline prototype_t find_prototype(const string& key)
	{
		auto result = get_proto_tab().find(key);
		if (result != get_proto_tab().cend())
			return result->second->get_shared_ptr();
		else
			return nullptr;
	}
};

class function_ast : public generic_ast
{
	prototype_t prototype;
	expr_t body;
public:
	function_ast(prototype_t  in_prototype, expr_t in_body)
	{
		prototype = std::move(in_prototype);
		body = std::move(in_body);
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
	number_ast(double in_val) : val(in_val) { type = NUMBER_AST;}
	inline double get_val() const {return val;}
};

class variable_ast : public expr_ast
{
	string name;
public:
	variable_ast(const string & in_name) : name(in_name) { type = VARIABLE_AST;}
	const string& get_name() const {return  name;}
};

typedef enum binary_operator_type
{
	BINARY_ADD,
	BINARY_SUB,
	BINARY_MUL,
	BINARY_LESS_THAN,
	BINARY_UNKNOWN
} binary_operator_t;

class binary_operator_ast : public expr_ast
{
	binary_operator_t op;
	expr_t LHS;
	expr_t RHS;
public:
	binary_operator_ast(binary_operator_t in_op, expr_t in_LHS, expr_t in_RHS)
	{
		op = in_op;
		LHS = std::move(in_LHS);
		RHS = std::move(in_RHS);
		type = BINARY_OPERATOR_AST;
	}
	static int get_priority(binary_operator_t in_op)
	{
		//确保这里的优先级数值与enum binary_op保持一致
		static const constexpr uint16_t operator_priority_array[BINARY_UNKNOWN + 1] = 
		{20, 20, 30, 10, 0};
		return operator_priority_array[in_op];
	}
	int get_priority() const {return get_priority(op);}
	inline binary_operator_t get_op() const {return op;}
	inline const expr_t& get_lhs() const {return LHS;}
	inline const expr_t& get_rhs() const {return RHS;}
	static binary_operator_t get_binary_op_type(token &in)
	{
		if (in.get_str().size() != 1|| in != TOKEN_BINARY_OP)
			return BINARY_UNKNOWN;
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