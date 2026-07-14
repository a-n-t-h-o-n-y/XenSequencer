#pragma once

#include <cstdint>
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

} // namespace xen
