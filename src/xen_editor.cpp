#include <xen/xen_editor.hpp>

#include <filesystem>

#include <xen/state.hpp>
#include <xen/xen_processor.hpp>

namespace xen
{

XenEditor::XenEditor(XenProcessor &p)
    : AudioProcessorEditor{p}, timeline_{p.timeline},
      plugin_window_{p.timeline, p.command_history}
{
    this->setResizable(true, true);
    this->setSize(1000, 300);
    this->setResizeLimits(400, 300, 1200, 900);

    this->addAndMakeVisible(&plugin_window_);

    {
        auto slot_update = sl::Slot<void(State const &, AuxState const &)>{
            [this](State const &state, AuxState const &aux) {
                this->update(state, aux);
            }};
        slot_update.track(lifetime_);

        // p.timeline because its mutable
        p.timeline.on_state_change.connect(slot_update);
        p.timeline.on_aux_change.connect(slot_update);
    }

    // Initialize GUI
    auto const [state, aux] = timeline_.get_state();
    this->update(state, aux);
}

auto XenEditor::update(State const &state, AuxState const &aux) -> void
{
    plugin_window_.update(state, aux);

    // TODO set base frequency?
}

void XenEditor::resized()
{
    plugin_window_.setBounds(this->getLocalBounds());
}

} // namespace xen