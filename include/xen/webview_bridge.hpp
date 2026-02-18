#pragma once

#include <string>

namespace xen
{
class XenProcessor;
}

namespace xen
{

class WebviewBridge
{
  public:
    explicit WebviewBridge(XenProcessor &processor);

    [[nodiscard]] auto handle_request_json(std::string const &request_json)
        -> std::string;

    [[nodiscard]] auto make_state_changed_event_json() const -> std::string;

  private:
    XenProcessor &processor_;
};

} // namespace xen
