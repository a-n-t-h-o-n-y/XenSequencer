#pragma once

#include <compare>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <utility>

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
[[nodiscard]] auto operator<=>(MessageLevel const lhs,
                               MessageLevel const rhs) -> std::strong_ordering;

[[nodiscard]] auto operator<<(std::ostream &os, MessageLevel level) -> std::ostream &;

/**
 * Return a MessageLevel::Debug message pair.
 */
[[nodiscard]] auto mdebug(std::string msg) -> std::pair<MessageLevel, std::string>;

/**
 * Return a MessageLevel::Info message pair.
 */
[[nodiscard]] auto minfo(std::string msg) -> std::pair<MessageLevel, std::string>;

/**
 * Return a MessageLevel::Warning message pair.
 */
[[nodiscard]] auto mwarning(std::string msg) -> std::pair<MessageLevel, std::string>;

/**
 * Return a MessageLevel::Error message pair.
 */
[[nodiscard]] auto merror(std::string msg) -> std::pair<MessageLevel, std::string>;

/**
 * Return a Color ID that can be used to display message text of a particular level.
 */
[[nodiscard]] auto get_color_id(xen::MessageLevel level) -> int;

} // namespace xen