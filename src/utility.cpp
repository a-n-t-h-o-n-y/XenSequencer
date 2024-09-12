#include <xen/utility.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
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

auto normalize_pitch(int pitch, std::size_t length) -> std::size_t
{
    return static_cast<std::size_t>(((pitch % (int)length) + (int)length) %
                                    (int)length);
}

auto get_octave(int pitch, std::size_t tuning_length) -> int
{
    if (pitch >= 0)
    {
        return pitch / static_cast<int>(tuning_length);
    }
    else
    {
        return (pitch - static_cast<int>(tuning_length) + 1) /
               static_cast<int>(tuning_length);
    }
}

auto split_version_string(std::string const &version) -> std::array<int, 3>
{
    auto result = std::array<int, 3>{0, 0, 0};
    auto ss = std::stringstream{version};
    auto part = std::string{};
    for (std::size_t i = 0; i < result.size() && std::getline(ss, part, '.'); ++i)
    {
        result[i] = std::stoi(part);
    }
    return result;
}

} // namespace xen
