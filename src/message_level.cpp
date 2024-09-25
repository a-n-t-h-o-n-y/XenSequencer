#include <xen/message_level.hpp>

#include <compare>
#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include <xen/gui/themes.hpp>

namespace xen
{

auto operator<=>(MessageLevel const lhs, MessageLevel const rhs) -> std::strong_ordering
{
    using T = std::underlying_type_t<MessageLevel>;
    return static_cast<T>(lhs) <=> static_cast<T>(rhs);
}

auto operator<<(std::ostream &os, MessageLevel level) -> std::ostream &
{
    switch (level)
    {
    case MessageLevel::Debug:
        os << "Debug";
        break;
    case MessageLevel::Info:
        os << "Info";
        break;
    case MessageLevel::Warning:
        os << "Warning";
        break;
    case MessageLevel::Error:
        os << "Error";
        break;
    }

    return os;
}

auto mdebug(std::string msg) -> std::pair<MessageLevel, std::string>
{
    return {MessageLevel::Debug, std::move(msg)};
}

auto minfo(std::string msg) -> std::pair<MessageLevel, std::string>
{
    return {MessageLevel::Info, std::move(msg)};
}

auto mwarning(std::string msg) -> std::pair<MessageLevel, std::string>
{
    return {MessageLevel::Warning, std::move(msg)};
}

auto merror(std::string msg) -> std::pair<MessageLevel, std::string>
{
    return {MessageLevel::Error, std::move(msg)};
}

auto get_color_id(xen::MessageLevel level) -> int
{
    switch (level)
    {
    case MessageLevel::Debug:
        return gui::ColorID::ForegroundHigh;
    case MessageLevel::Info:
        return gui::ColorID::ForegroundHigh;
    case MessageLevel::Warning:
        return gui::ColorID::ForegroundMedium;
    case MessageLevel::Error:
        return gui::ColorID::ForegroundMedium;
    default:
        throw std::invalid_argument{
            "Invalid MessageLevel: " +
            std::to_string(static_cast<std::underlying_type_t<MessageLevel>>(level))};
    }
}

} // namespace xen