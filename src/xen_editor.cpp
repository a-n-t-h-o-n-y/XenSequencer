#include <xen/xen_editor.hpp>

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

#include <sequence/sequence.hpp>

#include <xen/active_sessions.hpp>
#include <xen/command.hpp>
#include <xen/key_core.hpp>
#include <xen/state.hpp>
#include <xen/user_directory.hpp>
#include <xen/xen_command_tree.hpp>
#include <xen/xen_processor.hpp>

namespace
{

// Shared amongst plugin instances if not sandboxed.
auto on_load_keys_request = sl::Signal<void()>{};
auto on_load_keys_request_mtx = std::mutex{};

// Shared amongst plugin instances if not sandboxed.
auto copy_buffer = std::optional<sequence::Cell>{std::nullopt};
auto copy_buffer_mtx = std::mutex{};

/**
 * Set the key listeners for the plugin window.
 *
 * @details This removes previous_listeners and adds new_listeners. update_key_listeners
 * should be used in most cases.
 */
auto set_key_listeners(std::map<std::string, xen::KeyConfigListener> previous_listeners,
                       std::map<std::string, xen::KeyConfigListener> &new_listeners,
                       xen::gui::PluginWindow &plugin_window,
                       xen::XenCommandTree const &command_tree,
                       xen::XenTimeline &timeline) -> void
{
    using namespace xen;
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
        new_listeners.at(id).on_command.connect([&](std::string const &command) {
            auto const [mlevel, msg] =
                execute(command_tree, timeline, normalize_command_string(command));
            plugin_window.status_bar.message_display.set_status(mlevel, msg);
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

} // namespace

namespace xen::gui
{

XenEditor::XenEditor(XenProcessor &p)
    : AudioProcessorEditor{p}, timeline_{p.timeline},
      command_tree_{create_command_tree(on_focus_change_request_, on_load_keys_request,
                                        on_load_keys_request_mtx, copy_buffer,
                                        copy_buffer_mtx, p.get_process_uuid())},
      plugin_window_{p.timeline, p.command_history, command_tree_}
{
    this->setResizable(true, true);
    this->setSize(1000, 300);
    this->setResizeLimits(400, 300, 1200, 900);

    this->addAndMakeVisible(&plugin_window_);

    {
        auto slot = sl::Slot<void(State const &, AuxState const &)>{
            [this, &p](State const &state, AuxState const &aux) {
                this->update(state, aux, p.metadata);
            }};
        slot.track(lifetime_);

        timeline_.on_state_change.connect(slot);
        timeline_.on_aux_change.connect(slot);
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

    // No lifetime tracking needed signal is owned by *this.
    on_focus_change_request_.connect(
        [this](std::string const &name) { plugin_window_.set_focus(name); });

    {
        auto slot = sl::Slot<void()>{[this] {
            this->update_key_listeners(get_default_keys_file(), get_user_keys_file());
        }};
        slot.track(lifetime_);
        auto const lock = std::lock_guard{on_load_keys_request_mtx};
        on_load_keys_request.connect(slot);
    }

    // No lifetime tracking needed because its GUI->Processor
    plugin_window_.active_sessions.on_instance_selected.connect(
        [&p](juce::Uuid const &uuid) { p.active_sessions.request_state(uuid); });

    // No lifetime tracking needed because its GUI->Processor
    plugin_window_.active_sessions.on_this_instance_name_change.connect(
        [&p](std::string const &name) {
            p.metadata.display_name = name;
            p.active_sessions.notify_display_name_update(name);
        });

    p.active_sessions.request_other_session_ids();

    // Initialize GUI
    auto const [state, aux] = timeline_.get_state();
    this->update(state, aux, p.metadata);

    try
    {
        this->update_key_listeners(get_default_keys_file(), get_user_keys_file());
    }
    catch (std::exception const &e)
    {
        plugin_window_.status_bar.message_display.set_error(
            std::string{"Check `user_keys.yml`: "} + e.what());
    }
}

auto XenEditor::update(State const &state, AuxState const &aux,
                       Metadata const &metadata) -> void
{
    plugin_window_.update(state, aux, metadata);

    // TODO set base frequency?
}

auto XenEditor::update_key_listeners(std::filesystem::path const &default_keys,
                                     std::filesystem::path const &user_keys) -> void
{
    auto previous_listeners = std::move(key_config_listeners_);
    key_config_listeners_ = build_key_listeners(default_keys, user_keys, timeline_);
    set_key_listeners(std::move(previous_listeners), key_config_listeners_,
                      plugin_window_, command_tree_, timeline_);
}

auto XenEditor::resized() -> void
{
    plugin_window_.setBounds(this->getLocalBounds());
}

auto XenEditor::paint(juce::Graphics &g) -> void
{
    g.fillAll(juce::Colours::black);
}

} // namespace xen::gui