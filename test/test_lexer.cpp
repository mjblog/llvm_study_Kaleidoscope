#include "lexer.h"
#include <gtest/gtest.h>
using namespace toy_compiler;
TEST(test_lexer, lexer_init)
{
    lexer test_file("/proc/self");
    ASSERT_TRUE(test_file.is_ok);
    lexer test_file1("/aaa/bbb/IBeliveThisShouldNotExist");
    ASSERT_FALSE(test_file1.is_ok);
    lexer test_stdin;
    ASSERT_TRUE(test_stdin.is_ok);
}

TEST(test_lexer, lexer_process)
{
//暂时没有太大的必要做详细测试，ast的测试中已经有隐含
}
