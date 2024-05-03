#include <xen/xen_editor.hpp>

#include <cassert>
#include <iterator>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

#include <sequence/sequence.hpp>

#include <xen/active_sessions.hpp>
#include <xen/command.hpp>
#include <xen/gui/themes.hpp>
#include <xen/guide_text.hpp>
#include <xen/key_core.hpp>
#include <xen/state.hpp>
#include <xen/string_manip.hpp>
#include <xen/user_directory.hpp>
#include <xen/xen_command_tree.hpp>
#include <xen/xen_processor.hpp>

namespace xen::gui
{

XenEditor::XenEditor(XenProcessor &p)
    : AudioProcessorEditor{p}, plugin_window{p.plugin_state.current_phrase_directory,
                                             p.plugin_state.command_history},
      processor_{p}
{
    this->setResizable(true, true);
    this->setSize(1000, 300);
    this->setResizeLimits(400, 300, 1200, 900);

    this->addAndMakeVisible(&plugin_window);

    { // Initialize LookAndFeel
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
    plugin_window.command_bar.on_command_request.connect(
        [this](std::string const &command_string) {
            this->execute_command_string(command_string);
        });

    // CommandBar Guide Text Request
    plugin_window.command_bar.on_guide_text_request.connect(
        [this](std::string const &partial_command) -> std::string {
            return generate_guide_text(processor_.command_tree, partial_command);
        });

    // CommandBar ID Completion Request
    plugin_window.command_bar.on_complete_id_request.connect(
        [this](std::string const &partial_command) -> std::string {
            return complete_id(processor_.command_tree, partial_command);
        });

    // On Sequence File Selected
    plugin_window.phrases_view.directory_view.on_file_selected.connect(
        [this](juce::File const &file) {
            this->execute_command_string(
                "load state \"" + file.getFileNameWithoutExtension().toStdString() +
                '\"');
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

    { // ActiveSessions Shutdown
        auto slot = sl::Slot<void(juce::Uuid const &)>{[this](juce::Uuid const &uuid) {
            plugin_window.phrases_view.active_sessions_view.remove_instance(uuid);
        }};
        slot.track(lifetime_);
        p.active_sessions.on_instance_shutdown.connect(slot);
    }

    { // ActiveSession ID Update
        auto slot = sl::Slot<void(juce::Uuid const &, std::string const &)>{
            [this](juce::Uuid const &uuid, std::string const &display_name) {
                plugin_window.phrases_view.active_sessions_view.add_or_update_instance(
                    uuid, display_name);
            }};
        slot.track(lifetime_);
        p.active_sessions.on_id_update.connect(slot);
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
            this->update_key_listeners(get_default_keys_file(), get_user_keys_file());
        }};
        slot.track(lifetime_);
        auto const lock =
            std::lock_guard{p.plugin_state.shared.on_load_keys_request_mtx};
        p.plugin_state.shared.on_load_keys_request.connect(slot);
    }

    // Phrase Library Directory Change
    plugin_window.phrases_view.directory_view.on_directory_change.connect(
        [&](juce::File const &directory) {
            p.plugin_state.current_phrase_directory = directory;
        });

    // ActiveSession Selected
    plugin_window.phrases_view.active_sessions_view.on_instance_selected.connect(
        [&p](juce::Uuid const &uuid) { p.active_sessions.request_state(uuid); });

    // ActiveSession Name Change
    plugin_window.phrases_view.active_sessions_view.on_this_instance_name_change
        .connect([&p](std::string const &name) {
            p.plugin_state.display_name = name;
            p.active_sessions.notify_display_name_update(name);
        });

    p.active_sessions.request_other_session_ids();

    // Initialize GUI
    this->update_ui();

    try
    {
        this->update_key_listeners(get_default_keys_file(), get_user_keys_file());
    }
    catch (std::exception const &e)
    {
        plugin_window.status_bar.message_display.set_status(
            MessageLevel::Error, std::string{"Check `user_keys.yml`: "} + e.what());
    }
}

auto XenEditor::update_ui() -> void
{
    auto const &[state, aux] = processor_.plugin_state.timeline.get_state();
    plugin_window.update(state, aux, processor_.plugin_state.display_name);

    // TODO set base frequency?
}

auto XenEditor::update_key_listeners(juce::File const &default_keys,
                                     juce::File const &user_keys) -> void
{
    auto previous_listeners = std::move(key_config_listeners_);
    key_config_listeners_ =
        build_key_listeners(default_keys, user_keys, processor_.plugin_state.timeline);
    this->set_key_listeners(std::move(previous_listeners), key_config_listeners_);
}

auto XenEditor::resized() -> void
{
    plugin_window.setBounds(this->getLocalBounds());
}

auto XenEditor::execute_command_string(std::string const &command_string) -> void
{
    auto &ps = processor_.plugin_state;

    auto const commands = split(command_string, ';');
    auto status = std::pair<MessageLevel, std::string>{MessageLevel::Debug, ""};
    std::cerr << "SIZE: " << commands.size() << "\n";
    std::cerr << "COMMAND: " << command_string << "\n";
    try
    {
        for (auto const &command : commands)
        {
            status =
                execute(processor_.command_tree, ps, normalize_command_string(command));
        }
        if (ps.timeline.get_commit_flag())
        {
            ps.timeline.commit();
        }
        this->update_ui();
    }
    catch (ErrorNoMatch const &)
    {
        ps.timeline.reset_stage();
        status = {MessageLevel::Error, "Command not found: " + command_string};
    }
    catch (std::exception const &e)
    {
        ps.timeline.reset_stage();
        status = {MessageLevel::Error, e.what()};
    }

    plugin_window.status_bar.message_display.set_status(status.first, status.second);
}

auto XenEditor::set_key_listeners(
    std::map<std::string, xen::KeyConfigListener> previous_listeners,
    std::map<std::string, xen::KeyConfigListener> &new_listeners) -> void
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
        remove_listener(plugin_window.phrase_editor);
        add_listener(plugin_window.phrase_editor);
        // TODO
        // remove_listener(plugin_window.tuning_box);
        // add_listener(plugin_window.tuning_box);
    }
    catch (std::exception const &e)
    {
        throw std::runtime_error("Failed to set key listeners: " +
                                 std::string{e.what()});
    }
}

} // namespace xen::gui