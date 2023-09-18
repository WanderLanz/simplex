#include <iostream>
#include <string>
#include "simplex.hpp"

#define TEST(expr, input, expected)                                                                                           \
    {                                                                                                                         \
        auto ex = Sex(expr);                                                                                                  \
        string_input in(input);                                                                                               \
        bool matches = ex.match(in);                                                                                          \
        if (matches != expected)                                                                                              \
        {                                                                                                                     \
            std::cerr << "[FAIL] Simplex(\"" expr "\").match(\"" input "\")!=" << (expected ? "true" : "false") << std::endl; \
            ret = 1;                                                                                                          \
        }                                                                                                                     \
    }

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
    int ret = 0;

    // flags
    std::cout << "testing flags..." << std::endl;
    TEST("foo![@#%^jnm,]bar", "foobbar", true);
    TEST("foo![@#%^jnm,]bar", "foo bar", true);
    TEST("foo!*[@#%^jnm,]bar", "foobbar", false);
    TEST("foo!? bar", "foo  bar", true);

    // quantifiers
    std::cout << "testing quantifiers..." << std::endl;
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

    // groups
    std::cout << "testing groups..." << std::endl;
    TEST("a{1,3}[-az-AZ-09_ ]", "a_aZ", true);
    TEST("a{1,3}[-az-AZ-09_ ]", "a0 5", true);
    TEST("a{1,3}[-az-AZ-09_ ]", "a_ ab6", false);

    return ret;
}
