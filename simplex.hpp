/**
 * @file simplex.hpp
 * @copyright
 * Copyright 2023 Lance Warden.
 * Licensed under MIT or Apache 2.0 License, see LICENSE-MIT or LICENSE-APACHE for details.
 * @brief A simple, comptime-parsed, one-character lookahead regex (C++17).
 */
#ifndef SIMPLEX_HPP
#define SIMPLEX_HPP
#define SIMPLEX_QUANTIFY_MAX 0xFE
#define SIMPLEX_QUANTIFY_INF 0xFF

#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <string_view>

#ifndef SIMPLEX_INF
#define SIMPLEX_INF 0x0FFF
#endif

/// @brief A simple, comptime-parsed,, one-character lookahead regex (C++17).
/// @attention only basic ascii string literal expressions are supported (0x00-0x7F) and expression validation is not guaranteed
namespace simplex
{
    /// @brief internal namespace for simplex
    namespace internal
    {
        using uchar = unsigned char;

        /// @brief check if Container satisfies ContiguousContainer and has a value_type of char
        /// @tparam Container the container type to check
        template <typename Container>
        constexpr bool is_contiguous_char_container = std::is_same<typename std::remove_reference_t<decltype(*std::begin(std::declval<Container &>()))>, char>::value && std::is_base_of<std::random_access_iterator_tag, typename std::iterator_traits<decltype(std::begin(std::declval<Container &>()))>::iterator_category>::value;

        enum : uchar
        {
            /// @brief NOT op flag, invert the next matching unit
            NOT = 0x81,
            /// @brief QUANTIFY op code, next matching unit is quantified
            QUANTIFY,
            /// @brief ZERO_OR_MORE op code, next matching unit is quantified as zero or more
            ZERO_OR_MORE,
            /// @brief ONE_OR_MORE op code, next matching unit is quantified as one or more
            ONE_OR_MORE,
            /// @brief ZERO_OR_ONE op code, next matching unit is quantified as zero or one
            ZERO_OR_ONE,
            /// @brief ANY op code, next matching unit is a group of characters
            ANY,
            /// @brief RANGE op code, next matching unit is a range of characters
            RANGE,
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
                if (res > uint16_t(SIMPLEX_QUANTIFY_MAX))
                    throw std::logic_error("simplex::parse(): malformed quantifier, maximum bound is 254");
                return uchar(res);
            }
            default:
                throw std::logic_error("simplex::parse(): malformed quantifier");
            }
        }

        bool any(std::string_view expr, const uchar cur)
        { // assume expr is the actual group
            size_t pos{0};
            for (uchar scur = expr[pos]; scur == RANGE; scur = expr[++pos])
            { // ranges are always two characters, otherwise it's malformed
                if (cur >= expr[++pos] && cur <= expr[++pos])
                    return pos;
            }
            return expr.find(char(cur), pos) != std::string_view::npos;
        }

        template <typename Iter>
        bool quantify(std::string_view expr, Iter &begin, const Iter &end, size_t &pos, uchar cur, const uint16_t min, const uint16_t max)
        { // assume we have already read QUANTIFY operator
            size_t new_pos = ++pos;
            uint16_t cnt{0};
            uchar flags{0}, any_len{0}, scur = expr[pos];
            bool res{false};
            while (cnt <= max && begin != end)
            {
                switch (scur)
                {
                // case FLAGS: fallthrough...
                case NOT:
                    flags |= scur & uchar(0x7F), scur = expr[++pos], new_pos = pos;
                    continue;
                case QUANTIFY:
                case ZERO_OR_MORE:
                case ONE_OR_MORE:
                case ZERO_OR_ONE:
                    throw std::logic_error("simplex::matches(): malformed quantifier, nested quantifiers are not allowed");
                case ANY:
                    any_len = expr[pos + 1];
                    res = any(expr.substr(pos + 2, any_len), cur);
                    new_pos = pos + any_len + 1;
                    break;
                default:
                    res = cur == scur;
                    break;
                }
                if (!(test_flag(flags, NOT) ^ res))
                    return (pos = new_pos, cnt >= min);
                ++cnt, cur = *++begin;
            }
            return (pos = new_pos, cnt >= min && cnt <= max);
        }
    } // namespace internal

    /// @brief Parses a simplex expression and converts it to a string of internal codes.
    /// @tparam Iter Iterator type of the container.
    /// @param expr The simplex expression to parse.
    /// @param begin Iterator pointing to the beginning of the container.
    /// @param end Iterator pointing to the end of the container.
    /// @return constexpr std::string_view The string of internal codes.
    /// @throws std::logic_error If the expression is too large for the container, or if there is a syntax error in the expression.
    /// @details This function converts a simplex expression to a string of internal codes that can be used by the Simplex engine.
    /// The function iterates through the expression and converts each character to its corresponding internal code.
    /// The resulting string of internal codes is returned as a std::string_view.
    /// If the expression is too large for the container, or if there is a syntax error in the expression, a std::logic_error is thrown.
    template <typename Iter>
    constexpr std::string_view parse(std::string_view expr, Iter begin, const Iter end)
    {
        static_assert(std::is_same<char, typename std::iterator_traits<Iter>::value_type>::value, "simplex::parse(): iterator::value_type must be char");
        if (auto dist = std::distance(begin, end); 0 > dist || expr.size() > size_t(dist))
            throw std::logic_error("simplex::parse(): expression too large for container");
        using namespace internal;
        size_t xi{0};
        uchar c{0};
        Iter p = begin;
        for (; xi < expr.size(); ++xi, ++p)
        {
            c = expr[xi];
            switch (c)
            {
            case '\\':
                *p = expr[++xi];
                break;
            case '!':
                switch (c)
                {
                case '!':
                    *p |= NOT;
                    break;
                }
                for (c = expr[xi + 1];; c = expr[++xi + 1])
                {
                    switch (c)
                    {
                    case '!':
                        *p |= NOT;
                        continue;
                    }
                    break;
                }
                break;
            case '*':
                *p = ZERO_OR_MORE;
                break;
            case '+':
                *p = ONE_OR_MORE;
                break;
            case '?':
                *p = ZERO_OR_ONE;
                break;
            case '{':
            {
                *p = QUANTIFY;
                uchar digits[3]{};
                size_t i = 0;
                for (c = expr[++xi]; i < 3 && c >= '0' && c <= '9'; ++i, c = expr[++xi])
                    digits[i] = c;
                if (c != ',')
                    throw std::logic_error("simplex::parse(): malformed quantifier, exptected ','");
                *++p = (i == 0 ? uchar(0) : stouc(digits, i));
                i = 0;
                for (c = expr[++xi]; i < 3 && c >= '0' && c <= '9'; ++i, c = expr[++xi])
                    digits[i] = c;
                if (c != '}')
                    throw std::logic_error("simplex::parse(): malformed quantifier, exptected '}'");
                *++p = (i == 0 ? uchar(SIMPLEX_QUANTIFY_INF) : stouc(digits, i));
                continue;
            }
            case '[':
            {
                *p = ANY;
                Iter lp = ++p;
                c = expr[++xi];
                for (; c == '-'; c = expr[++xi])
                {
                    *++p = RANGE;
                    if (expr[++xi] == ']')
                        throw std::logic_error("simplex::parse(): malformed range, ']' must be escaped within a range");
                    *++p = expr[xi] == '\\' ? expr[++xi] : expr[xi];
                    if (expr[++xi] == ']')
                        throw std::logic_error("simplex::parse(): malformed range, ']' must be escaped within a range");
                    *++p = expr[xi] == '\\' ? expr[++xi] : expr[xi];
                }
                for (; c != ']'; c = expr[++xi])
                {
                    *++p = expr[xi] == '\\' ? expr[++xi] : expr[xi];
                }
                auto dist = std::distance(lp, p);
                if (255 < dist || 0 > dist)
                    throw std::logic_error("simplex::parse(): malformed any range, range must be 0 to 255 characters");
                *lp = uchar(dist); // store length of any group, max 255
                continue;
            }
            default:
                *p = c;
                continue;
            }
        }
        auto len = std::distance(begin, p);
        bool unterminated_quantify = len > 3 && uchar(*(p - 3)) == QUANTIFY;
        bool unterminated_flag = len > 1 && uchar(*(p - 1)) == NOT;
        if (unterminated_quantify || unterminated_flag)
            throw std::logic_error("simplex::parse(): unterminated simplex operator");
        return std::string_view(&(*begin), size_t(len));
    }

    /// @brief Matches a parsed simplex expression with a range of iterators.
    /// @tparam Iter The type of the iterator.
    /// @param expr The parsed simplex expression to match.
    /// @param begin The beginning of the range of iterators.
    /// @param end The end of the range of iterators.
    /// @return true If the expression matches the range of iterators.
    /// @return false If the expression does not match the range of iterators.
    template <typename Iter>
    bool matches(std::string_view expr, Iter begin, const Iter end)
    {
        using namespace internal;
        static_assert(std::is_base_of<std::forward_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>::value && std::is_same<char, typename std::iterator_traits<Iter>::value_type>::value, "simplex::match() iterator must be a forward iterator over chars");
        size_t pos{0};
        uint16_t min, max;
        uchar cur, scur, any_len, flags{0};
        bool res;
        for (; pos < expr.size() && begin != end; ++pos)
        {
            cur = *begin, scur = expr[pos];
            switch (scur)
            {
            case NOT:
                // flags only set the next matching unit
                flags |= scur & uchar(0x7F);
                continue;
            case QUANTIFY:
                min = expr[++pos], max = expr[++pos];
                res = quantify<Iter>(expr, begin, end, pos, cur, min, max == SIMPLEX_QUANTIFY_INF ? SIMPLEX_INF : max);
                break;
            case ZERO_OR_MORE:
                res = quantify<Iter>(expr, begin, end, pos, cur, 0, SIMPLEX_INF);
                break;
            case ONE_OR_MORE:
                res = quantify<Iter>(expr, begin, end, pos, cur, 1, SIMPLEX_INF);
                break;
            case ZERO_OR_ONE:
                res = quantify<Iter>(expr, begin, end, pos, cur, 0, 1);
                break;
            case ANY:
                any_len = expr[pos + 1];
                res = any(expr.substr(pos + 2, any_len), cur);
                pos += any_len + 1, ++begin;
                break;
            default:
                res = cur == scur, ++begin;
                break;
            }
            if (!(test_flag(flags, NOT) ^ res))
                return false;
            flags = 0;
        }
        // we reached the end of the expression, we do not check for any remaining input
        return pos == expr.size();
    }

    /// @brief Matches a parsed simplex expression with a string_view.
    /// @param expr The parsed simplex expression to match.
    /// @param input The string_view to match against.
    /// @return true If the expression matches the input.
    /// @return false If the expression does not match the input.
    bool matches(std::string_view expr, std::string_view input)
    {
        return matches<std::string_view::iterator>(expr, input.begin(), input.end());
    }
}; // namespace simplex

/// @brief A contexpr-parsed simplex expression that can be used to match against an input with `Simplex::matches()`
/// @tparam Container a contiguous container with value_type char
///
/// @example Construct a Simplex expression from a string literal
/// @code
/// constexpr Simplex s("a*[bc]d");
/// static_assert(s.matches("abbbcd"));
/// @endcode
template <typename Container>
class Simplex
{
    static_assert(simplex::internal::is_contiguous_char_container<Container>, "Simplex Container must satisfy ContiguousContainer with value_type char");

    Container buf;
    size_t len{0};

public:
    typedef char value_type;
    typedef Container container_type;

    constexpr Simplex() = delete;
    constexpr Simplex<Container> &operator=(const Simplex<Container> &) noexcept = default;

    /// @brief Construct a Simplex expression from a string literal
    /// @tparam N the size of the string literal
    /// @param expr the string literal to parse
    template <size_t N>
    constexpr Simplex(const char (&expr)[N]) : buf(), len(simplex::parse(std::string_view(expr, N - 1), std::begin(buf), std::end(buf)).size())
    {
        static_assert(std::is_same<Container, char[N - 1]>::value, "Simplex container must be char[N - 1] when constructing via Simplex(const char (&)[N])");
    }

    /// @brief Construct a Simplex expression from a string_view
    /// @tparam ...Args the types of the arguments to pass to the container constructor
    /// @param expr the string_view to parse
    /// @param ...args the arguments to pass to the container constructor
    template <typename... Args>
    constexpr Simplex(std::string_view expr, Args... args) : buf(args...)
    {
        if (expr.size() > std::size(buf))
            throw std::logic_error("simplex expression too large for Simplex container");
        len = simplex::parse(expr, std::begin(buf), std::end(buf)).size();
    }

    /// @brief Get a const reference to the inner buffer used to store the parsed expression
    inline constexpr const Container &data() const { return buf; }

    /// @brief Get the parsed expression
    inline constexpr std::string_view expr() const { return std::string_view(&(*std::begin(buf)), len); }

    /// @brief Match against a range of iterators.
    /// @tparam Iter The type of the iterator.
    /// @param begin The beginning of the range of iterators.
    /// @param end The end of the range of iterators.
    /// @return true If the range of iterators matches.
    /// @return false If the range of iterators does not match.
    template <typename Iter>
    inline bool matches(Iter begin, const Iter end) const
    {
        return simplex::matches<Iter>(this->expr(), begin, end);
    }

    /// @brief Match against a string_view.
    /// @param input The string_view to match against.
    /// @return true If the input matches.
    /// @return false If the input does not match.
    inline bool matches(std::string_view input) const
    {
        return simplex::matches(this->expr(), input);
    }
};

template <size_t N>
Simplex(const char (&expr)[N]) -> Simplex<char[N - 1]>;

#endif // SIMPLEX_HPP
