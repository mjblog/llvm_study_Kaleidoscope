#include <cstdlib>
#include <sstream>
/*
为了简单，使用环境变量控制编译器行为。
注意还未对输入值做合法性检查，后续可以callback函数形式添加
*/
namespace toy_compiler{
using namespace std;
class control_flags
{
	template <typename T>
	struct flag_item
	{
		T flag_val;
		const char* input_env;
		const char* description;
		flag_item(T default_val, const char* env, const char* des) :
			input_env(env), description(des)
		{
			const char *env_val = getenv(input_env);
			if (env_val != nullptr)
			{
				stringstream tmp;
				tmp << env_val;
				tmp >> flag_val;
			}
			else
				flag_val = default_val;
		}
		flag_item(){}
		operator T() {return flag_val;}
	};

public:
#define DECL_FLAG(flag_type, flag_name, default_val, input_env, des) \
	flag_item<flag_type> flag_name;
#include "flags.def"
#undef DECL_FLAG
	control_flags()
	{
#define DECL_FLAG(flag_type, flag_name, default_val, input_env, des) \
	flag_item<flag_type> flag_name##cons(default_val, input_env, des);\
	flag_name = flag_name##cons;
#include "flags.def"
#undef DECL_FLAG
	}
}global_flags;

}	//end of toy_compiler