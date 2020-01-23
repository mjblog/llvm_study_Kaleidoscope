#include "lexer.h"
#include <gtest/gtest.h>

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
    lexer test_file("/proc/self");
    ASSERT_TRUE(test_file.is_ok);
    lexer test_file1("/aaa/bbb/IBeliveThisShouldNotExist");
    ASSERT_FALSE(test_file1.is_ok);
    lexer test_stdin;
    ASSERT_TRUE(test_stdin.is_ok);
}
