#include "xen_editor.hpp"

#include "key_core.hpp"
#include "state.hpp"
#include "user_directory.hpp"
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

    // TODO wrap with try block and figure out how to display error
    this->update_key_listeners(get_keybinding_file().getFullPathName().toStdString());
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

auto XenEditor::update_key_listeners(std::string const &filepath) -> void
{
    key_config_listeners_ = build_key_listeners(filepath, processor_.timeline);
    plugin_window_.set_key_listeners(key_config_listeners_);
}

} // namespace xen