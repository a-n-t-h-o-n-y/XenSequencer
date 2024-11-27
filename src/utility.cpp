#include <xen/utility.hpp>

#include <array>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

namespace xen
{

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
