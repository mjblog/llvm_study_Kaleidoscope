#ifndef _UTILS_H_
#define _UTILS_H_

#include <iostream>
#include <cstdio>
#include <cstdarg>
namespace toy_compiler
{
	/*fatal的时候可以再打印堆栈回溯 */
	#define err_print(is_fatal, fmt, ...)  \
	({ \
		fprintf(stderr, "errors found %s:%d:%s\n", __FILE__, __LINE__, __FUNCTION__); \
		fprintf(stderr, fmt, ##__VA_ARGS__); \
		if (is_fatal) \
			abort(); \
	})

	#define print_and_return_nullptr_if_check_fail(expr, errmsg_fmt, ...) \
	({ \
		if ((expr) != true) \
		{ \
			fprintf(stderr, errmsg_fmt, ##__VA_ARGS__); \
			return nullptr; \
		} \
	})
}

#endif