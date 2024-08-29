#include <xen/utility.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace xen
{

auto read_file_to_string(std::filesystem::path const &filepath) -> std::string
{
    auto file = std::ifstream{filepath};
    if (!file)
    {
        throw std::runtime_error("Failed to Open File for Reading: " +
                                 filepath.string());
    }
    auto const content = std::string(std::istreambuf_iterator<char>(file),
                                     std::istreambuf_iterator<char>());
    return content;
}

void write_string_to_file(std::filesystem::path const &filepath,
                          std::string const &content)
{
    auto file = std::ofstream{filepath};
    if (!file)
    {
        throw std::runtime_error("Failed to open file for writing: " +
                                 filepath.string());
    }
    file << content;
}

auto normalize_interval(int interval, std::size_t length) -> std::size_t
{
    return static_cast<std::size_t>(((interval % (int)length) + (int)length) %
                                    (int)length);
}

auto get_octave(int interval, std::size_t tuning_length) -> int
{
    if (interval >= 0)
    {
        return interval / static_cast<int>(tuning_length);
    }
    else
    {
        return (interval - static_cast<int>(tuning_length) + 1) /
               static_cast<int>(tuning_length);
    }
}

} // namespace xen
