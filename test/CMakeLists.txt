# CMake 最低版本号要求
cmake_minimum_required (VERSION 2.8)

# 项目信息
#project (toy_compiler_test)

FILE(GLOB TEST_SOURCE *.cpp)
FILE(GLOB COMPILER_SRC ../src/*.cpp)

# 指定生成目标
add_executable(test_main ${TEST_SOURCE} ${COMPILER_SRC})
target_link_libraries (test_main gtest pthread)