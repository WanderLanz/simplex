/**
 * @file simplex.hpp
 * @copyright
 * Copyright 2023 Lance Warden.
 * Licensed under MIT or Apache 2.0 License, see LICENSE-MIT or LICENSE-APACHE for details.
 * @brief A simple, comptime-parsed, stack-based, one-character lookahead regex. (C++17 and later)
 */
#ifndef SIMPLEX_HPP
#define SIMPLEX_HPP
#include <cstdint>
#include <string_view>
#include <array>
#include <stdexcept>

#define Sex(expr) ([]() { \
    using namespace ::std::string_view_literals; \
    static constexpr ::std::string_view x = expr##sv; \
    return ::simplex::parse<x>(); })()

/**
 * @brief A simple, comptime-parsed, stack-based, one-character lookahead regex. (Minimum Supported Version C++17)
 * @attention only basic ascii string literal expressions are supported and expression validation is not guaranteed
 */
namespace simplex
{
    using uchar = unsigned char;

    // simplex runtime input stream interface i.e. stack-based input
    template <typename T>
    struct Input
    {
        inline uchar get()
        {
            static_assert(std::is_member_function_pointer<decltype(&T::get)>::value, "simplex Input T must have a get() method");
            return static_cast<T &>(*this).get();
        }
        inline uchar peek()
        {
            static_assert(std::is_member_function_pointer<decltype(&T::peek)>::value, "simplex Input T must have a peek() method");
            return static_cast<T &>(*this).peek();
        }
    };

    // constexpr stack with a fixed size array
    template <size_t N>
    class Stack
    {
        size_t len{0};
        std::array<uchar, N> buf;

    public:
        constexpr Stack() {}
        constexpr Stack(const std::array<uchar, N> &l) : len(N), buf(l) {}
        inline constexpr size_t size() const noexcept { return len; }
        inline constexpr size_t max_size() const noexcept { return N; }
        inline constexpr bool empty() const noexcept { return len == 0; }
        inline constexpr bool full() const noexcept { return len == N; }
        inline constexpr void clear() noexcept { len = 0; }
        inline constexpr void fill(const uchar v) noexcept
        {
            for (size_t j = 0; j < N; ++j)
                buf[j] = v;
            len = N;
        }
        inline constexpr void push(const uchar c)
        {
            if (full())
                throw std::logic_error("simplex stack overflow");
            buf[len++] = c;
        }
        inline constexpr void push_back(const uchar c) { push(c); }
        inline constexpr uchar &top()
        {
            if (empty())
                throw std::logic_error("empty simplex stack access via top()");
            return buf[len - 1];
        }
        inline constexpr uchar peek() const
        {
            if (empty())
                throw std::logic_error("empty simplex stack access via peek()");
            return buf[len - 1];
        }
        inline constexpr uchar pop()
        {
            if (empty())
                throw std::logic_error("empty simplex stack access via pop()");
            return buf[--len];
        }
        inline constexpr void set_len(size_t new_len)
        {
            if (new_len > N)
                throw std::logic_error("simplex stack set_len() cannot increase length beyond max_size()");
            len = new_len;
        }
        inline constexpr void drain(size_t n_drain)
        {
            if (n_drain > len)
                throw std::logic_error("simplex stack drain() cannot drain more than current length");
            len -= n_drain;
        }
        inline constexpr uchar &operator[](size_t idx)
        {
            if (idx >= len)
                throw std::logic_error("simplex stack access out of bounds");
            return buf[idx];
        }
        inline constexpr uchar operator[](size_t idx) const
        {
            if (idx >= len)
                throw std::logic_error("simplex stack access out of bounds");
            return buf[idx];
        }
    };

    namespace internal
    {
        // simplex operators
        enum : uchar
        {
            // simplex NOT operator: flag 0001
            NOT = 0x81,
            // simplex PEEK operator: flag 0010
            PEEK = 0x82,
            // simplex QUANTIFY operator
            QUANTIFY = 0x87,
            // simplex ANY operator
            ANY = 0x88,
            // simplex RANGE operator
            RANGE = 0x89,
            // simplex END_ANY operator
            END_ANY = 0x8A,
        };

        // for internal use, parse a stack of 3 characters into a single uchar
        constexpr uchar stouc(const uchar (&stack)[3], size_t i)
        {
            switch (i)
            {
            case 1:
                return stack[0] - '0';
            case 2:
                return (stack[0] - '0') * 10 + (stack[1] - '0');
            case 3:
                return (stack[0] - '0') * 100 + (stack[1] - '0') * 10 + (stack[2] - '0');
            default:
                throw std::logic_error("invalid simplex quantifier");
            }
        }

        // for internal use, parsing state
        struct ParseState
        {
            size_t idx;
            uchar res;
            enum S
            {
                NORMAL,
                ZERO_OR_MORE_LEFT,
                ZERO_OR_MORE_RIGHT,
                ONE_OR_MORE_LEFT,
                ONE_OR_MORE_RIGHT,
                ZERO_OR_ONE_LEFT,
                ZERO_OR_ONE_RIGHT,
                QUANTIFIER_LEFT,
                QUANTIFIER_RIGHT,
                ANY_RANGES,
                ANY_LITERALS,
                ANY_RANGE_LEFT,
                ANY_RANGE_RIGHT,
            } state{NORMAL};
        };

        // for internal use, parse state machine transition
        constexpr ParseState parse_step(const std::string_view &ex, size_t idx, ParseState::S s)
        {
            uchar res = 0;
            uchar c = ex[idx];
            switch (s)
            {
            case ParseState::NORMAL:
                switch (c)
                {
                case '!':
                case '-':
                {
                    size_t sentinel = 0;
                    for (; sentinel < 10; (++idx, c = ex[idx]))
                    {
                        switch (c)
                        {
                        case '!':
                            res |= (uchar)NOT;
                            continue;
                        case '-':
                            res |= (uchar)PEEK;
                            continue;
                        }
                        break;
                    }
                    if (sentinel >= 10) // should never happen... but just in case
                        throw std::logic_error("simplex parser reached flag parsing recursion limit, you only need a maximum of 2 flags per operation");
                    s = ParseState::NORMAL;
                    break;
                }
                case '*':
                    res = QUANTIFY;
                    return ParseState{idx, res, ParseState::ZERO_OR_MORE_LEFT};
                case '+':
                    res = QUANTIFY;
                    return ParseState{idx, res, ParseState::ONE_OR_MORE_LEFT};
                case '?':
                    res = QUANTIFY;
                    return ParseState{idx, res, ParseState::ZERO_OR_ONE_LEFT};
                case '{':
                    res = QUANTIFY;
                    s = ParseState::QUANTIFIER_LEFT;
                    break;
                case '[':
                    res = ANY;
                    s = ParseState::ANY_RANGES;
                    break;
                case '\\':
                    (++idx, res = ex[idx]);
                    break;
                default:
                    res = c;
                    break;
                }
                break;
            case ParseState::ZERO_OR_MORE_LEFT:
                res = 0;
                return ParseState{idx, res, ParseState::ZERO_OR_MORE_RIGHT};
            case ParseState::ZERO_OR_MORE_RIGHT:
                res = 0xFF;
                s = ParseState::NORMAL;
                break;
            case ParseState::ONE_OR_MORE_LEFT:
                res = 1;
                return ParseState{idx, res, ParseState::ONE_OR_MORE_RIGHT};
            case ParseState::ONE_OR_MORE_RIGHT:
                res = 0xFF;
                s = ParseState::NORMAL;
                break;
            case ParseState::ZERO_OR_ONE_LEFT:
                res = 0;
                return ParseState{idx, res, ParseState::ZERO_OR_ONE_RIGHT};
            case ParseState::ZERO_OR_ONE_RIGHT:
                res = 1;
                s = ParseState::NORMAL;
                break;
            case ParseState::QUANTIFIER_LEFT:
            {
                uchar digits[3]{};
                size_t i = 0;
                for (; i < 3 && c >= '0' && c <= '9'; (++i, ++idx, c = ex[idx]))
                    digits[i] = c;
                if (c != ',')
                    throw std::logic_error("malformed simplex quantifier");
                res = i == 0 ? (uchar)0 : stouc(digits, i);
                s = ParseState::QUANTIFIER_RIGHT;
                break;
            }
            case ParseState::QUANTIFIER_RIGHT:
            {
                uchar digits[3]{};
                size_t i = 0;
                for (; i < 3 && c >= '0' && c <= '9'; (++i, ++idx, c = ex[idx]))
                    digits[i] = c;
                if (c != '}')
                    throw std::logic_error("malformed simplex quantifier");
                res = i == 0 ? (uchar)0xFF : stouc(digits, i);
                s = ParseState::NORMAL;
                break;
            }
            case ParseState::ANY_RANGES:
                switch (c)
                {
                case '-':
                    res = RANGE;
                    s = ParseState::ANY_RANGE_LEFT;
                    break;
                case '\\':
                    (++idx, res = ex[idx]);
                    s = ParseState::ANY_LITERALS;
                    break;
                case ']':
                    res = END_ANY;
                    s = ParseState::NORMAL;
                    break;
                default:
                    res = c;
                    s = ParseState::ANY_LITERALS;
                    break;
                }
                break;
            case ParseState::ANY_LITERALS:
                switch (c)
                {
                case '\\':
                    (++idx, res = ex[idx]);
                    break;
                case ']':
                    res = END_ANY;
                    s = ParseState::NORMAL;
                    break;
                default:
                    res = c;
                    break;
                }
                break;
            case ParseState::ANY_RANGE_LEFT:
                switch (c)
                {
                case '\\':
                    (++idx, res = ex[idx]);
                    s = ParseState::ANY_RANGE_RIGHT;
                    break;
                case ']':
                    throw std::logic_error("malformed simplex range");
                default:
                    res = c;
                    s = ParseState::ANY_RANGE_RIGHT;
                    break;
                }
                break;
            case ParseState::ANY_RANGE_RIGHT:
                switch (c)
                {
                case '\\':
                    (++idx, res = ex[idx]);
                    s = ParseState::ANY_RANGES;
                    break;
                case ']':
                    throw std::logic_error("malformed simplex range");
                default:
                    res = c;
                    s = ParseState::ANY_RANGES;
                    break;
                }
                break;
            default:
                throw std::logic_error("invalid simplex parsing state");
            }
            ++idx;
            return ParseState{idx, res, s};
        }

        // for internal use, matching on any group
        template <size_t N>
        bool any(const Stack<N> &stack, size_t &any_idx, uchar cur, uchar scur)
        { // assume we are one past the ANY operator
            any_idx = stack.size();
            for (char left, right; scur == RANGE; (--any_idx, scur = stack[any_idx]))
            { // ranges are always two characters, otherwise it's malformed
                (--any_idx, left = stack[any_idx]);
                (--any_idx, right = stack[any_idx]);
                if (cur >= left && cur <= right)
                    return true;
            }
            for (; scur != END_ANY; (--any_idx, scur = stack[any_idx]))
            { // everything else is a literal until END_ANY, if a range follows it's ignored
                if (cur == scur)
                    return true;
            }
            return false;
        }

        // for internal use, matching on a qualified group
        template <typename T, size_t N>
        bool qualify(Input<T> &in, uchar (Input<T>::*next)(), const uint8_t min, const uint8_t max, Stack<N> &stack, size_t &any_idx, uchar &cur, uchar &scur)
        { // assume we are one past the QUANTIFY operator
            size_t cnt = 0;
            cur = (in.*next)();
            if (scur == ANY)
            { // repeat "any" until we don't match or we reach the max
                scur = stack.pop();
                for (; cnt <= max; (++cnt, cur = (in.*next)()))
                {
                    if (!any(stack, any_idx, cur, scur))
                        break;
                }
            }
            else
            { // match the next character until we don't match or we reach the max
                for (; cnt <= max; (++cnt, cur = (in.*next)()))
                {
                    if (scur != cur)
                        break;
                }
            }
            return cnt >= min && cnt <= max;
        }

        // for internal use, match a pre-parsed simplex expression against an input stream
        template <typename T, size_t N>
        bool matches(Input<T> &in, const Stack<N> &_stack)
        {
            Stack<N> stack(_stack);
            // current stack index and "any" index
            size_t any_idx{stack.size()};
            // current input character and stack character
            uchar cur{0}, scur{stack.pop()};
            uchar flags = 0;
            uint8_t min = 1, max = 1;
            bool b;
            for (; !stack.empty(); (scur = stack.pop()))
            {
                if ((uchar)scur > (uchar)0x80) // 0x80 is the boundary between parsed operators and literals
                {
                    // set flags
                    if ((uchar)scur <= (uchar)0x83) // (f=flag) 1xxx xxff, so 0x81 <= flags <= 0x83
                    {
                        flags = scur;
                        scur = stack.pop();
                    }
                    uchar (Input<T>::*next)() = (flags & (uchar)PEEK) ? &Input<T>::peek : &Input<T>::get;
                    // set quantifier
                    if (scur == QUANTIFY)
                    { // quantified
                        min = stack.pop();
                        max = stack.pop();
                        scur = stack.pop();
                        b = qualify<T, N>(in, next, min, max, stack, any_idx, cur, scur);
                        if (any_idx < stack.size()) // we matched "any" and need to skip ahead to the ']' character
                            stack.set_len(any_idx);
                        if (flags & (uint8_t)NOT)
                        {
                            if (b)
                                return false;
                        }
                        else
                        {
                            if (!b)
                                return false;
                        }
                        flags = 0;
                        continue;
                    }
                    // not qualified
                    cur = (in.*next)();
                    b = (scur == ANY) ? (scur = stack.pop(), any<N>(stack, any_idx, cur, scur)) : cur == scur;
                    if (flags & (uint8_t)NOT)
                    {
                        if (b)
                            return false;
                    }
                    else
                    {
                        if (!b)
                            return false;
                    }
                    flags = 0;
                }
                // optimized for no flags or qualifiers
                if (in.get() != scur)
                    return false;
            }
            // we reached the end of the expression, we do not check for any remaining input
            return true;
        }

        // for internal use, a parsed simplex expression
        template <uchar... chars>
        class Parsed
        {
            static constexpr Stack<sizeof...(chars)> stack{{chars...}};

        public:
            /**
             * @brief match a simplex expression against an input stream
             */
            template <typename T>
            static bool match(Input<T> &in)
            {
                return matches<T, sizeof...(chars)>(in, stack);
            }
        };
    }

    // parse a simplex expression
    template <const std::string_view &expr, size_t idx = 0, internal::ParseState::S state = internal::ParseState::NORMAL, uchar... chars>
    inline constexpr auto parse()
    {
        if constexpr (idx > expr.size())
        {
            throw std::logic_error("malformed simplex expression");
        }
        else if constexpr (idx == expr.size())
        {
            if constexpr (state == internal::ParseState::NORMAL)
            {
                return internal::Parsed<chars...>();
            }
            else
            {
                throw std::logic_error("simplex unterminated operator sequence");
            }
        }
        else
        {
            constexpr internal::ParseState next_state = parse_step(expr, idx, state);
            return parse<expr, next_state.idx, next_state.state, next_state.res, chars...>();
        }
    }
#if __cplusplus >= 202002L
    // for internal use, just a string_view for operator""_sex
    template <size_t N>
    struct NotAStringView
    {
        char str[N]{};
        constexpr NotAStringView(char const (&str)[N])
        {
            for (size_t i = 0; i < N; ++i)
                this->str[i] = str[i];
        }
        inline constexpr std::string_view to_string_view() const { return std::string_view(str, N); }
    };
#endif
}; // namespace simplex

#if __cplusplus >= 202002L
template <::simplex::NotAStringView x>
inline auto operator""_sex()
{
    static constexpr ::std::string_view sv{x.to_string_view()};
    return ::simplex::parse<sv>();
}
#endif

#endif // SIMPLEX_HPP
