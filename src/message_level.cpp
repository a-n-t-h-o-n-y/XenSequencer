#include <xen/message_level.hpp>

#include <string>
#include <utility>

namespace xen
{

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

} // namespace xen
