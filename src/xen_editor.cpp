#include "xen_editor.hpp"

#include "key_core.hpp"
#include "state.hpp"
#include "xen_processor.hpp"

namespace xen
{

XenEditor::XenEditor(XenProcessor &p)
    : AudioProcessorEditor{p}, processor_{p}, plugin_window_{p.command_core}
{
    this->setResizable(true, true);
    this->setSize(1000, 300);
    this->setResizeLimits(400, 300, 1200, 900);

    this->addAndMakeVisible(&plugin_window_);

    p.timeline.on_state_change.connect(
        [this](State const &state, AuxState const &aux) { this->update(state, aux); });

    p.timeline.on_aux_change.connect([this](State const &state, AuxState const &aux) {
        // TODO you could have code that only modifies the selected cell instead of
        // repainting the entire sequence
        this->update(state, aux);
    });

    // Initialize GUI
    auto const [state, aux] = p.timeline.get_state();
    plugin_window_.update(state, aux);

    // TODO Figure out the default location of this file and create this file.
    this->update_key_listeners("key_config.yml");
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

auto XenEditor::update_key_listeners(std::string const &filename) -> void
{
    key_config_listeners_ = build_key_listeners("key_config.yml", processor_.timeline);
    plugin_window_.set_key_listeners(key_config_listeners_);
}

} // namespace xen