#include <xen/engine.hpp>

#include <exception>
#include <string>
#include <utility>

#include <xen/command.hpp>
#include <xen/string_manip.hpp>
#include <xen/utility.hpp>
#include <xen/xen_command_tree.hpp>

namespace xen
{

Engine::Engine()
    : state_{.timeline = XenTimeline{{.sequencer = {}, .aux = {}}}},
      command_tree_{create_engine_command_tree()}
{
    pending_sequencer_update_ = state_.timeline.get_state().sequencer;
}

auto Engine::state() -> PluginState &
{
    return state_;
}

auto Engine::state() const -> PluginState const &
{
    return state_;
}

auto Engine::command_tree() const -> XenCommandTree const &
{
    return command_tree_;
}

auto Engine::execute_command(std::string const &command_string)
    -> std::pair<MessageLevel, std::string>
{
    try
    {
        auto command = minimize_spaces(command_string);
        if (command.empty())
        {
            return {MessageLevel::Debug, ""};
        }

        try
        {
            auto const status = command_tree_.execute(state_, split_input(command));
            if (state_.timeline.get_commit_flag())
            {
                state_.timeline.commit();
            }

            if (auto const id = state_.timeline.get_current_commit_id();
                id != previous_commit_id_)
            {
                previous_commit_id_ = id;
                pending_sequencer_update_ = state_.timeline.get_state().sequencer;
            }
            return status;
        }
        catch (...)
        {
            // Revert partial staged changes while preserving UI continuity fields.
            auto aux = state_.timeline.get_state().aux;
            state_.timeline.reset_stage();
            auto state = state_.timeline.get_state();
            state.aux = std::move(aux);
            state_.timeline.stage(std::move(state));
            throw;
        }
    }
    catch (utility::ErrorNoMatch const &)
    {
        return {MessageLevel::Error, "Command not found: " + command_string};
    }
    catch (std::exception const &e)
    {
        return {MessageLevel::Error, e.what()};
    }
    catch (...)
    {
        return {MessageLevel::Error, "Unknown error"};
    }
}

auto Engine::load_serialized_state(SequencerState state) -> void
{
    state_.timeline.stage({std::move(state), {}});
    state_.timeline.commit();
    pending_sequencer_update_ = state_.timeline.get_state().sequencer;
}

auto Engine::take_pending_sequencer_update() -> std::optional<SequencerState>
{
    auto out = std::move(pending_sequencer_update_);
    pending_sequencer_update_.reset();
    return out;
}

} // namespace xen
