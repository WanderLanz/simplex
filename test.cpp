#include <iostream>
#include <string>
#include "simplex.hpp"

#define TEST(expr, input, expected)                                                                                         \
    {                                                                                                                       \
        auto ex = Sex(expr);                                                                                                \
        string_input in(input);                                                                                             \
        bool matches = ex.matches(in);                                                                                      \
        if (matches != expected)                                                                                            \
        {                                                                                                                   \
            std::cerr << "[FAIL] Sex(\"" expr "\").matches(\"" input "\")!=" << (expected ? "true" : "false") << std::endl; \
            exitCode = 1;                                                                                                   \
        }                                                                                                                   \
    }
#if __cplusplus >= 202002L
#define TEST_OP(expr, input, expected)                                                                                     \
    {                                                                                                                      \
        auto ex = expr##_Sex;                                                                                              \
        string_input in(input);                                                                                            \
        bool matches = ex.matches(in);                                                                                     \
        if (matches != expected)                                                                                           \
        {                                                                                                                  \
            std::cerr << "[FAIL] \"" expr "\"_Sex.matches(\"" input "\")!=" << (expected ? "true" : "false") << std::endl; \
            exitCode = 1;                                                                                                  \
        }                                                                                                                  \
    }
#endif

struct string_input : simplex::Input<string_input>
{
    std::string str;
    size_t idx;
    string_input(std::string s) : str(s), idx(0) {}
    void pop()
    {
        ++idx;
    }
    char peek()
    {
        return str[idx];
    }
};

int main()
{
    int exitCode = 0;
    std::cout << "testing..." << std::endl;

    TEST("foo![@#%^jnm,]bar", "foobbar", true);
    TEST("foo![@#%^jnm,]bar", "foo bar", true);
    TEST("foo!*[@#%^jnm,]bar", "foobbar", false);
    TEST("foo!? bar", "foo  bar", true);
    TEST("foo!\\? bar", "foo@ bar", true);
    TEST("foo!\\? bar", "foo? bar", false);

    TEST("foo* bar", "foobar", true);
    TEST("foo* bar", "foo bar", true);
    TEST("foo* bar", "foo            bar", true);

    TEST("foo+ bar", "foobar", false);
    TEST("foo+ bar", "foo bar", true);
    TEST("foo+ bar", "foo    bar", true);

    TEST("foo? bar", "foobar", true);
    TEST("foo? bar", "foo bar", true);
    TEST("foo? bar", "foo            bar", false);

    TEST("foo{1,3} bar", "foobar", false);
    TEST("foo{1,3} bar", "foo bar", true);
    TEST("foo{1,3} bar", "foo   bar", true);
    TEST("foo{1,3} bar", "foo            bar", false);

    TEST("foo{0,0} bar", "foobar", true);
    TEST("foo{0,0} bar", "foo bar", false);
    TEST("foo{0,3} bar", "foo bar", true);
    TEST("foo{5,0} bar", "foo   bar", false); // malformed, but should still (not) match

    TEST("a{1,3}[-a\\z-AZ-09_ ]", "a_aZ", true);
    TEST("a{1,3}[-az-AZ-09_ \\]]", "a0 5", true);
    TEST("a{1,3}[-az-AZ-09_ ]", "a_ ab6", false);

#if __cplusplus >= 202002L
    // make sure operator""_Sex is working the same as Sex macro
    TEST_OP("foo![@#%^jnm,]bar", "foobbar", true);
    TEST_OP("foo![@#%^jnm,]bar", "foo bar", true);
    TEST_OP("foo!*[@#%^jnm,]bar", "foobbar", false);
    TEST_OP("foo!? bar", "foo  bar", true);
    TEST_OP("foo!\\? bar", "foo@ bar", true);
    TEST_OP("foo!\\? bar", "foo? bar", false);

    TEST_OP("foo* bar", "foobar", true);
    TEST_OP("foo* bar", "foo bar", true);
    TEST_OP("foo* bar", "foo            bar", true);

    TEST_OP("foo+ bar", "foobar", false);
    TEST_OP("foo+ bar", "foo bar", true);
    TEST_OP("foo+ bar", "foo    bar", true);

    TEST_OP("foo? bar", "foobar", true);
    TEST_OP("foo? bar", "foo bar", true);
    TEST_OP("foo? bar", "foo            bar", false);

    TEST_OP("foo{1,3} bar", "foobar", false);
    TEST_OP("foo{1,3} bar", "foo bar", true);
    TEST_OP("foo{1,3} bar", "foo   bar", true);
    TEST_OP("foo{1,3} bar", "foo            bar", false);

    TEST_OP("foo{0,0} bar", "foobar", true);
    TEST_OP("foo{0,0} bar", "foo bar", false);
    TEST_OP("foo{0,3} bar", "foo bar", true);
    TEST_OP("foo{5,0} bar", "foo   bar", false); // malformed, but should still (not) match

    TEST_OP("a{1,3}[-a\\z-AZ-09_ ]", "a_aZ", true);
    TEST_OP("a{1,3}[-az-AZ-09_ \\]]", "a0 5", true);
    TEST_OP("a{1,3}[-az-AZ-09_ ]", "a_ ab6", false);
#endif

    std::cout << std::endl;
    if (exitCode == 0)
        std::cout << "[PASS] all tests passed";
    else
        std::cout << "[FAIL] tests failed";
    std::cout << std::endl;
    return exitCode;
}
