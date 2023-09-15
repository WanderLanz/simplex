# simplex

A simple, overkill, constexpr-parsed, one-character lookahead regex, i.e. a toy I made for fun 😝.

Should probably just learn Boost Regex/Metaparse or something, but I needed to remind myself why I don't like c/c++ macros/templates.

simply copy [simplex.hpp] into your includes and create a matcher with the included `Sex` macro, use `simplex::parse` directly, or if you use C++20 and later, you can use `operator""_sex` to create a matcher.

## Notes

- no backtracking or capture groups
- only basic ascii (0x00-0x7F) string literal expressions
- does not exhaust input, i.e. only matching the current stream of input.
- special characters include R"\\!-\*+?{,}[]", in that order of precedence (escape '\\', flags "!-", quantifiers "*+?{,}", any-group "\[-\]")
- "!" is a negation flag
- '\\' escapes the next character, e.g. "\\\*" matches a literal '*'
- '-' outside of a "[]" is a peek flag, i.e. one character lookahead
- within "\[\]", a "-" is a range operator, e.g. "\[-az\]" matches any lowercase letter
- within "\[\]", all ranges must precede any literals.
- within "{}", only digits or ',' is valid.
- unlike regex, operators must precede the character or group it modifies, i.e. stack-based
- there is a max of 255 for any quantifier, e.g. "{,255}" is valid, "{,256}" is not, because this is a toy
- no predefined special character classes are provided, e.g. ".\b\B\\<\\>\c\s\S\d\D\w\W\x\O"
- there is no guaranteed expression validation, e.g. unterminated operator sequences, malformed expressions MAY cause undefined behaviour

## Examples

```cpp
#include "simplex.hpp"
// use Sex macro
// or operator""_sex
{
// using boost regex/metaparse or whatever...
... regex_ex = "[a-zA-Z0-9_]+";
// using simplex...
static auto simplex_ex = Sex("+[-az-AZ-09_]");
// the Sex macro is not consteval in favor of convenience, but all of the parsing work is,
// if you really want to, you can use simplex::parse directly to declare a constexpr matcher
}{
... regex = "[a-zA-Z0-9_]{20,25}";
static auto simplex = Sex("{20,25}[-az-AZ-09_]");
}{
... regex = "[^a-zA-Z0-9_]";
auto simplex = Sex("![-az-AZ-09_]");
}{
... regex = "(?=[a-zA-Z0-9_])";
static auto simplex = Sex("-[-az-AZ-09_]");
}
```

## 📜 License

This project is licensed under [MIT](./LICENSE) or [Apache-2.0](./LICENSE-APACHE).
