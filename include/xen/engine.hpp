#pragma once

#include <optional>
#include <string>
#include <utility>

#include <xen/engine_state.hpp>
#include <xen/message_level.hpp>
#include <xen/xen_command_tree.hpp>

namespace xen
{

class Engine
{
  public:
    Engine();

  public:
    [[nodiscard]] auto state() -> PluginState &;

    [[nodiscard]] auto state() const -> PluginState const &;

    [[nodiscard]] auto command_tree() const -> XenCommandTree const &;

    [[nodiscard]] auto execute_command(std::string const &command_string)
        -> std::pair<MessageLevel, std::string>;

    auto load_serialized_state(SequencerState state) -> void;

    [[nodiscard]] auto take_pending_sequencer_update() -> std::optional<SequencerState>;

  private:
    PluginState state_;
    XenCommandTree command_tree_;
    int previous_commit_id_{-1};
    std::optional<SequencerState> pending_sequencer_update_{std::nullopt};
};

} // namespace xen
