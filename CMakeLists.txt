# CMake 最低版本号要求
cmake_minimum_required (VERSION 2.8)

# 项目信息
project (toy_compiler)

set (CMAKE_INCLUDE_CURRENT_DIR ON)
set (CMAKE_CXX_FLAGS "-g3 -O2 -Wall -Wextra")
set (CMAKE_CXX_FLAGS_DEBUG "-g3 -O0")
set (CMAKE_EXE_LINKER_FLAGS "-pthread")

set (LLVM_DIR /home/majiang/hd/opensource/llvmbuild/install/usr/local)
include_directories(include SYSTEM ${LLVM_DIR}/include)
# 加入一个配置头文件，用于处理 CMake 对源码的设置
#configure_file (
#  "${PROJECT_SOURCE_DIR}/config.h.in"
#  "${PROJECT_BINARY_DIR}/config.h"
#  )
 
# 是否使用自己的 MathFunctions 库
#option (USE_MYMATH
#	   "Use provided math implementation" ON)

# 是否加入 MathFunctions 库
#if (USE_MYMATH)
#  include_directories ("${PROJECT_SOURCE_DIR}/math")
#  add_subdirectory (math)
#  set (EXTRA_LIBS ${EXTRA_LIBS} MathFunctions)
#endif (USE_MYMATH)

# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量
aux_source_directory(src/ DIR_SRCS)


# 指定生成目标
link_directories(${LLVM_DIR}/lib)
execute_process(COMMAND ${LLVM_DIR}/bin/llvm-config --libs core
                                    OUTPUT_VARIABLE LLVM_CORE_LIBS
                                    OUTPUT_STRIP_TRAILING_WHITESPACE)


add_executable(toy_compiler main.cpp ${DIR_SRCS})
target_link_libraries(toy_compiler ${LLVM_CORE_LIBS})

#cmake也有顺序问题，一定要把这个语句放到设置LLVM_CORE_LIBS变量的后面
#否则子目录中用不了这个变量
add_subdirectory(test)