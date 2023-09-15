#include <iostream>
#include <string>
#include "simplex.hpp"

struct string_input : simplex::Input<string_input>
{
    std::string str;
    size_t idx;
    string_input(std::string s) : str(s), idx(0) {}
    char get()
    {
        std::cout << "input.get() " << str[idx] << std::endl;
        return str[idx++];
    }
    char peek()
    {
        std::cout << "input.peek() " << str[idx] << std::endl;
        return str[idx];
    }
};

int main()
{
    auto matcher = Sex(R"(a{1,3}[-az-AZ-09_ ])");

    string_input in("a_abs");
    std::cout << ((matcher.match(in)) ? "true" : "false") << std::endl;
    return 0;
}
