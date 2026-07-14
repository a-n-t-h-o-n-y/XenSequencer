#pragma once

#include <cstddef>
#include <type_traits>
#include <variant>

namespace xen::utility
{

/**
 * Normalizes a pitch to the range [0, length).
 * @details -1 wraps around to length - 1.
 * @throws std::invalid_argument if length is zero or is not representable as int.
 */
[[nodiscard]] auto normalize_pitch(int pitch, std::size_t length) -> std::size_t;

/**
 * Returns the octave containing pitch using floor division.
 * @throws std::invalid_argument if tuning_length is zero or is not representable as
 * int.
 */
[[nodiscard]] auto get_octave(int pitch, std::size_t tuning_length) -> int;

/**
 * Trait to check if a type is a std::variant.
 */
template <typename T>
struct is_variant : std::false_type
{
};

template <typename... Ts>
struct is_variant<std::variant<Ts...>> : std::true_type
{
};

template <typename T>
constexpr auto is_variant_v = is_variant<T>::value;

} // namespace xen::utility
