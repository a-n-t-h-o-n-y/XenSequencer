#pragma once

#include <memory>
#include <mutex>
#include <string>

#include <signals_light/signal.hpp>

#include <xen/command_history.hpp>
#include <xen/gui/themes.hpp>

namespace juce
{
class LookAndFeel;
}

namespace xen
{

struct SharedRuntimeState
{
    sl::Signal<void()> on_load_keys_request{};
    std::mutex on_load_keys_request_mtx{};

    gui::Theme theme{};
    sl::Signal<void(gui::Theme const &)> on_theme_update{};
    std::mutex theme_mtx{};
};

struct RuntimeState
{
    sl::Signal<void(std::string const &)> on_focus_request{};
    sl::Signal<void(std::string const &)> on_show_request{};
    CommandHistory command_history{};
    inline static SharedRuntimeState shared{};
    std::unique_ptr<juce::LookAndFeel> laf{nullptr};
};

} // namespace xen
