#include "key_core.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>
#include <yaml-cpp/yaml.h>

#include "util.hpp"

namespace juce
{

auto operator<(juce::KeyPress const &lhs, juce::KeyPress const &rhs) -> bool
{
    auto const lhs_key = lhs.getKeyCode();
    auto const rhs_key = rhs.getKeyCode();
    if (lhs_key < rhs_key)
    {
        return true;
    }
    if (lhs_key > rhs_key)
    {
        return false;
    }

    auto const lhs_mods = lhs.getModifiers().getRawFlags();
    auto const rhs_mods = rhs.getModifiers().getRawFlags();
    return lhs_mods < rhs_mods;
}

} // namespace juce

namespace
{

using namespace xen;

auto const key_map = std::unordered_map<std::string, int>{
    // Letters
    {"a", 'a'},
    {"b", 'b'},
    {"c", 'c'},
    {"d", 'd'},
    {"e", 'e'},
    {"f", 'f'},
    {"g", 'g'},
    {"h", 'h'},
    {"i", 'i'},
    {"j", 'j'},
    {"k", 'k'},
    {"l", 'l'},
    {"m", 'm'},
    {"n", 'n'},
    {"o", 'o'},
    {"p", 'p'},
    {"q", 'q'},
    {"r", 'r'},
    {"s", 's'},
    {"t", 't'},
    {"u", 'u'},
    {"v", 'v'},
    {"w", 'w'},
    {"x", 'x'},
    {"y", 'y'},
    {"z", 'z'},
    // Function Keys
    {"f1", juce::KeyPress::F1Key},
    {"f2", juce::KeyPress::F2Key},
    {"f3", juce::KeyPress::F3Key},
    {"f4", juce::KeyPress::F4Key},
    {"f5", juce::KeyPress::F5Key},
    {"f6", juce::KeyPress::F6Key},
    {"f7", juce::KeyPress::F7Key},
    {"f8", juce::KeyPress::F8Key},
    {"f9", juce::KeyPress::F9Key},
    {"f10", juce::KeyPress::F10Key},
    {"f11", juce::KeyPress::F11Key},
    {"f12", juce::KeyPress::F12Key},
    // SymbolKeys
    {"`", '`'},
    {"~", '`'},
    {"-", '-'},
    {"_", '-'},
    {"=", '='},
    {"+", '='},
    {"[", '['},
    {"{", '['},
    {"]", ']'},
    {"}", ']'},
    {";", ';'},
    {":", ';'},
    {"'", '\''},
    {"\"", '\''},
    {"\\", '\\'},
    {"|", '\\'},
    {",", ','},
    {"<", ','},
    {".", '.'},
    {">", '.'},
    {"/", '/'},
    {"?", '/'},
    // ControlKeys
    {"escape", juce::KeyPress::escapeKey},
    {"enter", juce::KeyPress::returnKey},
    {"spacebar", juce::KeyPress::spaceKey},
    {"backspace", juce::KeyPress::backspaceKey},
    {"delete", juce::KeyPress::deleteKey},
    // NavigationKeys
    {"arrowup", juce::KeyPress::upKey},
    {"arrowdown", juce::KeyPress::downKey},
    {"arrowleft", juce::KeyPress::leftKey},
    {"arrowright", juce::KeyPress::rightKey},
    {"pageup", juce::KeyPress::pageUpKey},
    {"pagedown", juce::KeyPress::pageDownKey},
    {"home", juce::KeyPress::homeKey},
    {"end", juce::KeyPress::endKey},
    // Numpad
    {"numpad0", juce::KeyPress::numberPad0},
    {"numpad1", juce::KeyPress::numberPad1},
    {"numpad2", juce::KeyPress::numberPad2},
    {"numpad3", juce::KeyPress::numberPad3},
    {"numpad4", juce::KeyPress::numberPad4},
    {"numpad5", juce::KeyPress::numberPad5},
    {"numpad6", juce::KeyPress::numberPad6},
    {"numpad7", juce::KeyPress::numberPad7},
    {"numpad8", juce::KeyPress::numberPad8},
    {"numpad9", juce::KeyPress::numberPad9},
    {"numpad+", juce::KeyPress::numberPadAdd},
    {"numpad-", juce::KeyPress::numberPadSubtract},
    {"numpad*", juce::KeyPress::numberPadMultiply},
    {"numpad/", juce::KeyPress::numberPadDivide},
    {"numpad.", juce::KeyPress::numberPadDelete},
    // SpecialKeys
    {"insert", juce::KeyPress::insertKey},
};

/** @brief Parses the given YAML node to a KeyConfig
 *
 *  @param key_combo_str The key combination string
 *  @param command The associated command
 *  @return A KeyConfig object
 */
[[nodiscard]] auto parse_key_config(std::string const &key_combo_str,
                                    std::string const &command) -> KeyConfig
{
    static const auto mode_map = std::unordered_map<char, InputMode>{
        {'m', InputMode::Movement}, {'n', InputMode::Note}, {'v', InputMode::Velocity},
        {'d', InputMode::Delay},    {'g', InputMode::Gate},
    };

    // Convert to lowercase for case-insensitivity
    auto key_combo_lower = key_combo_str;
    std::transform(std::cbegin(key_combo_lower), std::cend(key_combo_lower),
                   std::begin(key_combo_lower), ::tolower);
    key_combo_lower.erase(std::remove_if(std::begin(key_combo_lower),
                                         std::end(key_combo_lower), ::isspace),
                          std::end(key_combo_lower));

    auto mode = std::optional<InputMode>{};
    auto modifiers = juce::ModifierKeys{};

    // Extract optional mode from key_combo_lower
    if (key_combo_lower.front() == '[')
    {
        auto const closing_bracket = key_combo_lower.find(']');
        if (closing_bracket != std::string::npos)
        {
            auto const mode_char = key_combo_lower[1];
            auto it = mode_map.find(mode_char);
            if (it != std::end(mode_map))
            {
                mode = it->second;
            }
            else
            {
                throw std::invalid_argument("Invalid mode specified: " +
                                            std::string(1, mode_char));
            }
            key_combo_lower.erase(0, closing_bracket + 1);
        }
    }

    // Remaining keys after mode is extracted
    auto ss = std::stringstream{key_combo_lower};
    auto key = std::string{};
    int key_code = 0;

    while (std::getline(ss, key, '+'))
    {
        if (key == "shift")
            modifiers = modifiers.withFlags(juce::ModifierKeys::shiftModifier);
        else if (key == "alt")
            modifiers = modifiers.withFlags(juce::ModifierKeys::altModifier);
        else if (key == "cmd")
            modifiers = modifiers.withFlags(juce::ModifierKeys::commandModifier);
        else if (!key.empty())
        {
            auto const it = key_map.find(key);
            if (it != std::cend(key_map))
            {
                key_code = it->second;
            }
            else
            {
                throw std::invalid_argument("Invalid key specified: " + key);
            }
        }
    }

    // Change case of key_code based on shift modifier
    key_code = modifiers.testFlags(juce::ModifierKeys::shiftModifier)
                   ? keyboard_toupper(key_code)
                   : keyboard_tolower(key_code);

    return KeyConfig{mode, juce::KeyPress(key_code, modifiers, 0), command};
}

/** @brief Parses a YAML file into a collection of KeyCore objects.
 *
 *  @param file_path The path to the YAML file.
 *  @return A map of KeyCore objects, one for each component.
 */
[[nodiscard]] auto parse_key_config_from_file(std::string const &filepath)
    -> std::unordered_map<std::string, KeyCore>
{
    auto root = YAML::LoadFile(filepath);

    auto component_to_keycore = std::unordered_map<std::string, KeyCore>{};

    for (auto const &component : root)
    {
        auto component_name = component.first.as<std::string>();
        std::transform(std::cbegin(component_name), std::cend(component_name),
                       std::begin(component_name), ::tolower);
        auto const &key_mappings = component.second;

        auto configs = std::vector<KeyConfig>{};

        for (auto const &mapping : key_mappings)
        {
            auto const key_combo_str = mapping.first.as<std::string>();
            auto const command = mapping.second.as<std::string>();

            auto config = parse_key_config(key_combo_str, command);
            configs.push_back(config);
        }

        auto const [_, inserted] =
            component_to_keycore.try_emplace(component_name, KeyCore{configs});

        if (!inserted)
        {
            throw std::runtime_error{"Duplicate component name in key config file."};
        }
    }

    return component_to_keycore;
}

} // namespace

namespace xen
{

KeyCore::KeyCore(std::vector<KeyConfig> const &configs)
{
    for (auto const &config : configs)
    {
        if (config.mode)
        {
            mode_sensitive_actions_[*config.mode][config.keypress] = config.command;
        }
        else
        {
            mode_independent_actions_[config.keypress] = config.command;
        }
    }
}

auto KeyCore::find_action(const juce::KeyPress &key, InputMode mode) const
    -> std::optional<std::string>
{
    // Check mode-sensitive actions first
    auto it_mode = mode_sensitive_actions_.find(mode);
    if (it_mode != std::end(mode_sensitive_actions_))
    {
        auto it_key = it_mode->second.find(key);
        if (it_key != std::end(it_mode->second))
        {
            return it_key->second;
        }
    }

    // Check mode-independent actions
    auto it_key = mode_independent_actions_.find(key);
    if (it_key != std::end(mode_independent_actions_))
    {
        return it_key->second;
    }

    return std::nullopt;
}

KeyConfigListener::KeyConfigListener(KeyCore key_core, XenTimeline const &tl)
    : key_core_{std::move(key_core)}, tl_{tl}
{
}

auto KeyConfigListener::keyPressed(juce::KeyPress const &key, juce::Component *) -> bool
{
    auto const action = key_core_.find_action(key, tl_.get_aux_state().input_mode);
    if (action)
    {
        on_command.emit(*action);
        return true;
    }
    return false;
}

auto build_key_listeners(std::string const &filepath, XenTimeline const &tl)
    -> std::map<std::string, KeyConfigListener>
{
    auto key_cores = parse_key_config_from_file(filepath);
    auto result = std::map<std::string, KeyConfigListener>{};

    for (auto &[component_name, key_core] : key_cores)
    {
        result.try_emplace(component_name, KeyConfigListener{std::move(key_core), tl});
    }

    return result;
}

} // namespace xen