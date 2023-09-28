# simplex (simple expression) - v1.0.0

A simple, overkill, constexpr-parsed, one-character lookahead regex, i.e. a toy I made for fun 😝.

Should probably just learn Boost Regex/Metaparse or something, but I needed to remind myself why I don't like c/c++ macros/templates.

simply copy [simplex.hpp] into your includes, create a `Simplex` matcher, and `Simplex::matches(...)`. You can also use `simplex::parse` and `simplex::matches` directly.

## Notes

- no backtracking or capture groups
- only basic ascii (0x00-0x7F) string literal expressions
- does not exhaust input, only matches the immediate beginning of the input, similar to std::regex_match
- special characters include R"\\!\*+?{,}\[-]", in that order of precedence (escape '\\', flags "!", quantifiers "*+?{,}", any-group "\[-]")
- "!" is a negation flag, i.e. "! " matches any non-space character
- '\\' escapes the next character, e.g. "\\\*" matches a literal '*'
- within "\[]", all ranges "-az" must precede any literals "a".
- within "{}", only digits or ',' is valid.
- unlike regex, operators must precede the character or group it modifies, i.e. stack-based
- there is a max of 254 for any quantifier bound, e.g. "{0,254}", and "{,}" is equivalent to "{0,SIMPLEX_INF}"
- no predefined special character classes are provided, e.g. ".\b\B\\<\\>\c\s\S\d\D\w\W\x\O"

## Examples

```cpp
#include "simplex.hpp"
{
// regex
// "[a-zA-Z0-9_]+"

// simplex
constexpr auto matcher = Simplex("+[-az-AZ-09_]");

assert(matcher.matches("foobar"));
}{
// "[a-zA-Z0-9_]{20,25}"
constexpr auto matcher = Simplex("{20,25}[-az-AZ-09_]");
assert(!matcher.matches("foobar"));
}{
// "[^a-zA-Z0-9_]"
constexpr auto matcher = Simplex("![-az-AZ-09_]");
// only actually matches against the first character
assert(matcher.matches("     "));
}
```

## 📜 License

This project is licensed under [MIT](./LICENSE) or [Apache-2.0](./LICENSE-APACHE).
