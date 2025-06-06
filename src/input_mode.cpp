#include <xen/input_mode.hpp>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <xen/parse_args.hpp>
#include <xen/string_manip.hpp>

namespace xen
{

auto operator<<(std::ostream &os, InputMode mode) -> std::ostream &
{
    switch (mode)
    {
    case InputMode::Pitch:
        return os << "pitch";
    case InputMode::Velocity:
        return os << "velocity";
    case InputMode::Delay:
        return os << "delay";
    case InputMode::Gate:
        return os << "gate";
    case InputMode::Scale:
        return os << "scale";
    case InputMode::ScaleMode:
        return os << "scaleMode";
    default:
        throw std::invalid_argument{"Invalid input mode: " +
                                    std::to_string(static_cast<int>(mode))};
    }
}

auto operator>>(std::istream &is, InputMode &mode) -> std::istream &
{
    auto str = std::string{};
    is >> str;
    auto lower_str = to_lower(str);

    if (lower_str == "pitch")
    {
        mode = InputMode::Pitch;
    }
    else if (lower_str == "velocity")
    {
        mode = InputMode::Velocity;
    }
    else if (lower_str == "delay")
    {
        mode = InputMode::Delay;
    }
    else if (lower_str == "gate")
    {
        mode = InputMode::Gate;
    }
    else if (lower_str == "scale")
    {
        mode = InputMode::Scale;
    }
    else if (lower_str == "scalemode")
    {
        mode = InputMode::ScaleMode;
    }
    else
    {
        throw std::invalid_argument{"Invalid input mode: " + str};
    }
    return is;
}

auto parse_input_mode(std::string const &str) -> InputMode
{
    auto is = std::istringstream{str};
    auto mode = InputMode{};
    is >> mode;
    return mode;
}

auto to_string(InputMode mode) -> std::string
{
    auto os = std::ostringstream{};
    os << mode;
    return os.str();
}

} // namespace xen
