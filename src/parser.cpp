#include <cassert>
#include <memory>
#include <string>
#include "parser.h"
#include "utils.h"
namespace toy_compiler{
using namespace std;
void parser::parse()
{
/*
自顶向下解析
顶层只有extern声明，def函数，运算表达式三种情况。
原示例为了表达终止，加入了';'符号。
考虑在非交互模式下该特性没有用处，且其对程序语义无影响，去掉了该特性。
终止可以用eof表达。
top ::= definition | external | expression | ';'
*/
	auto cur_token = get_next_token();
	while (1)
	{
		switch (cur_token)
		{
			case TOKEN_EOF:
				return;
			case TOKEN_DEF:
				handle_definition();
				break;
			case TOKEN_EXTERN:
				handle_extern();
				break;
			default:
				handle_toplevel_expression();
				break;
		}
		cur_token = get_cur_token();
	}
}

//handle系列的函数都只是push ast到vector中并报告错误
void parser::handle_definition()
{
	auto function_ast = parse_definition();
	if (function_ast)
	{
		//把全局的ast统一放到vector中，便于后续遍历处理
		ast_vec.push_back(std::move(function_ast));
	}
	else
	{
		/* 吃掉错误的token，尝试继续解析 */
		err_print(false, "fail to parse a function, tring to continue\n");
		get_next_token();
	}
}

//definition 有两个ast，表达原型的prototype_ast和函数体body的expr_ast
shared_ptr<function_ast> parser::parse_definition()
{
/* definition 就是函数，其格式为： 关键字def  prototype body*/
	assert(parser::get_cur_token().type == TOKEN_DEF);
	get_next_token();	//def无需记录，直接吃掉def这个token
	auto proto = parse_prototype();
	print_and_return_nullptr_if_check_fail(proto != nullptr, 
		"fail to get a prototype\n");

	auto body = parse_expr();
	print_and_return_nullptr_if_check_fail(body != nullptr, 
		"fail to get the body for function %s\n", proto->get_name().c_str());

	return std::make_shared<function_ast>(std::move(proto), std::move(body));
}


//解析函数原型（也包括用户自定义的operator）
prototype_t parser::parse_prototype()
{
/*
prototype有三种格式。
第一种是函数： 函数名 左括号 参数 右括号
第二种是用户定义的binary operator： binary 符号 优先级 左括号 参数 右括号
第三种是用户定义的unary operator： unary 符号 左括号 参数 右括号
后面的参数部分可以复用代码
*/
	const token* cur_token = &get_cur_token();
	string name;

/*
命名错误的用户定义operator会导致各种不可预期的错误：
如用户重新定义已经builtin的 '+'，则用户的定义无法生效。
用户定义一个合法的identifier名称如abc，很容易与函数定义产生冲突，
例如库里定义了operator abc，用户代码里又再定义了abc函数，调用时会产生二义性。
*/
	int args_num_limit = -1;	//operator需要限制入参个数
	string op_sym;
	double prio;
	int op_prio = -1;
	switch (*cur_token)
	{
		case TOKEN_IDENTIFIER:
			name = cur_token->get_str();
			break;
		case TOKEN_BINARY:
			args_num_limit = 2;
			op_sym = get_next_token().get_str();
/*
检查用户定义的operator符号是否合法。
当前不合法我们是直接abort了，所以返回值实际上无用。
*/
			prototype_ast::verify_operator_sym(op_sym);
			prio = get_double_from_number_token(get_next_token());
			print_and_return_nullptr_if_check_fail (prio > 1 && prio < 100, 
				"Invalid precedence %lf for operator %s, should be 1~100\n",
					prio, get_cur_token().get_cstr());
			//prio在1~100间再转，不会溢出
			op_prio = prio;
			name = prototype_ast::build_operator_external_name(2,
																op_sym, op_prio);
			break;
		case TOKEN_UNARY:
			args_num_limit = 1;
			op_sym = get_next_token().get_str();
			prototype_ast::verify_operator_sym(op_sym);
			name = prototype_ast::build_operator_external_name(1,
																op_sym);
			break;
		default:
			print_and_return_nullptr_if_check_fail(false, "expected a 'binary'"
				", 'unary' or identifier,  but got a %s\n", cur_token->get_cstr());
	}

	auto left_paren = get_next_token();
	print_and_return_nullptr_if_check_fail(
		left_paren == TOKEN_LEFT_PAREN, "expected a '(' but got a %s\n", 
			left_paren.get_cstr());

	//参数的格式为  0或多个identitfier token，以')'结束
	vector<string> args;
	cur_token = &get_next_token();
	for ( ; *cur_token == TOKEN_IDENTIFIER; )
	{
		args.push_back(cur_token->get_str());
		cur_token = &get_next_token();
	}

	print_and_return_nullptr_if_check_fail(*cur_token == TOKEN_RIGHT_PAREN, 
		"expected identifier or ')' but got a %s\n", cur_token->get_cstr());
	get_next_token();	//吃掉 ')'

	prototype_t ret;
	if (args_num_limit == -1)	//没有参数个数限制是普通函数
	{
	/*
	为了简单，暂时将重复定义设置为致命错误。
	更精细的控制，应该考虑优雅地吃掉重复定义包含的所有token，
	以使得后续的错误提示有实际意义。
	*/
		if (find_prototype(name) != nullptr)
			err_print(true, "%s redefined\n", name.c_str());
		ret = make_shared<prototype_ast>(name, std::move(args));
	}
	else
	{	//operator 生成
	/* 
	operator的检查比较复杂。
	为了保证定义和引用时operator优先级一致，
	operator的name尾部植入了优先级数值。
	目前这样的机制设计，仍有一种特殊情况比较模糊：
	a中定义operatorX 优先级20,
	b中定义operatorX 优先级10,
	c中extern operatorX 优先级引用10；
	如果a中的定义是一种错误，我们也无法检查出来。
	一种比较简单的补救措施是，定义operator时同时输出
	带优先级和不带优先级的名称，链接时可以报重复定义。
	考虑到必要性不强，暂不实施。
	*/
		//注册用户自定义运算符，并做重复定义检查
		const auto defined = user_defined_operator_prio_tab.find(op_sym);
		if (defined == user_defined_operator_prio_tab.cend())
			user_defined_operator_prio_tab.insert(make_pair(op_sym, op_prio));
		else
			err_print(true, "binary operator %s redefined\n", op_sym.c_str());
		
		//确定参数个数和operator的要求一致
		print_and_return_nullptr_if_check_fail((size_t)args_num_limit == 
			args.size(), "operator expected %d args but got %ld\n",
			args_num_limit, args.size());
		ret = make_shared<prototype_ast>(name, std::move(args), true, op_prio);
	}
/*
注意这里一定要先转为string_view再make_pair，否则这里会临时构造出一个
pair<string, prototype_t*> 再转为 pair<string_view, prototype_t*>再insert。
这会导致get_proto_tab中的key 变成一个指向临时stack变量的string_view。
一定注意make_pair是根据入参确定返回值的，所以要先转换好再传入。
*/
	get_proto_tab().insert(make_pair(string_view(ret->get_name()), ret.get()));
	return ret;
}

/*
expr 是最为复杂的ast，有四种子类。从格式上来说，应满足如下要求
左操作数+(binary_operator+右操作数)*
操作数可以是如下几种类型：
number 数值；identifier 变量；identifier() call调用；(expr) 括号包裹的表达式
*/
expr_t parser::parse_expr()
{
//parse_primary处理所有可能为操作数的情况
	auto lhs = parse_unary_expr();
	if (lhs == nullptr)
		return nullptr;

//如果解析完后没有二元操作符，就直接返回了
	const auto& cur_token = get_cur_token();
/* 
	由于parse_binary_expr头部有同样的检查逻辑，其实这个if是不必要的。
	为了更清晰的表达逻辑，将其留下
*/
	if (!is_binary_operator_token(cur_token))
		return lhs;
	else
	{
		return parse_binary_expr(0, lhs);
	}
}

//number 数值；identifier 变量；identifier() call调用；(expr) 括号包裹的表达式
expr_t parser::parse_primary_expr()
{
	const auto& cur_token = get_cur_token();
	switch (cur_token.get_type())
	{
		case TOKEN_NUMBER:
			return parse_number();
//call的格式为identifier + '(' + ')'，为了简单一并放入parse_identifier中
		case TOKEN_IDENTIFIER:
			return parse_identifier();
		case TOKEN_LEFT_PAREN:
			return parse_paren();
		case TOKEN_IF:
			return parse_if();
		case TOKEN_FOR:
			return parse_for();
		default:
			err_print(false, "expected number,identifier or '(', but got %s\n",
				cur_token.get_cstr());
			return nullptr;
	}

	assert(false);
}

expr_t parser::parse_unary_expr()
{
	const auto& cur_token = get_cur_token();
	//这里最终应该是
	if (!is_unary_operator_token(cur_token))
		return parse_primary_expr();
	else
	{
		const string opcode = cur_token.get_str();
		get_next_token();	//吃掉当前的unary
		auto operand = parse_unary_expr();
		print_and_return_nullptr_if_check_fail(operand != nullptr, "failed to "
			"get the operand of unary %s\n", opcode.c_str());
		const auto& name = prototype_ast::build_operator_external_name(1,
			opcode);
		return make_shared<unary_operator_ast>(opcode,
			std::move(operand), std::move(name));
	}
}

/*
parse_binary_expr负责构建由lhs开头的连续二元操作表达式，
为了递归实现的简单性，这个函数逻辑上返回的是binary_op的rhs（rhs其实也可以是一个完整的二元操作表达式），为此有两个特殊的约定：
1 它接受单个primary_ast作为返回值
2 在prev_op_prio=0传入时，就认为没有前置op了，此时返回本函数返回的右半边就是整个binary_op表达式了

这个函数看到的输入有三个：
prev_op_prio入参		是前一个binary_op的优先级
lhs入参								是当前函数要给出的rhs的左半部
cur_token						是rhs的operator

它的处理逻辑是：
如果右半边operator优先级(cur_token给出)低于或者等于prev_op_prio，
那么说明右半边的lhs应该和prev_op结合，此时返回lhs由上一层来合成binary_op表达式。
反之说明右半边的lhs应该和右半边的operator结合，
此时向下读取一个primary_ast然后组装成binary_op_ast(注意组装时需要取rhs，需要递归调用本函数)。
组装binary_op_ast完后，不能立即返回，需要判断下一个op的优先级是否高于prev_op。
只有下一个op的优先级低于或者等于prev_op时，才应该返回当前获得变得binary_op_ast。
否则应该将组装的binary_op_ast作为lhs，继续和下一个op结合。

以一个比较复杂的a<b*c+d例子来看处理过程：
level1：
	输入prev_prio=0，lhs=a，cur_token指向'<';
	<的优先级为10，高于0，所以a应该和<结合;
	组装a<?时，先吃掉<，再调用parse_primary解析出b；
	此时不能直接组装为a<b因为b可能会跟后面更高优先级的op结合；
	需要需要递归调用函数parse_binary_expr(10, b)来获得；
level2:
	输入prev_prio=10，lhs=b，cur_token指向'*';
	*的优先级为30，高于10，所以b需要与*结合；
	组装b*?时，先吃掉*，再调用parse_primary解析出c；
	递归调用函数parse_binary_expr(30, c)来获得?；
level3：
	输入prev_prio=30，lhs=c，cur_token指向'+';
	+的优先级为20，低于30，c应该和上一个op结合，直接返回c；
level2:
	获得了parse_binary_expr(30, c)的返回值为c，组装出b*c；
	读取cur_token的优先级(当前cur_token已经指向+)为20，大于prev_prio=10；
	cur_op_prio > prev_prio意味着，组装出的b*c应该和cur_op(也就是+)组装后再作为rhs返回;
	把b*c作为lhs，组装(b*c)+?，先吃掉+，再调用parse_primary解析出d；
	递归调用函数parse_binary_expr(20, d)来获得?；
level3：
	输入prev_prio=20，lhs=d，cur_token指向EOF;
	检查cur_token不是binary_op，二元操作表达式结束，返回d；
level2:
	获得了parse_binary_expr(20, d)的返回值为d，组装出(b*c)+d；
	再次读取cur_token，由于cur_token已经是EOF，返回(b*c)+d；
level1:
	获得parse_binary_expr(10, b)的返回值(b*c)+d；
	组装出a<((b*c)+d)
	再次读取cur_token，由于cur_token已经是EOF，返回a<((b*c)+d)；
*/
expr_t parser::parse_binary_expr(int prev_op_prio, expr_t lhs)
{
	while (1)
	{
		auto cur_token = get_cur_token();
//必须要有这个检查，因为递归解析完最后一个identifier后，会while回到这里
//这个return同时也是确保处理完binary_op后循环能退出的检查点
		if (!is_binary_operator_token(cur_token))
			return lhs;
		auto cur_op_type = binary_operator_ast::get_binary_op_type(cur_token);
		//unknown是实现错误
		assert(cur_op_type != BINARY_UNKNOWN);
		int cur_op_prio = -1;
		string op_external_name;
		if (cur_op_type != BINARY_USER_DEFINED)
			cur_op_prio = binary_operator_ast::get_priority(cur_op_type);
		else
		{
			cur_op_prio = get_user_defined_operator_prio(cur_token.get_str());
			op_external_name = prototype_ast::build_operator_external_name(2,
				cur_token.get_str(), cur_op_prio);
		}
		
		if (cur_op_prio > prev_op_prio)
		{
			get_next_token();	//吃掉binary_op后解析所有可能为数值的情况
			auto rhs = parse_unary_expr();
			if (rhs == nullptr)
				return nullptr;
			auto new_rhs = parse_binary_expr(cur_op_prio, rhs);
			lhs = std::make_shared<binary_operator_ast>(
				cur_op_type, lhs, new_rhs, op_external_name);
		}
		else
			return lhs;
	}
}

expr_t parser::parse_number()
{
	double num_d = get_double_from_number_token(get_cur_token());
	get_next_token();		//吃掉当前的number token
	return std::make_shared<number_ast>(num_d);
}

//该函数要处理variable变量和call调用两种情况
expr_t parser::parse_identifier()
{
	string name = get_cur_token().get_str();
	auto next_token = get_next_token();
	if (next_token != TOKEN_LEFT_PAREN)
		return std::make_shared<variable_ast>(name);
	else
		get_next_token(); //吃掉左括号
	vector<expr_t> args;
/*
剩下的部分为0或多个expr，然后')'
原示例中要求args中的多个expr以 ','分割是没有必要的，将其去掉
*/
	while(1)
	{
		if (get_cur_token() == TOKEN_RIGHT_PAREN)
		{
			//吃掉右括号，然后构建call_ast返回
			get_next_token();
			auto find_callee = find_prototype(name);
			print_and_return_nullptr_if_check_fail(find_callee != nullptr, 
				"can not find prototype for %s\n", name.c_str());
			prototype_t callee(find_callee);
			return std::make_shared<call_ast>(callee, std::move(args));
		}
		auto arg = parse_expr();
		print_and_return_nullptr_if_check_fail(arg != nullptr, 
			"fail to parse expr for  %s\n", name.c_str());
		args.push_back(std::move(arg));
	}

//不应该走到
	assert(false);
	return nullptr;
}

//paren就是 ( + expr +），剥去括号就是常规的expr解析
expr_t parser::parse_paren()
{
	get_next_token();		//吃掉左括号
	auto expr = parse_expr();
	if (expr != nullptr)
	{
		auto cur_token = get_cur_token();
		print_and_return_nullptr_if_check_fail(
			cur_token == TOKEN_RIGHT_PAREN,
			"expected ')' but got %s\n", cur_token.get_cstr());
		get_next_token();	//吃掉右括号
		return expr;
	}
	else
		return nullptr;
}

// if的语法为: IF expr THEN expr ELSE expr
expr_t parser::parse_if() 
{
	const token* cur_token;
	get_next_token();	//吃掉IF
	const auto& cond = parse_expr();
	print_and_return_nullptr_if_check_fail(cond != nullptr, 
		"failed to parse cond expr in if_ast\n");

	cur_token = &(get_cur_token());
	print_and_return_nullptr_if_check_fail(*cur_token == TOKEN_THEN, 
		"expected a 'then' but got %s\n", cur_token->get_cstr());
	get_next_token();	//吃掉THEN
	const auto& expr_in_then = parse_expr();
	print_and_return_nullptr_if_check_fail(expr_in_then != nullptr, 
		"failed to parse then expr in if_ast\n");

/*
这里我们强制要求有else分支，这与很多常规命令式语言的设计常识冲突。
其原因是，当前这个玩具语言的基本设计逻辑是：
所有的语句都是expr，每一个expr都必须要有value。

这是比较典型的函数式语言设计逻辑，可以参考Haskell和Erlang语言。
如无else分支，会遇到“cond==false时无法确定if_expr的值”的困境。
Erlang语言的if如果没有condition为真，会抛出异常。
当前该语言没有异常，且由于值类型只有double，甚至无法正常表达错误。

综上，最简单的选择还是强制else分支存在。
*/
	cur_token = &(get_cur_token());
	print_and_return_nullptr_if_check_fail(*cur_token == TOKEN_ELSE, 
		"expected a 'else' but got %s\n", cur_token->get_cstr());
	get_next_token();	//吃掉next
	const auto& expr_in_else = parse_expr();
	print_and_return_nullptr_if_check_fail(expr_in_else != nullptr, 
		"failed to parse else expr in if_ast\n");

	return std::make_shared<if_ast>(cond, expr_in_then, expr_in_else);
}

/*
原示例中，for表达式的格式：
FOR i = 1, i < n, 1.0 IN
	body_expr
为了把 ',' 留给顺序求值operator(与多数语言保持一致)，这里改用':'分割。
修改后格式为:
FOR i = 1 : i < n : 1.0 IN
*/
expr_t parser::parse_for() 
{
	const token* cur_token;
	get_next_token();									//吃掉FOR
	cur_token = &(get_cur_token());		//吃掉FOR后，第一个是变量名
	print_and_return_nullptr_if_check_fail(*cur_token == TOKEN_IDENTIFIER, 
		"expected a identifier token but got %s\n", cur_token->get_cstr());
	//parse_identifier可能会返回call的ast，正确性检查还更复杂
	//我们直接从token中取出induction_var
	const string idt_var_name = cur_token->get_str();
	get_next_token();									//吃掉变量名token

	cur_token = &(get_cur_token());		//变量名后是=
	print_and_return_nullptr_if_check_fail(*cur_token == TOKEN_ASSIGN, 
		"expected a '=' token but got %s\n", cur_token->get_cstr());

	get_next_token();									//吃掉=，后面是循环变量start值
	expr_t start  = parse_expr();
	print_and_return_nullptr_if_check_fail(start != nullptr, 
		"failed to parse start expr in for_ast\n");

	cur_token = &(get_cur_token());		//循环变量start值后是分隔符":"
	print_and_return_nullptr_if_check_fail(*cur_token == TOKEN_COLON, 
		"expected a ':' token but got %s\n", cur_token->get_cstr());

	get_next_token();									//吃掉":"，解析end
	expr_t end = parse_expr();
	print_and_return_nullptr_if_check_fail(end != nullptr, 
		"failed to parse end expr in for_ast\n");

	cur_token = &(get_cur_token());		//end后可能是分隔符":"或者IN
	expr_t step = nullptr;
	if (*cur_token == TOKEN_COLON)
	{
		get_next_token();							//吃掉":"
		step = parse_expr();
		print_and_return_nullptr_if_check_fail(end != nullptr, 
			"failed to parse step expr in for_ast\n");
	}

	//走到这里一定是in token了
	cur_token = &(get_cur_token());
	print_and_return_nullptr_if_check_fail(*cur_token == TOKEN_IN, 
		"expected a 'in' token but got %s\n", cur_token->get_cstr());

	get_next_token();							//吃掉"IN"
	expr_t body = parse_expr();
	print_and_return_nullptr_if_check_fail(body != nullptr, 
		"failed to parse body in for_ast\n");
	return std::make_shared<for_ast>(std::move(idt_var_name), std::move(start),
		std::move(end), std::move(step), std::move(body));
}

void parser::handle_extern()
{
	auto prototype_ast = parse_extern();
	//把全局的ast统一放到vector中，便于后续遍历处理
	if (prototype_ast)
		ast_vec.push_back(std::move(prototype_ast));
	else
	{
		/* 吃掉错误的token，尝试继续解析 */
		err_print(false, "fail to parse a extern prototype,"
			"tring to continue\n");
		get_next_token();
	}
}

prototype_t parser::parse_extern()
{
	get_next_token();		//吃掉extern后就是prototype
	return parse_prototype();
}


/*
原示例中全局表达式被放到一个匿名函数中，会产生很多问题，如重名函数等。
我们的实现将其放到全局ast vector中，更简单灵活。
*/
void parser::handle_toplevel_expression()
{
	auto expr = parse_expr();
	//把全局的ast统一放到vector中，便于后续遍历处理
	if (expr)
		ast_vec.push_back(std::move(expr));
	else
	{
		/* 吃掉错误的token，尝试继续解析 */
		err_print(false, "fail to parse a global expression,\
tring to continue\n");
		get_next_token();
	}
}

}	//end of toy_compiler