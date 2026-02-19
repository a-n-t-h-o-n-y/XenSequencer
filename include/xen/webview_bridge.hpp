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
    struct SequencePhase
    {
        std::size_t sequence_index{0};
        double phase{0.0};
    };

  public:
    explicit WebviewBridge(XenProcessor &processor);

    [[nodiscard]] auto handle_request_json(std::string const &request_json)
        -> std::string;

    [[nodiscard]] auto make_state_changed_event_json() const -> std::string;
    [[nodiscard]] auto make_trigger_note_event_json(std::size_t sequence_index,
                                                    bool active) const
        -> std::string;
    [[nodiscard]] auto make_phase_sync_event_json(
        std::vector<SequencePhase> const &phases, float bpm) const -> std::string;

  private:
    XenProcessor &processor_;
};

} // namespace xen
