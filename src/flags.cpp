#include <cstdlib>
#include <sstream>
#include "flags.h"
/*
为了简单，使用环境变量控制编译器行为。
注意还未对输入值做合法性检查，后续可以callback函数形式添加
*/
namespace toy_compiler{
using namespace std;
control_flags global_flags;

control_flags::control_flags()
{
#define DECL_FLAG(flag_type, flag_name, default_val, input_env, des) \
flag_item<flag_type> flag_name##cons(default_val, input_env, des);\
flag_name = flag_name##cons;
#include "flags.def"
#undef DECL_FLAG
}
}	//end of toy_compiler