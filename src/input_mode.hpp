#pragma once

#include <string>

namespace xen
{

/**
 * @brief The input mode of the Sequence editor.
 */
enum class InputMode
{
    Movement,
    Note,
    Velocity,
    Delay,
    Gate,
};

[[nodiscard]] auto parse_input_mode(std::string const &str) -> InputMode;

[[nodiscard]] auto to_string(InputMode mode) -> std::string;

} // namespace xen