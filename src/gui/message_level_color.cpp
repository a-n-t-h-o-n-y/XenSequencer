#include <xen/message_level.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <xen/gui/themes.hpp>

namespace xen
{

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
