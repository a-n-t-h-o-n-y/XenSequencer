#include <xen/utility.hpp>

#include <cstddef>
#include <stdexcept>

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
    return static_cast<std::size_t>(remainder < 0 ? remainder + int_length : remainder);
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

} // namespace xen::utility
