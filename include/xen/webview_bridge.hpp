#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace xen
{
class XenProcessor;
}

namespace xen
{

class WebviewBridge
{
  public:
    struct MeasurePhase
    {
        double phase{0.0};
    };

  public:
    explicit WebviewBridge(XenProcessor &processor);

    [[nodiscard]] auto handle_request_json(std::string const &request_json)
        -> std::string;

    [[nodiscard]] auto make_state_changed_event_json() const -> std::string;
    [[nodiscard]] auto make_phase_sync_event_json(MeasurePhase phase, float bpm) const
        -> std::string;

  private:
    XenProcessor &processor_;
};

} // namespace xen
