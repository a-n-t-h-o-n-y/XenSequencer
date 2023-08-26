#include "input_mode.hpp"

#include <stdexcept>
#include <string>

#include "parse_args.hpp"

namespace xen
{

auto parse_input_mode(std::string const &str) -> InputMode
{
    if (str == "movement")
    {
        return InputMode::Movement;
    }
    else if (str == "note")
    {
        return InputMode::Note;
    }
    else if (str == "velocity")
    {
        return InputMode::Velocity;
    }
    else if (str == "delay")
    {
        return InputMode::Delay;
    }
    else if (str == "gate")
    {
        return InputMode::Gate;
    }
    else
    {
        throw std::invalid_argument{"Invalid input mode: " + str};
    }
}

auto to_string(InputMode mode) -> std::string
{
    switch (mode)
    {
    case InputMode::Movement:
        return "movement";
    case InputMode::Note:
        return "note";
    case InputMode::Velocity:
        return "velocity";
    case InputMode::Delay:
        return "delay";
    case InputMode::Gate:
        return "gate";
    default:
        throw std::invalid_argument{"Invalid input mode: " +
                                    std::to_string(static_cast<int>(mode))};
    }
}

} // namespace xen