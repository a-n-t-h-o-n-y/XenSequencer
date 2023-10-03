#include <xen/string_manip.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <sstream>
#include <string>

namespace xen
{

auto to_lower(std::string x) -> std::string
{
    std::transform(std::cbegin(x), std::cend(x), std::begin(x), ::tolower);
    return x;
}

auto strip(std::string const &input) -> std::string
{
    auto const begin =
        std::find_if_not(std::cbegin(input), std::cend(input), ::isspace);
    auto const end =
        std::find_if_not(std::crbegin(input), std::crend(input), ::isspace).base();
    return (end <= begin) ? std::string{} : std::string{begin, end};
}

auto minimize_spaces(std::string const &input) -> std::string
{
    auto result = std::string{};
    result.reserve(input.size());

    auto stream = std::istringstream{input};
    auto word = std::string{};
    bool in_quotes = false;

    for (auto const &ch : input)
    {
        if (ch == '"')
        {
            in_quotes = !in_quotes;
        }

        if (in_quotes || ch != ' ')
        {
            result.push_back(ch);
            continue;
        }

        if (!result.empty() && result.back() != ' ')
        {
            result.push_back(' ');
        }
    }

    return strip(result);
}

auto get_first_word(std::string const &input) -> std::string
{
    auto result = std::string{};
    auto stream = std::istringstream{input};
    char ch;
    bool in_quotes = false;

    // Move stream past initial spaces
    while (stream.get(ch))
    {
        if (!std::isspace(ch))
        {
            stream.unget();
            break;
        }
    }

    while (stream.get(ch))
    {
        if (ch == '"')
        {
            in_quotes = !in_quotes;
            if (!in_quotes)
            {
                break;
            }
            continue;
        }

        if (ch == ' ' && !in_quotes)
        {
            break;
        }

        result.push_back(ch);
    }
    return result;
}

auto pop_first_word(std::string const &input) -> std::string
{
    auto i = std::string::size_type{0};
    bool in_quotes = false;

    // Move past initial spaces
    for (; i < input.size(); ++i)
    {
        if (!std::isspace(input[i]))
        {
            break;
        }
    }

    for (; i < input.size(); ++i)
    {
        if (input[i] == '"')
        {
            in_quotes = !in_quotes;
            if (!in_quotes)
            {
                i++; // move past the closing quote
                break;
            }
            continue;
        }

        if (input[i] == ' ' && !in_quotes)
        {
            break;
        }
    }
    return input.substr(std::min(i, input.size()));
}

auto word_count(std::string const &input) -> std::size_t
{
    auto count = std::size_t{0};
    auto in_quotes = false;
    auto is_word = false;

    for (auto const ch : input)
    {
        if (ch == '"')
        {
            in_quotes = !in_quotes;
        }

        if (!std::isspace(ch) || in_quotes)
        {
            if (!is_word)
            {
                is_word = true;
                ++count;
            }
        }
        else
        {
            is_word = false;
        }
    }
    return count;
}

} // namespace xen
