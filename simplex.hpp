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

    /// simplex runtime input stream interface i.e. stack-based input
    template <typename T>
    struct Input
    {
        inline void pop()
        {
            static_assert(std::is_member_function_pointer<decltype(&T::pop)>::value, "simplex::Input T must have a pop() method");
            static_cast<T &>(*this).pop();
        }
        inline uchar peek()
        {
            static_assert(std::is_member_function_pointer<decltype(&T::peek)>::value, "simplex::Input T must have a peek() method");
            return static_cast<T &>(*this).peek();
        }
    };

    /// constexpr stack with a fixed size array
    template <size_t N>
    class Stack
    {
        size_t len{0};
        std::array<uchar, N> buf;

    public:
        constexpr Stack() {}
        explicit constexpr Stack(const std::array<uchar, N> &l) : len(N), buf(l) {}
        explicit constexpr Stack(std::array<uchar, N> &&l) : len(N), buf(std::move(l)) {}
        inline size_t size() const noexcept { return len; }
        static inline constexpr size_t max_size() noexcept { return N; }
        inline bool empty() const noexcept { return len == 0; }
        inline bool full() const noexcept { return len == N; }
        inline void clear() noexcept { len = 0; }
        inline void fill(const uchar v) noexcept
        {
            for (size_t j = 0; j < N; ++j)
                buf[j] = v;
            len = N;
        }
        inline void push(const uchar c)
        {
            if (full())
                throw std::runtime_error("simplex::Stack::push() overflow");
            buf[len++] = c;
        }
        inline void push_back(const uchar c) { push(c); }
        inline uchar &top()
        {
            if (empty())
                throw std::runtime_error("simplex::Stack::top() out of bounds access");
            return buf[len - 1];
        }
        inline uchar peek() const
        {
            if (empty())
                throw std::runtime_error("simplex::Stack::peek() out of bounds access");
            return buf[len - 1];
        }
        inline uchar pop()
        {
            if (empty())
                throw std::runtime_error("simplex::Stack::pop() out of bounds access");
            return buf[--len];
        }
        inline void set_len(size_t new_len)
        {
            if (new_len > N)
                throw std::runtime_error("simplex::Stack::set_len() overflow");
            len = new_len;
        }
        inline void drain(size_t n_drain)
        {
            if (n_drain > len)
                throw std::runtime_error("simplex::Stack::drain() overflow");
            len -= n_drain;
        }
        inline uchar &operator[](size_t idx)
        {
            return buf[idx];
        }
        inline uchar operator[](size_t idx) const
        {
            return buf[idx];
        }
    };

    namespace internal
    {
        /// simplex operators, 1xxx xxxx, flags are 1xxx xfff
        enum : uchar
        {
            // simplex NOT operator
            NOT = 0x81,
            // simplex QUANTIFY operator
            QUANTIFY = 0x88,
            // simplex ANY operator
            ANY,
            // simplex RANGE operator
            RANGE,
            // simplex END_ANY operator
            END_ANY,
        };

        /// for internal use, parse a stack of 3 characters into a single uchar
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

        /// for internal use, parsing state
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

        /// for internal use, parse state machine transition
        constexpr ParseState parse_step(const std::string_view &ex, size_t idx, ParseState::S s)
        {
            uchar c = ex[idx];
            switch (s)
            {
            case ParseState::NORMAL:
                switch (c)
                {
                case '!':
                    return ParseState{idx + 1, NOT, ParseState::NORMAL};
                case '*':
                    return ParseState{idx, QUANTIFY, ParseState::ZERO_OR_MORE_LEFT};
                case '+':
                    return ParseState{idx, QUANTIFY, ParseState::ONE_OR_MORE_LEFT};
                case '?':
                    return ParseState{idx, QUANTIFY, ParseState::ZERO_OR_ONE_LEFT};
                case '{':
                    return ParseState{idx + 1, QUANTIFY, ParseState::QUANTIFIER_LEFT};
                case '[':
                    return ParseState{idx + 1, ANY, ParseState::ANY_RANGES};
                case '\\':
                    return ParseState{idx + 2, (uchar)ex[idx + 1], ParseState::NORMAL};
                default:
                    return ParseState{idx + 1, c, ParseState::NORMAL};
                }
                break;
            case ParseState::ZERO_OR_MORE_LEFT:
                return ParseState{idx, (uchar)0, ParseState::ZERO_OR_MORE_RIGHT};
            case ParseState::ZERO_OR_MORE_RIGHT:
                return ParseState{idx + 1, (uchar)0xFF, ParseState::NORMAL};
            case ParseState::ONE_OR_MORE_LEFT:
                return ParseState{idx, (uchar)1, ParseState::ONE_OR_MORE_RIGHT};
            case ParseState::ONE_OR_MORE_RIGHT:
                return ParseState{idx + 1, (uchar)0xFF, ParseState::NORMAL};
            case ParseState::ZERO_OR_ONE_LEFT:
                return ParseState{idx, (uchar)0, ParseState::ZERO_OR_ONE_RIGHT};
            case ParseState::ZERO_OR_ONE_RIGHT:
                return ParseState{idx + 1, (uchar)1, ParseState::NORMAL};
            case ParseState::QUANTIFIER_LEFT:
            {
                uchar digits[3]{};
                size_t i = 0;
                for (; i < 3 && c >= '0' && c <= '9'; (++i, ++idx, c = ex[idx]))
                    digits[i] = c;
                if (c != ',')
                    throw std::logic_error("malformed simplex quantifier");
                return ParseState{idx + 1, (i == 0 ? (uchar)0 : stouc(digits, i)), ParseState::QUANTIFIER_RIGHT};
            }
            case ParseState::QUANTIFIER_RIGHT:
            {
                uchar digits[3]{};
                size_t i = 0;
                for (; i < 3 && c >= '0' && c <= '9'; (++i, ++idx, c = ex[idx]))
                    digits[i] = c;
                if (c != '}')
                    throw std::logic_error("malformed simplex quantifier");
                return ParseState{idx + 1, (i == 0 ? (uchar)0xFF : stouc(digits, i)), ParseState::NORMAL};
            }
            case ParseState::ANY_RANGES:
                switch (c)
                {
                case '-':
                    return ParseState{idx + 1, RANGE, ParseState::ANY_RANGE_LEFT};
                case '\\':
                    return ParseState{idx + 2, (uchar)ex[idx + 1], ParseState::ANY_LITERALS};
                case ']':
                    return ParseState{idx + 1, END_ANY, ParseState::NORMAL};
                default:
                    return ParseState{idx + 1, c, ParseState::ANY_LITERALS};
                }
                break;
            case ParseState::ANY_LITERALS:
                switch (c)
                {
                case '\\':
                    return ParseState{idx + 2, (uchar)ex[idx + 1], ParseState::ANY_LITERALS};
                case ']':
                    return ParseState{idx + 1, END_ANY, ParseState::NORMAL};
                default:
                    return ParseState{idx + 1, c, ParseState::ANY_LITERALS};
                }
                break;
            case ParseState::ANY_RANGE_LEFT:
                switch (c)
                {
                case '\\':
                    return ParseState{idx + 2, (uchar)ex[idx + 1], ParseState::ANY_RANGE_RIGHT};
                case ']':
                    throw std::logic_error("malformed simplex range");
                default:
                    return ParseState{idx + 1, c, ParseState::ANY_RANGE_RIGHT};
                }
                break;
            case ParseState::ANY_RANGE_RIGHT:
                switch (c)
                {
                case '\\':
                    return ParseState{idx + 2, (uchar)ex[idx + 1], ParseState::ANY_RANGES};
                case ']':
                    throw std::logic_error("malformed simplex range");
                default:
                    return ParseState{idx + 1, c, ParseState::ANY_RANGES};
                }
                break;
            default:
                throw std::logic_error("invalid simplex parsing state");
            }
        }

        /// for internal use, helper function for validating no unterminated ending quantifiers
        template <uchar c0, uchar c1, uchar c2, uchar... chars>
        inline constexpr bool terminated_quantifier()
        {
            return c2 != internal::QUANTIFY;
        }

        /// for internal use, helper function for validating no ending operators
        template <uchar c0, uchar... chars>
        inline constexpr bool terminated()
        {
            return c0 < (uchar)0x80 || c0 == END_ANY;
        }

        /// for internal use, matching on any group
        template <size_t N>
        bool any(const Stack<N> &stack, size_t &any_idx, const uchar cur, uchar scur)
        { // assume we are one past the ANY operator
            any_idx = stack.size();
            for (char left, right; scur == RANGE; scur = stack[--any_idx])
            { // ranges are always two characters, otherwise it's malformed
                left = stack[--any_idx];
                right = stack[--any_idx];
                if (cur >= left && cur <= right)
                    return true;
            }
            for (; scur != END_ANY; scur = stack[--any_idx])
            { // everything else is a literal until END_ANY, if a range follows it's ignored
                if (cur == scur)
                    return true;
            }
            return false;
        }

        /// for internal use, matching on a quantified group
        template <typename T, size_t N>
        bool quantify(Input<T> &in, const uint8_t min, const uint8_t max, Stack<N> &stack, size_t &any_idx, uchar &cur, uchar &scur)
        { // assume we are one past the QUANTIFY operator
            uint8_t cnt = 0;
            if (scur == ANY)
            { // repeat "any" until we don't match or we reach the max
                scur = stack.pop();
                for (; cnt <= max; ++cnt, in.pop(), cur = in.peek())
                {
                    if (any(stack, any_idx, cur, scur))
                        continue;
                    while (stack[any_idx] != internal::END_ANY)
                        --any_idx;
                    return (stack.set_len(any_idx + 1), cnt >= min);
                }
                stack.set_len(any_idx + 1);
            }
            else
            { // match the next character until we don't match or we reach the max
                for (; cnt <= max; ++cnt, in.pop(), cur = in.peek())
                {
                    if (scur == cur)
                        continue;
                    return cnt >= min;
                }
            }
            return false;
        }
#if __cplusplus >= 202002L
        /// for internal use, just a string_view for operator""_sex
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
    }

    /// a parsed simplex expression that can be used to match against an input stream with match()
    template <size_t N>
    class Simplex
    {
        Stack<N> stack;

    public:
        constexpr Simplex() = delete;
        constexpr Simplex<N> &operator=(const Simplex<N> &) = delete;
        /// for internal use, create a simplex matcher with a parsed expression's operator stack
        explicit constexpr Simplex(const std::array<uchar, N> &stack) : stack(stack) {}
        /// for internal use, create a simplex matcher with a parsed expression's operator stack
        explicit constexpr Simplex(std::array<uchar, N> &&stack) : stack(std::move(stack)) {}
        /// match a simplex expression against an input stream
        template <typename T>
        bool match(Input<T> &in)
        {
            // current stack index and "any" index
            size_t any_idx{stack.size()};
            // current input character and stack character
            uchar cur{in.peek()}, scur{stack.pop()};
            uchar flags = 0;
            uint8_t min = 1, max = 1;
            bool b;
            for (; !stack.empty(); scur = stack.pop())
            {
                if (scur > (uchar)0x80) // 0x80 is the boundary between parsed operators and literals
                {
                    // set flags
                    if (scur <= (uchar)0x87) // (f=flag) 1xxx xfff, so 0x81 <= flags <= 0x87
                    {
                        flags = scur & (uchar)0x7F; // clear the top bit so we only have the flags
                        scur = stack.pop();
                    }
                    // set quantifier
                    if (scur == internal::QUANTIFY)
                    { // quantified
                        min = stack.pop();
                        max = stack.pop();
                        scur = stack.pop();
                        b = internal::quantify<T, N>(in, min, max, stack, any_idx, cur, scur);
                        b = (flags & (uint8_t)internal::NOT) ? !b : b;
                        if (!b)
                            return false;
                        flags = 0;
                        continue;
                    }
                    // not quantified
                    if (scur == internal::ANY)
                    {
                        scur = stack.pop();
                        b = internal::any<N>(stack, any_idx, cur, scur);
                        while (stack[any_idx] != internal::END_ANY)
                            --any_idx;
                        stack.set_len(any_idx);
                    }
                    else
                    {
                        b = cur == scur;
                    }
                    b = (flags & (uint8_t)internal::NOT) ? !b : b;
                    if (!b)
                        return false;
                    in.pop(), cur = in.peek(), flags = 0;
                    continue;
                }
                // optimized for no flags or quantifiers
                if (cur != scur)
                    return false;
                in.pop(), cur = in.peek();
            }
            // we reached the end of the expression, we do not check for any remaining input
            return true;
        }
    };

    /// parse a simplex expression
    template <const std::string_view &expr, size_t idx = 0, internal::ParseState::S state = internal::ParseState::NORMAL, uchar... chars>
    inline constexpr auto parse()
    {
        if constexpr (idx > expr.size())
        { // string_view operator[] should throw out of bounds but just in case
            throw std::logic_error("malformed simplex expression");
        }
        else if constexpr (idx == expr.size())
        { // finished parsing, validating...
            if constexpr (state == internal::ParseState::NORMAL && (sizeof...(chars) < 3 || internal::terminated_quantifier<chars...>()) && (sizeof...(chars) < 1 || internal::terminated<chars...>()))
            {
                return Simplex<sizeof...(chars)>({chars...});
            }
            else
            {
                throw std::logic_error("simplex unterminated operator sequence");
            }
        }
        else
        { // parse the next operator/character
            constexpr internal::ParseState next_state = parse_step(expr, idx, state);
            return parse<expr, next_state.idx, next_state.state, next_state.res, chars...>();
        }
    }
}; // namespace simplex

#if __cplusplus >= 202002L
template <::simplex::internal::NotAStringView x>
inline auto operator""_sex()
{
    static constexpr ::std::string_view sv{x.to_string_view()};
    return ::simplex::parse<sv>();
}
#endif

#endif // SIMPLEX_HPP
