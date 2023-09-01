#include "util.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace xen
{

auto to_lower(std::string x) -> std::string
{
    std::transform(std::cbegin(x), std::cend(x), std::begin(x), ::tolower);
    return x;
}

auto read_file_to_string(std::string const &filepath) -> std::string
{
    auto file = std::ifstream{filepath};
    if (!file)
    {
        throw std::runtime_error("Failed to open file for reading: " + filepath);
    }
    auto const content = std::string(std::istreambuf_iterator<char>(file),
                                     std::istreambuf_iterator<char>());
    return content;
}

auto write_string_to_file(std::string const &filepath, std::string const &content)
    -> void
{
    auto file = std::ofstream{filepath};
    if (!file)
    {
        throw std::runtime_error("Failed to open file for writing: " + filepath);
    }
    file << content;
}

} // namespace xen