#include <xen/string_manip.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace xen
{

auto to_lower(std::string_view x) -> std::string
{
    auto result = std::string{};
    std::transform(std::cbegin(x), std::cend(x), std::back_inserter(result),
                   [](char c) { return static_cast<char>(::tolower(c)); });
    return result;
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

auto split(std::string const &input, char delimiter) -> std::vector<std::string>
{
    auto result = std::vector<std::string>{};
    auto stream = std::istringstream{input};
    auto word = std::string{};

    while (std::getline(stream, word, delimiter))
    {
        result.push_back(word);
    }

    return result;
}

auto split_quoted_string(std::string const &input) -> std::vector<std::string>
{
    auto result = std::vector<std::string>{};
    auto current_word = std::string{};
    bool in_quotes = false;
    int json_depth = 0;

    for (char ch : input)
    {
        if (ch == '"' && json_depth == 0)
        {
            in_quotes = !in_quotes;
        }
        else if (ch == '{')
        {
            json_depth++;
            current_word += ch;
        }
        else if (ch == '}' && json_depth > 0)
        {
            json_depth--;
            current_word += ch;
        }
        else if (std::isspace(ch) && !in_quotes && json_depth == 0)
        {
            if (!current_word.empty())
            {
                result.push_back(current_word);
                current_word.clear();
            }
        }
        else
        {
            current_word += ch;
        }
    }

    if (!current_word.empty())
    {
        result.push_back(current_word);
    }

    return result;
}

// auto split_quoted_string(std::string const &input) -> std::vector<std::string>
// {
//     auto result = std::vector<std::string>{};
//     auto current_word = std::string{};
//     bool in_quotes = false;

//     for (char ch : input)
//     {
//         if (ch == '"')
//         {
//             in_quotes = !in_quotes;
//         }
//         else if (std::isspace(ch) && !in_quotes)
//         {
//             if (!current_word.empty())
//             {
//                 result.push_back(current_word);
//                 current_word.clear();
//             }
//         }
//         else
//         {
//             current_word += ch;
//         }
//     }

//     if (!current_word.empty())
//     {
//         result.push_back(current_word);
//     }

//     return result;
// }

auto join(std::vector<std::string> const &input, char delimiter) -> std::string
{
    auto result = std::string{};
    auto it = std::begin(input);

    if (it != std::end(input))
    {
        result += *it;
        ++it;
    }

    for (; it != std::end(input); ++it)
    {
        result.push_back(delimiter);
        result += *it;
    }

    return result;
}

auto double_quote(std::string const &input) -> std::string
{
    return '"' + input + '"';
}

auto single_quote(std::string const &input) -> std::string
{
    return '\'' + input + '\'';
}

} // namespace xen
