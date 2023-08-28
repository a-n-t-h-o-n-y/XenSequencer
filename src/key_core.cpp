#include "key_core.hpp"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>
#include <yaml-cpp/yaml.h>

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

/** @brief Parses the given YAML node to a KeyConfig
 *
 *  @param key_combo_str The key combination string
 *  @param command The associated command
 *  @return A KeyConfig object
 */
[[nodiscard]] auto parse_key_config(std::string const &key_combo_str,
                                    std::string const &command) -> KeyConfig
{
}

/** @brief Parses a YAML file into a collection of KeyCore objects.
 *
 *  @param file_path The path to the YAML file.
 *  @return A map of KeyCore objects, one for each component.
 */
[[nodiscard]] auto parse_key_config_from_file(std::string const &filepath)
    -> std::unordered_map<std::string, xen::KeyCore>
{
    auto root = YAML::LoadFile(file_path);

    auto component_to_keycore = std::unordered_map<std::string, KeyCore>{};

    for (auto const &component : root)
    {
        auto const component_name = component.first.as<std::string>();
        auto const &key_mappings = component.second;

        auto configs = std::vector<KeyConfig>{};

        for (auto const &mapping : key_mappings)
        {
            auto const key_combo_str = mapping.first.as<std::string>();
            auto const command = mapping.second.as<std::string>();

            auto config = parse_key_config(key_combo_str, command);
            configs.push_back(config);
        }

        if (component_to_keycore.find(component_name) != component_to_keycore.end())
        {
            throw std::runtime_error{"Duplicate component name in key config file."};
        }
        component_to_keycore[component_name] = KeyCore{configs};
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
    if (it_mode != mode_sensitive_actions_.end())
    {
        auto it_key = it_mode->second.find(key);
        if (it_key != it_mode->second.end())
        {
            return it_key->second;
        }
    }

    // Check mode-independent actions
    auto it_key = mode_independent_actions_.find(key);
    if (it_key != mode_independent_actions_.end())
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

auto build_key_listeners(std::string const & /*filename*/, XenTimeline const &tl)
    -> std::map<std::string, KeyConfigListener>
{
    // TODO build up each keycore from filename.
    // TODO add command to give focus to command bar.
    auto phrase_key_core = KeyCore{{
        {std::nullopt, juce::KeyPress{'h'}, "moveleft"},
        {std::nullopt, juce::KeyPress{juce::KeyPress::leftKey}, "moveleft"},
        {std::nullopt, juce::KeyPress{'l'}, "moveright"},
        {std::nullopt, juce::KeyPress{juce::KeyPress::rightKey}, "moveright"},

        {InputMode::Movement, juce::KeyPress{'j'}, "movedown"},
        {InputMode::Movement, juce::KeyPress{juce::KeyPress::downKey}, "movedown"},
        {InputMode::Movement, juce::KeyPress{'k'}, "moveup"},
        {InputMode::Movement, juce::KeyPress{juce::KeyPress::upKey}, "moveup"},

        {InputMode::Note, juce::KeyPress{'j'}, "shiftnote -1"},
        {InputMode::Note, juce::KeyPress{'J', juce::ModifierKeys::shiftModifier, 0},
         "shiftoctave -1"},
        {InputMode::Note, juce::KeyPress{juce::KeyPress::downKey}, "shiftnote -1"},
        {InputMode::Note,
         juce::KeyPress{juce::KeyPress::downKey, juce::ModifierKeys::shiftModifier, 0},
         "shiftoctave -1"},
        {InputMode::Note, juce::KeyPress{'k'}, "shiftnote +1"},
        {InputMode::Note, juce::KeyPress{'K', juce::ModifierKeys::shiftModifier, 0},
         "shiftoctave +1"},
        {InputMode::Note, juce::KeyPress{juce::KeyPress::upKey}, "shiftnote +1"},
        {InputMode::Note,
         juce::KeyPress{juce::KeyPress::upKey, juce::ModifierKeys::shiftModifier, 0},
         "shiftoctave +1"},

        // TODO shift and ctrl modifiers
        // TODO look into cmd/ctrl for cross platform, doesn't seem simple
        {InputMode::Velocity, juce::KeyPress{'j'}, "shiftvelocity -0.05"},
        {InputMode::Velocity, juce::KeyPress{juce::KeyPress::downKey},
         "shiftvelocity -0.05"},
        {InputMode::Velocity, juce::KeyPress{'k'}, "shiftvelocity 0.05"},
        {InputMode::Velocity, juce::KeyPress{juce::KeyPress::upKey},
         "shiftvelocity 0.05"},

        {InputMode::Delay, juce::KeyPress{'j'}, "shiftdelay -0.05"},
        {InputMode::Delay, juce::KeyPress{juce::KeyPress::downKey}, "shiftdelay -0.05"},
        {InputMode::Delay, juce::KeyPress{'k'}, "shiftdelay 0.05"},
        {InputMode::Delay, juce::KeyPress{juce::KeyPress::upKey}, "shiftdelay 0.05"},

        {InputMode::Gate, juce::KeyPress{'j'}, "shiftgate -0.05"},
        {InputMode::Gate, juce::KeyPress{juce::KeyPress::downKey}, "shiftgate -0.05"},
        {InputMode::Gate, juce::KeyPress{'k'}, "shiftgate 0.05"},
        {InputMode::Gate, juce::KeyPress{juce::KeyPress::upKey}, "shiftgate 0.05"},

        {std::nullopt, juce::KeyPress{'m'}, "mode movement"},
        {std::nullopt, juce::KeyPress{'n'}, "mode note"},
        {std::nullopt, juce::KeyPress{'v'}, "mode velocity"},
        {std::nullopt, juce::KeyPress{'d'}, "mode delay"},
        {std::nullopt, juce::KeyPress{'g'}, "mode gate"},

        {std::nullopt, juce::KeyPress{':', juce::ModifierKeys::shiftModifier, 0},
         "focus commandbar"},

        {std::nullopt, juce::KeyPress{'c', juce::ModifierKeys::ctrlModifier, 0},
         "copy"},
        {std::nullopt, juce::KeyPress{'x', juce::ModifierKeys::ctrlModifier, 0}, "cut"},
        {std::nullopt, juce::KeyPress{'v', juce::ModifierKeys::ctrlModifier, 0},
         "paste"},
        {std::nullopt, juce::KeyPress{'d', juce::ModifierKeys::ctrlModifier, 0},
         "duplicate"},
    }};
    return {{"phraseeditor", KeyConfigListener{phrase_key_core, tl}}};
    // TODO Tuning Box
    // TODO Command Bar.. but this will only be escape etc..? can this be clean? it
    // already handles its own.
}

} // namespace xen