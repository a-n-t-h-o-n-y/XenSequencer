#include "xen_editor.hpp"

#include "state.hpp"
#include "xen_processor.hpp"

namespace xen
{

XenEditor::XenEditor(XenProcessor &p)
    : AudioProcessorEditor{p}, plugin_window_{p.command_core}
{
    this->setResizable(true, true);
    this->setSize(1000, 300);
    this->setResizeLimits(400, 300, 1200, 900);

    this->addAndMakeVisible(&plugin_window_);

    p.timeline.state_changed.connect(
        [this](State const &state) { this->update(state); });
}

auto XenEditor::update(State const &state) -> void
{
    plugin_window_.update(state);

    // TODO set base frequency?
}

void XenEditor::resized()
{
    plugin_window_.setBounds(this->getLocalBounds());
}

} // namespace xen