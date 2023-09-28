#include <iostream>
#include <string>
#include "simplex.hpp"

using namespace std::literals;

#define TEST(expr, input, expected)                                                                                         \
    {                                                                                                                       \
        constexpr auto ex{Simplex(expr)}; /* char[expr.size()] */                                                           \
        constexpr std::string_view in{input##sv};                                                                           \
        bool matches = ex.matches(in);                                                                                      \
        if (matches != expected)                                                                                            \
        {                                                                                                                   \
            std::cerr << "[FAIL] Sex(\"" expr "\").matches(\"" input "\")!=" << (expected ? "true" : "false") << std::endl; \
            exitCode = 1;                                                                                                   \
        }                                                                                                                   \
    }

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

    TEST("a{1,3}![-a\\z-AZ-09_ ]", "a_aZ", false);
    TEST("a{1,3}![-az-AZ-09_ \\]]", "a0 5", false);
    TEST("a{1,3}![-az-AZ-09_ \0]", "a}}}", true);

    std::cout << std::endl;
    if (exitCode == 0)
        std::cout << "[PASS] all tests passed";
    else
        std::cout << "[FAIL] tests failed";
    std::cout << std::endl;
    return exitCode;
}
