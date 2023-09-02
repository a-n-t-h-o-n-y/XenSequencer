#pragma once

namespace xen
{

/**
 * The type of message returned by Command objects.
 */
enum class MessageType
{
    Success,
    Warning,
    Error,
};

[[nodiscard]] inline auto msuccess(std::string msg)
    -> std::pair<MessageType, std::string>
{
    return {MessageType::Success, std::move(msg)};
}

[[nodiscard]] inline auto mwarning(std::string msg)
    -> std::pair<MessageType, std::string>
{
    return {MessageType::Warning, std::move(msg)};
}

[[nodiscard]] inline auto merror(std::string msg) -> std::pair<MessageType, std::string>
{
    return {MessageType::Error, std::move(msg)};
}

} // namespace xen