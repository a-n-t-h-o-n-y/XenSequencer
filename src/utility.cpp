#include <xen/utility.hpp>

#include <array>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

#include "numeric.hpp"

namespace xen::utility
{

auto normalize_pitch(int pitch, std::size_t length) -> std::size_t
{
    if (length == 0 || !std::in_range<int>(length))
    {
        throw std::invalid_argument{
            "pitch normalization length must be representable as a positive int"};
    }

    auto const int_length = static_cast<int>(length);
    auto const remainder = pitch % int_length;
    return static_cast<std::size_t>(remainder < 0 ? remainder + int_length
                                                  : remainder);
}

auto get_octave(int pitch, std::size_t tuning_length) -> int
{
    if (tuning_length == 0 || !std::in_range<int>(tuning_length))
    {
        throw std::invalid_argument{
            "tuning length must be representable as a positive int"};
    }

    auto const length = static_cast<int>(tuning_length);
    auto quotient = pitch / length;
    if (pitch % length < 0)
    {
        --quotient;
    }
    return quotient;
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

} // namespace xen::utility
