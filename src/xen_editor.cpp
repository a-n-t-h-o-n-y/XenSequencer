#include <xen/xen_editor.hpp>

#include <exception>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/command.hpp>
#include <xen/gui/themes.hpp>
#include <xen/guide_text.hpp>
#include <xen/key_core.hpp>
#include <xen/scale.hpp>
#include <xen/state.hpp>
#include <xen/string_manip.hpp>
#include <xen/user_directory.hpp>
#include <xen/xen_processor.hpp>

namespace
{

class NoTabFocusTraverser : public juce::KeyboardFocusTraverser
{
  public:
    [[nodiscard]] auto getNextComponent(juce::Component *) -> juce::Component * override
    {
        return nullptr;
    }

    [[nodiscard]] auto getPreviousComponent(juce::Component *)
        -> juce::Component * override
    {
        return nullptr;
    }
};

} // namespace

namespace xen::gui
{

XenEditor::XenEditor(XenProcessor &p, int width, int height)
    : AudioProcessorEditor{p},
      plugin_window{p.plugin_state.current_phrase_directory,
                    p.plugin_state.current_tuning_directory,
                    p.plugin_state.command_history, p.audio_thread_state_for_gui},
      processor_{p}, tooltip_window_{this}
{
    this->setFocusContainerType(juce::Component::FocusContainerType::focusContainer);

    this->setResizable(true, true);
    this->setSize(width, height);
    this->setResizeLimits(400, 300, 0x3fffffff, 0x3fffffff);

    this->addAndMakeVisible(&plugin_window);

    { // Initialize LookAndFeel after plugin_window is added as child.
        if (p.plugin_state.laf == nullptr)
        {
            auto const theme = [&] {
                auto const lock = std::lock_guard{p.plugin_state.shared.theme_mtx};
                return p.plugin_state.shared.theme;
            }();
            p.plugin_state.laf = gui::make_laf(theme);
        }
        this->setLookAndFeel(p.plugin_state.laf.get());
    }

    // CommandBar Execute Request
    plugin_window.bottom_bar.command_bar.on_command.connect(
        [this](std::string const &command_string) {
            this->execute_command_string(command_string);
        });

    // CommandBar Guide Text Request
    plugin_window.bottom_bar.command_bar.on_guide_text_request.connect(
        [this](std::string const &partial_command) -> std::string {
            return generate_guide_text(processor_.command_tree, partial_command);
        });

    // CommandBar ID Completion Request
    plugin_window.bottom_bar.command_bar.on_complete_id_request.connect(
        [this](std::string const &partial_command) -> std::string {
            return complete_id(processor_.command_tree, partial_command);
        });

    // Sequence File Selected
    plugin_window.center_component.library_view.sequences_list.on_file_selected.connect(
        [this](juce::File const &file) {
            auto const filename = file.getFileNameWithoutExtension().toStdString();
            this->execute_command_string("load sequenceBank " + double_quote(filename) +
                                         ";show SequenceView;focus SequenceView");
        });

    // Tuning File Selected
    plugin_window.center_component.library_view.tunings_list.on_file_selected.connect(
        [this](juce::File const &file) {
            auto const filename = file.getFileNameWithoutExtension().toStdString();
            this->execute_command_string("load tuning " + double_quote(filename) +
                                         ";show SequenceView;focus SequenceView");
        });

    // Scale Selected
    plugin_window.center_component.library_view.scales_list.on_scale_selected.connect(
        [this](std::string const &scale_name) {
            this->execute_command_string("set scale " + double_quote(scale_name) +
                                         ";show SequenceView;focus SequenceView");
        });

    // SequenceView Command Requests
    plugin_window.center_component.sequence_view.on_command.connect(
        [this](std::string const &command_string) {
            this->execute_command_string(command_string);
        });

    // Library/Sequencer Flip Request
    plugin_window.bottom_bar.library_sequencer_toggle.on_command.connect(
        [this](std::string const &command_string) {
            this->execute_command_string(command_string);
        });

    // Sequence Change Request
    plugin_window.center_component.sequence_view.sequence_bank.on_index_selected
        .connect([this](std::size_t index) {
            this->execute_command_string("select sequence " + std::to_string(index));
        });

    { // Theme Changed
        auto slot = sl::Slot<void(gui::Theme const &)>{[&](gui::Theme const &theme) {
            p.plugin_state.laf = gui::make_laf(theme);
            this->setLookAndFeel(p.plugin_state.laf.get());
        }};
        slot.track(lifetime_);
        auto const lock = std::lock_guard{p.plugin_state.shared.theme_mtx};
        p.plugin_state.shared.on_theme_update.connect(slot);
    }

    { // Focus Change Request
        auto slot = sl::Slot<void(std::string const &)>{
            [this](std::string const &component_id) {
                plugin_window.set_focus(component_id);
            }};
        slot.track(lifetime_);
        p.plugin_state.on_focus_request.connect(slot);
    }

    { // Show Component Request
        auto slot = sl::Slot<void(std::string const &)>{
            [this](std::string const &component_id) {
                plugin_window.show_component(component_id);
            }};
        slot.track(lifetime_);
        p.plugin_state.on_show_request.connect(slot);
    }

    { // Load Keys File Request
        auto slot = sl::Slot<void()>{[this] {
            this->update_key_listeners(get_system_keys_file(), get_user_keys_file());
        }};
        slot.track(lifetime_);
        auto const lock =
            std::lock_guard{p.plugin_state.shared.on_load_keys_request_mtx};
        p.plugin_state.shared.on_load_keys_request.connect(slot);
    }

    // Sequence Library Directory Change
    plugin_window.center_component.library_view.sequences_list.on_directory_change
        .connect([&](juce::File const &directory) {
            p.plugin_state.current_phrase_directory = directory;
        });

    // Tuning Library Directory Change
    plugin_window.center_component.library_view.tunings_list.on_directory_change
        .connect([&](juce::File const &directory) {
            p.plugin_state.current_tuning_directory = directory;
        });

    // Initialize GUI
    this->update();

    try
    {
        this->update_key_listeners(get_system_keys_file(), get_user_keys_file());
    }
    catch (std::exception const &e)
    {
        auto const message = std::string{"Check `user_keys.yml`: "} + e.what();
        plugin_window.bottom_bar.status_bar.set_status(MessageLevel::Error, message);
        plugin_window.center_component.message_log.add_message(message,
                                                               MessageLevel::Error);
    }

    this->execute_command_string("welcome");
}

auto XenEditor::createKeyboardFocusTraverser()
    -> std::unique_ptr<juce::ComponentTraverser>
{
    return std::make_unique<NoTabFocusTraverser>();
}

void XenEditor::update()
{
    plugin_window.update(processor_.plugin_state);
}

void XenEditor::update_key_listeners(juce::File const &default_keys,
                                     juce::File const &user_keys)
{
    auto previous_listeners = std::move(key_config_listeners_);
    key_config_listeners_ =
        build_key_listeners(default_keys, user_keys, processor_.plugin_state.timeline);
    this->set_key_listeners(std::move(previous_listeners), key_config_listeners_);
}

void XenEditor::resized()
{
    plugin_window.setBounds(this->getLocalBounds());
    processor_.editor_width = this->getWidth();
    processor_.editor_height = this->getHeight();
}

void XenEditor::execute_command_string(std::string const &command_string)
{
    auto const [level, message] = processor_.execute_command_string(command_string);
    if (level != MessageLevel::Error)
    {
        this->update();
    }
    plugin_window.bottom_bar.status_bar.set_status(level, message);
    plugin_window.center_component.message_log.add_message(message, level);
}

void XenEditor::set_key_listeners(
    std::map<std::string, xen::KeyConfigListener> previous_listeners,
    std::map<std::string, xen::KeyConfigListener> &new_listeners)
{
    // This relies on Component::getComponentID();
    auto const remove_listener = [&previous_listeners](juce::Component &component) {
        auto const id = to_lower(component.getComponentID().toStdString());
        auto const iter = previous_listeners.find(id);
        if (iter != std::cend(previous_listeners))
        {
            component.removeKeyListener(&(iter->second));
        }
    };

    auto const add_listener = [&](juce::Component &component) {
        auto const id = to_lower(component.getComponentID().toStdString());
        component.addKeyListener(&new_listeners.at(id));
        new_listeners.at(id).on_command.connect([&](std::string const &command_string) {
            this->execute_command_string(command_string);
        });
    };

    try
    {
        remove_listener(plugin_window.center_component.sequence_view);
        add_listener(plugin_window.center_component.sequence_view);

        remove_listener(plugin_window.center_component.library_view.sequences_list);
        add_listener(plugin_window.center_component.library_view.sequences_list);

        remove_listener(plugin_window.center_component.library_view.tunings_list);
        add_listener(plugin_window.center_component.library_view.tunings_list);

        remove_listener(plugin_window.center_component.library_view.scales_list);
        add_listener(plugin_window.center_component.library_view.scales_list);

        remove_listener(plugin_window.center_component.message_log);
        add_listener(plugin_window.center_component.message_log);
    }
    catch (std::exception const &e)
    {
        throw std::runtime_error("Failed to set key listeners: " +
                                 std::string{e.what()});
    }
}

} // namespace xen::gui