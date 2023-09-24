/**
 * @file simplex.hpp
 * @copyright
 * Copyright 2023 Lance Warden.
 * Licensed under MIT or Apache 2.0 License, see LICENSE-MIT or LICENSE-APACHE for details.
 * @brief A simple, comptime-parsed, stack-based, one-character lookahead regex. (C++17 and later)
 */
#ifndef SIMPLEX_HPP
#define SIMPLEX_HPP
#define QUANTIFY_MAX 0xFE
#define QUANTIFY_INF 0xFF

#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <array>

#ifndef SIMPLEX_INF
#define SIMPLEX_INF 0x0FFF
#endif
#define Sex(expr) ([]() constexpr { \
    using namespace ::std::string_view_literals; \
    constexpr ::std::string_view x = expr##sv; \
    constexpr auto matcher = ::simplex::Simplex<x.size() + 1>(x); \
    return matcher; })()

/**
 * @brief A simple, comptime-parsed, stack-based, one-character lookahead regex. (Minimum Supported Version C++17)
 * @attention only basic ascii string literal expressions are supported and expression validation is not guaranteed
 */
namespace simplex
{
    using uchar = unsigned char;

    /// simplex runtime stack-based input interface
    template <typename T>
    struct Input
    {
        inline void pop()
        {
            static_assert(std::is_member_function_pointer<decltype(&T::pop)>::value, "simplex::Input T must have a pop() method");
            static_cast<T *>(this)->pop();
        }
        inline uchar peek()
        {
            static_assert(std::is_member_function_pointer<decltype(&T::peek)>::value, "simplex::Input T must have a peek() method");
            return static_cast<T *>(this)->peek();
        }
    };

    namespace internal
    {
        template <typename Iterator>
        struct is_uchar_iterator : public std::bool_constant<
                                       std::is_same<typename std::iterator_traits<Iterator>::value_type, uchar>::value &&
                                       std::is_same<typename std::iterator_traits<Iterator>::difference_type, ptrdiff_t>::value &&
                                       std::is_base_of<std::random_access_iterator_tag, typename std::iterator_traits<Iterator>::iterator_category>::value &&
                                       std::is_copy_assignable<Iterator>::value>
        {
        };

        enum : uchar
        {
            // reserve 0x81 - 0x87 for flags (1000 0xxx)
            NOT = 0x81,
            QUANTIFY = 0x88,
            ZERO_OR_MORE,
            ONE_OR_MORE,
            ZERO_OR_ONE,
            ANY,
            RANGE,
            END_ANY,
            END,
        };

        inline constexpr bool test_flag(const uchar flags, uchar flag)
        {
            return (flags & flag) != uchar(0);
        }

        constexpr uchar stouc(const uchar (&stack)[3], size_t i)
        {
            switch (i)
            {
            case 1:
                return stack[0] - '0';
            case 2:
                return (stack[0] - '0') * 10 + (stack[1] - '0');
            case 3:
            {
                uint16_t res = uint16_t(stack[0] - '0') * 100 + uint16_t(stack[1] - '0') * 10 + uint16_t(stack[2] - '0');
                if (res > uint16_t(QUANTIFY_MAX))
                    throw std::logic_error("malformed simplex quantifier, maximum bound is 254");
                return (uchar)res;
            }
            }
        }

        /// for internal use, matching on any group
        template <typename Iterator>
        bool any(const Iterator &begin, Iterator &any_begin, const uchar cur, uchar scur)
        { // assume we have already read ANY operator
            any_begin = begin;
            scur = *++any_begin;
            for (; scur == RANGE; scur = *++any_begin)
            { // ranges are always two characters, otherwise it's malformed
                if (cur >= *++any_begin && cur <= *++any_begin)
                    return true;
            }
            for (; scur != END_ANY; scur = *++any_begin)
            { // everything else is a literal until END_ANY, if a range follows it's ignored
                if (cur == scur)
                    return true;
            }
            return false;
        }

        /// for internal use, matching on a quantified group
        template <typename Iterator, typename T>
        bool quantify(Iterator &begin, Iterator &any_begin, Input<T> &in, uchar &cur, uchar &scur, const uint16_t min, uint16_t max)
        { // assume we have already read QUANTIFY operator
            scur = *++begin;
            uint16_t cnt{0}, flags{0};
            bool res{false};
            if (max == uint16_t(QUANTIFY_INF))
                max = uint16_t(SIMPLEX_INF);
            while (cnt <= max)
            {
                switch (scur)
                {
                // case FLAGS: fallthrough...
                case NOT:
                    flags |= scur & uchar(0x7F), scur = *++begin;
                    continue;
                case QUANTIFY:
                case ZERO_OR_MORE:
                case ONE_OR_MORE:
                case ZERO_OR_ONE:
                    throw std::logic_error("malformed simplex quantifier, nested quantifiers are not allowed");
                case ANY:
                    res = any<Iterator>(begin, any_begin, cur, scur);
                    break;
                default:
                    res = cur == scur;
                    break;
                }
                if (!(test_flag(flags, NOT) ^ res))
                    return cnt >= min;
                ++cnt, in.pop(), cur = in.peek();
            }
            return false;
        }

#if __cplusplus >= 202002L
        /// for internal use, just a string_view for operator""_sex
        template <size_t N>
        struct NotAStringView
        {
            const char str[N - 1]{};
            constexpr NotAStringView(const char (&str)[N])
            {
                for (size_t i = 0; i < N - 1; ++i)
                    this->str[i] = str[i];
            }
            inline constexpr std::string_view to_string_view() const { return std::string_view(str, N - 1); }
        };
#endif
    } // namespace internal

    /// for internal use, parse expression into a container,
    ///
    /// container MUST have capacity of at least (`expression.size() + 1`)
    ///
    /// returns an iterator to the end of the filled portion of the container
    template <typename Iterator>
    constexpr Iterator parse(const std::string_view &x, Iterator begin)
    {
        using namespace internal;
        static_assert(is_uchar_iterator<Iterator>::value, "simplex::parse() Iterator must be a pointer-like iterator of unsigned char");
        const Iterator _begin = begin;
        size_t xi{0};
        uchar c{0};
        for (; xi < x.size(); ++xi, ++begin)
        {
            c = x[xi];
            switch (c)
            {
            case '\\':
                *begin = x[++xi];
                break;
                // case FLAG: fallthrough...
            case '!':
                switch (c)
                {
                case '!':
                    *begin |= NOT;
                    break;
                }
                for (c = x[xi + 1];; c = x[++xi + 1])
                {
                    switch (c)
                    {
                    case '!':
                        *begin |= NOT;
                        continue;
                    }
                    break;
                }
                break;
            case '*':
                *begin = ZERO_OR_MORE;
                break;
            case '+':
                *begin = ONE_OR_MORE;
                break;
            case '?':
                *begin = ZERO_OR_ONE;
                break;
            case '{':
            {
                *begin = QUANTIFY;
                uchar digits[3]{};
                size_t i = 0;
                for (c = x[++xi]; i < 3 && c >= '0' && c <= '9'; ++i, c = x[++xi])
                    digits[i] = c;
                if (c != ',')
                    throw std::logic_error("malformed simplex quantifier, exptected ','");
                *++begin = (i == 0 ? '\x00' : stouc(digits, i));
                i = 0;
                for (c = x[++xi]; i < 3 && c >= '0' && c <= '9'; ++i, c = x[++xi])
                    digits[i] = c;
                if (c != '}')
                    throw std::logic_error("malformed simplex quantifier, exptected '}'");
                *++begin = (i == 0 ? '\xFF' : stouc(digits, i));
                continue;
            }
            case '[':
            {
                *begin = ANY;
                c = x[++xi];
                for (; c == '-'; c = x[++xi])
                {
                    *++begin = RANGE;
                    if (x[++xi] == ']')
                        throw std::logic_error("malformed simplex range, ']' must be escaped within a range");
                    *++begin = x[xi] == '\\' ? x[++xi] : x[xi];
                    if (x[++xi] == ']')
                        throw std::logic_error("malformed simplex range, ']' must be escaped within a range");
                    *++begin = x[xi] == '\\' ? x[++xi] : x[xi];
                }
                for (; c != ']'; c = x[++xi])
                {
                    *++begin = x[xi] == '\\' ? x[++xi] : x[xi];
                }
                *++begin = END_ANY;
                continue;
            }
            default:
                *begin = c;
                continue;
            }
        }
        *begin = END;
        ptrdiff_t len = begin - _begin;
        bool unterminated_quantify = len > 3 && *(begin - 3) == QUANTIFY;
        bool unterminated_flag = len > 1 && *(begin - 1) == NOT;
        if (unterminated_quantify || unterminated_flag)
            throw std::logic_error("unterminated simplex operator");
        return ++begin;
    }

    /// match a simplex expression against an input stream
    template <typename Iterator, typename T>
    bool matches(Iterator begin, Input<T> &in)
    {
        using namespace internal;
        static_assert(is_uchar_iterator<Iterator>::value, "simplex::match() Iterator must be a pointer-like iterator of unsigned char");
        Iterator any_begin = begin;
        uchar cur = in.peek(), scur = *begin, flags{0};
        bool res;
        for (; scur != END; scur = *++begin)
        {
            switch (scur)
            {
            // case FLAGS: fallthrough...
            case NOT:
                // flags only set the next matching unit
                flags |= scur & uchar(0x7F);
                continue;
            case QUANTIFY:
            {
                uchar min = *++begin, max = *++begin;
                res = quantify<Iterator, T>(begin, any_begin, in, cur, scur, min, max);
                break;
            }
            case ZERO_OR_MORE:
                res = quantify<Iterator, T>(begin, any_begin, in, cur, scur, 0, QUANTIFY_INF);
                break;
            case ONE_OR_MORE:
                res = quantify<Iterator, T>(begin, any_begin, in, cur, scur, 1, QUANTIFY_INF);
                break;
            case ZERO_OR_ONE:
                res = quantify<Iterator, T>(begin, any_begin, in, cur, scur, 0, 1);
                break;
            case ANY:
                res = any<Iterator>(begin, any_begin, cur, scur);
                if (!(test_flag(flags, NOT) ^ res))
                    return false;
                while (*any_begin != END_ANY)
                    ++any_begin;
                begin = any_begin;
                in.pop(), cur = in.peek(), flags = 0;
                continue;
            default:
                res = cur == scur, in.pop(), cur = in.peek();
                break;
            }
            if (!(test_flag(flags, NOT) ^ res))
                return false;
            if (any_begin > begin)
            {
                while (*any_begin != END_ANY)
                    ++any_begin;
                begin = any_begin;
            }
            flags = 0;
        }
        // we reached the end of the expression, we do not check for any remaining input
        return true;
    }

    template <size_t N>
    /// a contexpr-parsed simplex expression that can be used to match against an input stream with matches()
    class Simplex
    {
        std::array<uchar, N> arr{};

    public:
        constexpr Simplex() = delete;
        constexpr Simplex<N> &operator=(const Simplex<N> &) = delete;
        constexpr Simplex(const std::string_view &expr)
        {
            if (expr.size() + 1 > N)
                throw std::logic_error("simplex expression too large for Simplex container");
            parse(expr, arr.begin());
        }
        /// match a simplex expression against an input stream
        template <typename T>
        inline bool matches(Input<T> &in) const
        {
            return simplex::matches(arr.begin(), in);
        }
    };
}; // namespace simplex

#if __cplusplus >= 202002L
template <::simplex::internal::NotAStringView _x>
inline constexpr auto operator""_Sex()
{
    constexpr ::std::string_view x{_x.to_string_view()};
    constexpr auto matcher = ::simplex::Simplex<x.size() + 1>(x);
    return matcher;
}
#endif

#endif // SIMPLEX_HPP
