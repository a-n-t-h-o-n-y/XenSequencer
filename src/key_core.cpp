#include <xen/key_core.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>
#include <yaml-cpp/yaml.h>

#include <xen/string_manip.hpp>
#include <xen/utility.hpp>

namespace
{

using namespace xen;

/**
 * @brief Merge two YAML files into a single YAML::Node, with the overlay taking
 * precedence.

 * This is not a general solution, this is specific to the XenSequencer key
 * config. This will remove the 'version' top-level key from base.
 *
 * @param base_filepath The base YAML file.
 * @param overlay_filepath The overlay YAML file.
 * @return YAML::Node The merged YAML node.
 * @throws YAML::Exception if there is an error parsing the YAML files.
 * @throws std::runtime_error if the YAML file structure is invalid.
 */
[[nodiscard]] auto merge_yaml_files(std::filesystem::path const &base_filepath,
                                    std::filesystem::path const &overlay_filepath)
    -> YAML::Node
{
    auto base = YAML::LoadFile(base_filepath.string());
    auto overlay = YAML::LoadFile(overlay_filepath.string());
    base.remove("version");

    if (!base.IsMap() || !overlay.IsMap())
    {
        // TODO catch this and display as error message in GUI
        throw std::runtime_error{"Invalid YAML file structure."};
    }

    for (auto overlay_pair : overlay)
    {
        auto component_name = overlay_pair.first.as<std::string>();
        auto bindings_node = overlay_pair.second;
        for (auto key : bindings_node)
        {
            auto key_value = key.first.as<std::string>();
            auto value_value = key.second.as<std::string>();
            base[component_name][key_value] = value_value;
        }
    }

    return base;
}

auto const get_code = [](auto letter) {
    return juce::KeyPress::createFromDescription(letter).getKeyCode();
};

auto const key_map = [] {
    auto x = std::unordered_map<std::string, int>{
        // Letters
        {"a", get_code("a")},
        {"b", get_code("b")},
        {"c", get_code("c")},
        {"d", get_code("d")},
        {"e", get_code("e")},
        {"f", get_code("f")},
        {"g", get_code("g")},
        {"h", get_code("h")},
        {"i", get_code("i")},
        {"j", get_code("j")},
        {"k", get_code("k")},
        {"l", get_code("l")},
        {"m", get_code("m")},
        {"n", get_code("n")},
        {"o", get_code("o")},
        {"p", get_code("p")},
        {"q", get_code("q")},
        {"r", get_code("r")},
        {"s", get_code("s")},
        {"t", get_code("t")},
        {"u", get_code("u")},
        {"v", get_code("v")},
        {"w", get_code("w")},
        {"x", get_code("x")},
        {"y", get_code("y")},
        {"z", get_code("z")},
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
        {"`", get_code("`")},
        {"~", get_code("~")},
        {"-", get_code("-")},
        {"_", get_code("_")},
        {"=", get_code("=")},
        {"+", get_code("+")},
        {"[", get_code("[")},
        {"{", get_code("{")},
        {"]", get_code("]")},
        {"}", get_code("}")},
        {";", get_code(";")},
        {":", get_code(":")},
        {"'", get_code("'")},
        {"\"", get_code("\"")},
        {"\\", get_code("\\")},
        {"|", get_code("|")},
        {",", get_code(",")},
        {"<", get_code("<")},
        {".", get_code(".")},
        {">", get_code(">")},
        {"/", get_code("/")},
        {"?", get_code("?")},
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

#if defined(_WIN32) || defined(_WIN64)
    // Windows
    x["["] = get_code("{");
    x["]"] = get_code("}");
    x["|"] = get_code("\\");
    x[":"] = get_code(";");
    x["\""] = get_code("'");
    x["<"] = get_code(",");
    x[">"] = get_code(".");
    x["?"] = get_code("/");
    x["+"] = get_code("=");
    x["_"] = get_code("-");
    x["~"] = get_code("`");

#elif defined(__APPLE__) || defined(__MACH__)
    // macOS

#elif defined(__linux__)
    // Linux

#endif

    return x;
}();

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
                   std::begin(key_combo_lower),
                   [](char c) { return static_cast<char>(std::tolower(c)); });
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

    return KeyConfig{mode, juce::KeyPress(key_code, modifiers, 0), command};
}

/** @brief Parse a YAML::Node into a collection of KeyCore objects.
 *
 * @param root The root YAML node.
 *  @return A map of KeyCore objects, one for each component.
 */
[[nodiscard]] auto create_component_key_cores(YAML::Node const &root)
    -> std::unordered_map<std::string, KeyCore>
{
    auto component_to_keycore = std::unordered_map<std::string, KeyCore>{};

    for (auto const &component : root)
    {
        auto component_name = component.first.as<std::string>();
        component_name = to_lower(component_name);
        auto const &key_mappings = component.second;

        auto configs = std::vector<KeyConfig>{};

        for (auto const &mapping : key_mappings)
        {
            auto const key_combo_str = mapping.first.as<std::string>();
            auto const command = mapping.second.as<std::string>();

            auto config = parse_key_config(key_combo_str, command);
            configs.push_back(std::move(config));
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
            mode_sensitive_actions_[*config.mode].push_back(
                KeyCore::KeyAction{config.keypress, config.command});
        }
        else
        {
            mode_independent_actions_.push_back(
                KeyCore::KeyAction{config.keypress, config.command});
        }
    }
}

auto KeyCore::find_action(const juce::KeyPress &key, InputMode mode) const
    -> std::optional<std::string>
{
    // Check mode-sensitive actions first
    auto const it_mode = mode_sensitive_actions_.find(mode);
    if (it_mode != std::cend(mode_sensitive_actions_))
    {
        auto &actions = it_mode->second;

        auto const it_key = std::ranges::find_if(
            actions, [key](auto const &x) { return x.key == key; });

        if (it_key != std::cend(actions))
        {
            return it_key->action;
        }
    }

    // Check mode-independent actions
    auto const it_key = std::ranges::find_if(
        mode_independent_actions_, [key](auto const &x) { return x.key == key; });

    if (it_key != std::end(mode_independent_actions_))
    {
        return it_key->action;
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

auto KeyConfigListener::keyStateChanged(bool isKeyDown, juce::Component *) -> bool
{
    // Workaround for Windows bug, keyPress is called twice if this does not return
    // true. This allows spacebar press to go to DAW.
    return isKeyDown && !juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey);
}

auto build_key_listeners(std::filesystem::path const &default_keys,
                         std::filesystem::path const &user_keys, XenTimeline const &tl)
    -> std::map<std::string, KeyConfigListener>
{
    auto const keys_node = merge_yaml_files(default_keys, user_keys);
    auto key_cores = create_component_key_cores(keys_node);
    auto result = std::map<std::string, KeyConfigListener>{};

    for (auto &[component_name, key_core] : key_cores)
    {
        result.try_emplace(component_name, KeyConfigListener{std::move(key_core), tl});
    }

    return result;
}

} // namespace xen
