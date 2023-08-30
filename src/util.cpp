#include "util.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_map>

namespace xen
{

auto to_lower(std::string x) -> std::string
{
    std::transform(std::cbegin(x), std::cend(x), std::begin(x), ::tolower);
    return x;
}

auto keyboard_toupper(char ch) -> char
{
    static const auto symbol_map = std::unordered_map<char, char>{
        {';', ':'},  {',', '<'},  {'.', '>'}, {'/', '?'}, {'[', '{'}, {']', '}'},
        {'\\', '|'}, {'\'', '"'}, {'`', '~'}, {'1', '!'}, {'2', '@'}, {'3', '#'},
        {'4', '$'},  {'5', '%'},  {'6', '^'}, {'7', '&'}, {'8', '*'}, {'9', '('},
        {'0', ')'},  {'-', '_'},  {'=', '+'},
    };

    auto it = symbol_map.find(ch);
    if (it != std::end(symbol_map))
    {
        return it->second;
    }
    return std::toupper(static_cast<unsigned char>(ch));
}

auto keyboard_tolower(char ch) -> char
{
    static const auto symbol_map = std::unordered_map<char, char>{
        {':', ';'},  {'<', ','},  {'>', '.'}, {'?', '/'}, {'{', '['}, {'}', ']'},
        {'|', '\\'}, {'"', '\''}, {'~', '`'}, {'!', '1'}, {'@', '2'}, {'#', '3'},
        {'$', '4'},  {'%', '5'},  {'^', '6'}, {'&', '7'}, {'*', '8'}, {'(', '9'},
        {')', '0'},  {'_', '-'},  {'+', '='},
    };
    auto it = symbol_map.find(ch);
    if (it != std::end(symbol_map))
    {
        return it->second;
    }
    return std::tolower(static_cast<unsigned char>(ch));
}

} // namespace xen