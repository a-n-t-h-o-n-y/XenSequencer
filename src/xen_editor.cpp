#include <xen/xen_editor.hpp>

#include <filesystem>

#include <xen/active_sessions.hpp>
#include <xen/state.hpp>
#include <xen/xen_processor.hpp>

namespace xen
{

XenEditor::XenEditor(XenProcessor &p)
    : AudioProcessorEditor{p}, timeline_{p.timeline},
      plugin_window_{p.timeline, p.command_history}
{
    this->setResizable(true, true);
    this->setSize(1000, 400);
    this->setResizeLimits(400, 300, 1200, 900);

    this->addAndMakeVisible(&plugin_window_);

    {
        auto slot = sl::Slot<void(State const &, AuxState const &)>{
            [this](State const &state, AuxState const &aux) {
                this->update(state, aux);
            }};
        slot.track(lifetime_);

        // p.timeline because its mutable
        p.timeline.on_state_change.connect(slot);
        p.timeline.on_aux_change.connect(slot);
    }

    // ActiveSessions Signals/Slots
    {
        auto slot = sl::Slot<void(juce::Uuid const &)>{[this](juce::Uuid const &uuid) {
            plugin_window_.active_sessions.remove_instance(uuid);
        }};
        slot.track(lifetime_);
        p.active_sessions.on_instance_shutdown.connect(slot);
    }
    {
        auto slot = sl::Slot<void(juce::Uuid const &, std::string const &)>{
            [this](juce::Uuid const &uuid, std::string const &display_name) {
                plugin_window_.active_sessions.add_or_update_instance(uuid,
                                                                      display_name);
            }};
        slot.track(lifetime_);
        p.active_sessions.on_id_update.connect(slot);
    }

    plugin_window_.active_sessions.on_instance_selected.connect(
        [&p](juce::Uuid const &uuid) { p.active_sessions.request_state(uuid); });

    plugin_window_.active_sessions.on_this_instance_name_change.connect(
        [&p](std::string const &name) {
            p.metadata.display_name = name;
            p.active_sessions.notify_display_name_update(name);
        });

    // TODO any other gui active session signals to connect?

    p.active_sessions.request_other_session_ids();

    // Initialize GUI
    auto const [state, aux] = timeline_.get_state();
    this->update(state, aux);
}

auto XenEditor::update(State const &state, AuxState const &aux) -> void
{
    plugin_window_.update(state, aux);

    // TODO set base frequency?
}

auto XenEditor::resized() -> void
{
    plugin_window_.setBounds(this->getLocalBounds());
}

auto XenEditor::paint(juce::Graphics &g) -> void
{
    g.fillAll(juce::Colours::black);
}

} // namespace xen