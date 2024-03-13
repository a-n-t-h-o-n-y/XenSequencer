#pragma once

#include <compare>
#include <cstdint>
#include <ostream>
#include <type_traits>

namespace xen
{

/**
 * The type/level of message returned by Command objects.
 */
enum class MessageLevel : std::uint8_t
{
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
};

/**
 * Compare two MessageLevel instances using the spaceship operator.
 *
 * @param lhs The left-hand side MessageLevel.
 * @param rhs The right-hand side MessageLevel.
 * @return A std::strong_ordering value indicating the comparison result.
 */
[[nodiscard]] inline auto operator<=>(MessageLevel const lhs, MessageLevel const rhs)
    -> std::strong_ordering
{
    using T = std::underlying_type_t<MessageLevel>;
    return static_cast<T>(lhs) <=> static_cast<T>(rhs);
}

[[nodiscard]] inline auto operator<<(std::ostream &os, MessageLevel level)
    -> std::ostream &
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

/**
 * Return a MessageLevel::Debug message pair.
 */
[[nodiscard]] inline auto mdebug(std::string msg)
    -> std::pair<MessageLevel, std::string>
{
    return {MessageLevel::Debug, std::move(msg)};
}

/**
 * Return a MessageLevel::Info message pair.
 */
[[nodiscard]] inline auto minfo(std::string msg) -> std::pair<MessageLevel, std::string>
{
    return {MessageLevel::Info, std::move(msg)};
}

/**
 * Return a MessageLevel::Warning message pair.
 */
[[nodiscard]] inline auto mwarning(std::string msg)
    -> std::pair<MessageLevel, std::string>
{
    return {MessageLevel::Warning, std::move(msg)};
}

/**
 * Return a MessageLevel::Error message pair.
 */
[[nodiscard]] inline auto merror(std::string msg)
    -> std::pair<MessageLevel, std::string>
{
    return {MessageLevel::Error, std::move(msg)};
}

} // namespace xen