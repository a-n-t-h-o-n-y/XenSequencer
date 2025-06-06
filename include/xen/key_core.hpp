#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <xen/input_mode.hpp>
#include <xen/state.hpp>

namespace juce
{

/**
 * Compares two juce::KeyPress objects.
 *
 * @param lhs The left-hand side juce::KeyPress.
 * @param rhs The right-hand side juce::KeyPress.
 * @return true if lhs is less than rhs, false otherwise.
 */
[[nodiscard]] auto operator<(juce::KeyPress const &lhs,
                             juce::KeyPress const &rhs) -> bool;

} // namespace juce

namespace xen
{

struct KeyConfig
{
    std::optional<InputMode> mode;
    juce::KeyPress keypress;
    std::string command;
};

class KeyCore
{
  public:
    /**
     * Constructs a new KeyCore object.
     *
     * @param configs A vector of KeyConfig objects to initialize the KeyCore.
     */
    explicit KeyCore(std::vector<KeyConfig> const &configs);

  public:
    /**
     * Finds an associated command.
     *
     * @param key The juce::KeyPress to search for.
     * @param mode The current InputMode.
     * @return An optional string, which contains the associated command if the search
     * is successful.
     */
    [[nodiscard]] auto find_action(juce::KeyPress const &key,
                                   InputMode mode) const -> std::optional<std::string>;

  private:
    struct KeyAction
    {
        juce::KeyPress key;
        std::string action;
    };

    std::map<InputMode, std::vector<KeyAction>> mode_sensitive_actions_;
    std::vector<KeyAction> mode_independent_actions_;
};

class KeyConfigListener : public juce::KeyListener
{
  public:
    sl::Signal<void(std::string const &)> on_command;

  public:
    KeyConfigListener(KeyCore key_core, XenTimeline const &tl);

  protected:
    auto keyPressed(juce::KeyPress const &key,
                    juce::Component *origin) -> bool override;

    auto keyStateChanged(bool, juce::Component *) -> bool override;

  private:
    KeyCore key_core_;
    XenTimeline const &tl_;
    std::optional<int> prefix_int_;
};

[[nodiscard]] auto build_key_listeners(
    juce::File const &default_keys, juce::File const &user_keys,
    XenTimeline const &tl) -> std::map<std::string, KeyConfigListener>;

} // namespace xen