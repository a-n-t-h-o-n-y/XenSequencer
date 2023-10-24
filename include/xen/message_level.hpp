#pragma once

namespace xen
{

/**
 * The type of message returned by Command objects.
 */
enum class MessageLevel
{
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
};

// TODO mdebug

[[nodiscard]] inline auto minfo(std::string msg) -> std::pair<MessageLevel, std::string>
{
    return {MessageLevel::Info, std::move(msg)};
}

[[nodiscard]] inline auto mwarning(std::string msg)
    -> std::pair<MessageLevel, std::string>
{
    return {MessageLevel::Warning, std::move(msg)};
}

[[nodiscard]] inline auto merror(std::string msg)
    -> std::pair<MessageLevel, std::string>
{
    return {MessageLevel::Error, std::move(msg)};
}

} // namespace xen