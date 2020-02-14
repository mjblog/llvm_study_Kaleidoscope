#include <cassert>
#include <memory>

#include "codegen.h"
#include "utils.h"

namespace toy_compiler{
using namespace std;
/* 
从ast读出数据转换为IR的流程是基本固定的，就是遍历结构，逐步向下细化；
由于这个遍历过程是基本稳定的，可以将其抽象为统一的访问模型class。
*/


}	//end of toy_compiler