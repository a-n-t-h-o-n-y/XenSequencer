#include "xen_editor.hpp"
#include "xen_processor.hpp"

namespace xen
{

XenEditor::XenEditor(XenProcessor &p) : AudioProcessorEditor{p}, processor_ref_{p}
{
    this->setResizable(true, true);
    this->setSize(1000, 300);
    this->setResizeLimits(400, 300, 1200, 900);

    this->addAndMakeVisible(&plugin_window_);
    // TODO hook up signal from processor.timeline_ state changed to call update(state)
}

auto XenEditor::update(State const &state) -> void
{
    plugin_window_.update(state);

    // TODO set base frequency?
}

void XenEditor::resized()
{
    plugin_window_.setBounds(getLocalBounds());
}

} // namespace xen