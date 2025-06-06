#pragma once

#include <iosfwd>
#include <string>

namespace xen
{

/**
 * The input mode of the Sequence editor.
 */
enum class InputMode
{
    Pitch,
    Velocity,
    Delay,
    Gate,
    Scale,
    ScaleMode,
};

auto operator<<(std::ostream &os, InputMode mode) -> std::ostream &;

auto operator>>(std::istream &is, InputMode &mode) -> std::istream &;

[[nodiscard]] auto parse_input_mode(std::string const &str) -> InputMode;

[[nodiscard]] auto to_string(InputMode mode) -> std::string;

} // namespace xen
