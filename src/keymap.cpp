#include <xen/keymap.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

#include <nlohmann/json.hpp>

#include <xen/text_file.hpp>
#include <xen/user_directory.hpp>

namespace
{

using xen::KeymapBinding;
using xen::KeymapContexts;
using xen::KeymapOverride;
using xen::KeymapTarget;
using xen::KeymapTargetType;
using xen::KeymapTrigger;

auto trigger(std::string key, bool shift = false, bool command = false,
             bool alt = false, std::optional<std::string> input_mode = std::nullopt)
    -> KeymapTrigger
{
    return {
        .key = std::move(key),
        .shift = shift,
        .command = command,
        .alt = alt,
        .input_mode = std::move(input_mode),
    };
}

auto command(std::string value) -> KeymapTarget
{
    return {
        .type = KeymapTargetType::Command,
        .value = std::move(value),
    };
}

auto ui_action(std::string value, nlohmann::json arguments) -> KeymapTarget
{
    return {
        .type = KeymapTargetType::UiAction,
        .value = std::move(value),
        .arguments = std::move(arguments),
    };
}

void add(KeymapContexts &contexts, std::string const &context,
         KeymapTrigger binding_trigger, KeymapTarget target)
{
    contexts[context].push_back(
        {.trigger = std::move(binding_trigger), .target = std::move(target)});
}

void add(KeymapContexts &contexts, KeymapTrigger binding_trigger, KeymapTarget target)
{
    add(contexts, "sequence", std::move(binding_trigger), std::move(target));
}

auto command_ui_action(std::string value) -> KeymapTarget
{
    return ui_action(std::move(value), nlohmann::json::object());
}

auto trigger_to_json(KeymapTrigger const &value) -> nlohmann::json
{
    auto json = nlohmann::json{
        {"key", value.key},
        {"modifiers",
         {
             {"shift", value.shift},
             {"command", value.command},
             {"alt", value.alt},
         }},
    };
    if (value.input_mode.has_value())
    {
        json["when"] = {{"input_mode", *value.input_mode}};
    }
    return json;
}

auto target_to_json(KeymapTarget const &value) -> nlohmann::json
{
    if (value.type == KeymapTargetType::Command)
    {
        return {
            {"type", "command"},
            {"command", value.value},
        };
    }
    return {
        {"type", "ui_action"},
        {"action", value.value},
        {"arguments", value.arguments},
    };
}

auto trigger_from_json(nlohmann::json const &json) -> KeymapTrigger
{
    auto value = KeymapTrigger{.key = json.at("key").get<std::string>()};
    auto const &modifiers = json.at("modifiers");
    value.shift = modifiers.at("shift").get<bool>();
    value.command = modifiers.at("command").get<bool>();
    value.alt = modifiers.at("alt").get<bool>();
    if (json.contains("when"))
    {
        value.input_mode = json.at("when").at("input_mode").get<std::string>();
    }
    xen::validate(value);
    return value;
}

auto target_from_json(nlohmann::json const &json) -> KeymapTarget
{
    auto const type = json.at("type").get<std::string>();
    auto value = KeymapTarget{};
    if (type == "command")
    {
        value.type = KeymapTargetType::Command;
        value.value = json.at("command").get<std::string>();
    }
    else if (type == "ui_action")
    {
        value.type = KeymapTargetType::UiAction;
        value.value = json.at("action").get<std::string>();
        value.arguments = json.at("arguments");
    }
    else
    {
        throw std::invalid_argument{"Unknown keymap target type: " + type};
    }
    xen::validate(value);
    return value;
}

auto override_to_json(KeymapOverride const &value) -> nlohmann::json
{
    auto json = nlohmann::json{
        {"context", value.context},
        {"trigger", trigger_to_json(value.trigger)},
    };
    json["target"] = value.target.has_value() ? target_to_json(*value.target)
                                              : nlohmann::json(nullptr);
    return json;
}

auto override_from_json(nlohmann::json const &json) -> KeymapOverride
{
    auto value = KeymapOverride{
        .context = json.at("context").get<std::string>(),
        .trigger = trigger_from_json(json.at("trigger")),
    };
    if (!json.at("target").is_null())
    {
        value.target = target_from_json(json.at("target"));
    }
    xen::validate(value);
    return value;
}

auto is_context_name(std::string const &value) -> bool
{
    return !value.empty() && value.size() <= 64 &&
           std::ranges::all_of(value, [](unsigned char character) {
               return std::islower(character) != 0 || std::isdigit(character) != 0 ||
                      character == '_' || character == '-' || character == '.';
           });
}

auto effective_keymap(KeymapContexts contexts,
                      std::vector<KeymapOverride> const &overrides) -> KeymapContexts
{
    for (auto const &override : overrides)
    {
        auto &bindings = contexts[override.context];
        auto const existing =
            std::ranges::find(bindings, override.trigger, &KeymapBinding::trigger);
        if (!override.target.has_value())
        {
            if (existing != bindings.end())
            {
                bindings.erase(existing);
            }
        }
        else if (existing == bindings.end())
        {
            bindings.push_back(
                {.trigger = override.trigger, .target = *override.target});
        }
        else
        {
            existing->target = *override.target;
        }
    }

    std::erase_if(contexts, [](auto const &entry) { return entry.second.empty(); });
    return contexts;
}

} // namespace

namespace xen
{

auto default_keymap() -> KeymapContexts
{
    auto contexts = KeymapContexts{};
    auto const move = [](std::string direction) {
        return ui_action("selection.move",
                         {{"direction", std::move(direction)}, {"amount", 1}});
    };
    auto const composition_move = [](std::string direction) {
        return ui_action("composition.selection.move",
                         {{"direction", std::move(direction)}, {"amount", 1}});
    };
    auto const input_mode = [](std::string mode) {
        return ui_action("input_mode.set", {{"mode", std::move(mode)}});
    };

    add(contexts, trigger("Escape"), input_mode("pitch"));
    add(contexts, trigger("p"), input_mode("pitch"));
    add(contexts, trigger("v"), input_mode("velocity"));
    add(contexts, trigger("d"), input_mode("delay"));
    add(contexts, trigger("g"), input_mode("gate"));
    add(contexts, trigger("c"), input_mode("scale"));

    add(contexts, trigger("c", false, true), command("copy"));
    add(contexts, trigger("x", false, true), command("cut"));
    add(contexts, trigger("v", false, true), command("paste"));
    add(contexts, trigger("d", false, true), command("duplicate"));
    add(contexts, trigger("z", false, true), command("undo"));
    add(contexts, trigger("y", false, true), command("redo"));
    add(contexts, trigger("."), command("again"));

    add(contexts, trigger("h"), move("left"));
    add(contexts, trigger("ArrowLeft"), move("left"));
    add(contexts, trigger("l"), move("right"));
    add(contexts, trigger("ArrowRight"), move("right"));
    add(contexts, trigger("h", true), move("left"));
    add(contexts, trigger("ArrowLeft", true), move("left"));
    add(contexts, trigger("l", true), move("right"));
    add(contexts, trigger("ArrowRight", true), move("right"));
    add(contexts, trigger("j", true), move("down"));
    add(contexts, trigger("ArrowDown", true), move("down"));
    add(contexts, trigger("k", true), move("up"));
    add(contexts, trigger("ArrowUp", true), move("up"));

    auto const add_mode_shift = [&contexts](std::string mode, std::string key,
                                            std::string command_text,
                                            bool command_modifier = false) {
        add(contexts,
            trigger(std::move(key), false, command_modifier, false, std::move(mode)),
            command(std::move(command_text)));
    };
    add_mode_shift("pitch", "j", "shift Pitch -1");
    add_mode_shift("pitch", "ArrowDown", "shift Pitch -1");
    add_mode_shift("pitch", "k", "shift Pitch +1");
    add_mode_shift("pitch", "ArrowUp", "shift Pitch +1");
    add(contexts, trigger("PageDown"), command("shift Octave -1"));
    add(contexts, trigger("PageUp"), command("shift Octave +1"));

    for (auto const &[mode, field, coarse, fine] :
         std::vector<std::tuple<std::string, std::string, std::string, std::string>>{
             {"velocity", "Velocity", "0.015", "0.007"},
             {"delay", "Delay", "0.05", "0.01"},
             {"gate", "Gate", "0.05", "0.01"},
         })
    {
        add_mode_shift(mode, "j", "shift " + field + " -" + coarse);
        add_mode_shift(mode, "ArrowDown", "shift " + field + " -" + coarse);
        add_mode_shift(mode, "k", "shift " + field + " +" + coarse);
        add_mode_shift(mode, "ArrowUp", "shift " + field + " +" + coarse);
        add_mode_shift(mode, "j", "shift " + field + " -" + fine, true);
        add_mode_shift(mode, "ArrowDown", "shift " + field + " -" + fine, true);
        add_mode_shift(mode, "k", "shift " + field + " +" + fine, true);
        add_mode_shift(mode, "ArrowUp", "shift " + field + " +" + fine, true);
    }

    add_mode_shift("scale", "j", "shift entireScale -1");
    add_mode_shift("scale", "ArrowDown", "shift entireScale -1");
    add_mode_shift("scale", "k", "shift entireScale +1");
    add_mode_shift("scale", "ArrowUp", "shift entireScale +1");
    add_mode_shift("scale", "j", "shift scale -1", true);
    add_mode_shift("scale", "ArrowDown", "shift scale -1", true);
    add_mode_shift("scale", "k", "shift scale +1", true);
    add_mode_shift("scale", "ArrowUp", "shift scale +1", true);

    add(contexts, trigger("+", true), command("double measure timeSignature"));
    add(contexts, trigger("-"), command("halve measure timeSignature"));
    add(contexts, trigger("Delete"), command("delete"));
    add(contexts, trigger("s"), command("split :N=2:"));
    add(contexts, trigger("n"), command("note :N=0:"));
    add(contexts, trigger("r"), command("rest"));

    add(contexts, trigger("k", false, true), command_ui_action("command.open"));
    add(contexts, trigger(":"), command_ui_action("command.open"));

    add(contexts, "sequence", trigger("Tab"),
        command_ui_action("workspace.view.composition"));

    add(contexts, "composition", trigger("h"), composition_move("left"));
    add(contexts, "composition", trigger("ArrowLeft"), composition_move("left"));
    add(contexts, "composition", trigger("l"), composition_move("right"));
    add(contexts, "composition", trigger("ArrowRight"), composition_move("right"));
    add(contexts, "composition", trigger("j"), composition_move("down"));
    add(contexts, "composition", trigger("ArrowDown"), composition_move("down"));
    add(contexts, "composition", trigger("k"), composition_move("up"));
    add(contexts, "composition", trigger("ArrowUp"), composition_move("up"));
    add(contexts, "composition", trigger("Enter"),
        command_ui_action("composition.cell.edit_measure"));
    add(contexts, "composition", trigger("["),
        command_ui_action("composition.loop.set_start"));
    add(contexts, "composition", trigger("]"),
        command_ui_action("composition.loop.set_end"));
    add(contexts, "composition", trigger("Tab"),
        command_ui_action("workspace.view.sequencer"));

    add(contexts, "command.input", trigger("Escape"),
        command_ui_action("command.cancel"));
    add(contexts, "command.input", trigger("Enter"),
        command_ui_action("command.submit"));
    add(contexts, "command.input", trigger("ArrowUp"),
        command_ui_action("command.history.previous"));
    add(contexts, "command.input", trigger("ArrowDown"),
        command_ui_action("command.history.next"));
    add(contexts, "command.input", trigger("Tab"),
        command_ui_action("command.completion.accept"));

    add(contexts, "command.completions", trigger("Escape"),
        command_ui_action("command.completion.dismiss"));
    add(contexts, "command.completions", trigger("Enter"),
        command_ui_action("command.completion.accept"));
    add(contexts, "command.completions", trigger("Tab"),
        command_ui_action("command.completion.accept"));
    add(contexts, "command.completions", trigger("ArrowUp"),
        command_ui_action("command.completion.previous"));
    add(contexts, "command.completions", trigger("ArrowDown"),
        command_ui_action("command.completion.next"));
    return contexts;
}

void validate(KeymapTrigger const &value)
{
    if (value.key.empty() || value.key.size() > 64)
    {
        throw std::invalid_argument{"Keymap trigger key must contain 1 to 64 bytes."};
    }
    if (value.key.size() == 1 && value.key.front() >= 'A' && value.key.front() <= 'Z')
    {
        throw std::invalid_argument{
            "Single ASCII letter keymap triggers must be lowercase."};
    }
    if (value.input_mode.has_value())
    {
        auto const &mode = *value.input_mode;
        if (mode != "pitch" && mode != "velocity" && mode != "delay" &&
            mode != "gate" && mode != "scale")
        {
            throw std::invalid_argument{"Unknown keymap input mode: " + mode};
        }
    }
}

void validate(KeymapTarget const &value)
{
    if (value.value.empty() || value.value.size() > 4'096)
    {
        throw std::invalid_argument{"Keymap target must contain 1 to 4096 bytes."};
    }
    if (value.type == KeymapTargetType::Command)
    {
        if (!value.arguments.is_object() || !value.arguments.empty())
        {
            throw std::invalid_argument{
                "Command keymap targets cannot have arguments."};
        }
        return;
    }
    if (!value.arguments.is_object())
    {
        throw std::invalid_argument{"UI action arguments must be an object."};
    }
    if (value.value == "selection.move" || value.value == "composition.selection.move")
    {
        auto const direction = value.arguments.at("direction").get<std::string>();
        auto const amount = value.arguments.at("amount").get<int>();
        if ((direction != "left" && direction != "right" && direction != "up" &&
             direction != "down") ||
            amount < 1 || amount > 1'000 || value.arguments.size() != 2)
        {
            throw std::invalid_argument{"Invalid selection move arguments."};
        }
        return;
    }
    if (value.value == "input_mode.set")
    {
        auto const mode = value.arguments.at("mode").get<std::string>();
        if ((mode != "pitch" && mode != "velocity" && mode != "delay" &&
             mode != "gate" && mode != "scale") ||
            value.arguments.size() != 1)
        {
            throw std::invalid_argument{"Invalid input_mode.set arguments."};
        }
        return;
    }
    if (value.value == "command.open" || value.value == "command.cancel" ||
        value.value == "command.submit" || value.value == "command.close_if_empty" ||
        value.value == "command.history.previous" ||
        value.value == "command.history.next" ||
        value.value == "command.completion.accept" ||
        value.value == "command.completion.dismiss" ||
        value.value == "command.completion.previous" ||
        value.value == "command.completion.next" ||
        value.value == "composition.cell.edit_measure" ||
        value.value == "composition.loop.set_start" ||
        value.value == "composition.loop.set_end" ||
        value.value == "workspace.view.toggle" ||
        value.value == "workspace.view.composition" ||
        value.value == "workspace.view.sequencer")
    {
        if (!value.arguments.empty())
        {
            throw std::invalid_argument{
                "No-argument UI actions cannot have arguments."};
        }
        return;
    }
    throw std::invalid_argument{"Unknown UI action: " + value.value};
}

void validate(KeymapOverride const &value)
{
    if (!is_context_name(value.context))
    {
        throw std::invalid_argument{"Invalid keymap context: " + value.context};
    }
    validate(value.trigger);
    if (value.target.has_value())
    {
        validate(*value.target);
    }
}

KeymapStore::KeymapStore(std::filesystem::path file) : file_{std::move(file)}
{
    load();
}

auto KeymapStore::default_file() -> std::filesystem::path
{
    return std::filesystem::path{
               get_user_settings_directory().getFullPathName().toStdString()} /
           "keymap.json";
}

void KeymapStore::load()
{
    auto const text = read_text_file(file_);
    if (!text.has_value())
    {
        save();
        return;
    }
    if (text->size() > (4 * 1'024 * 1'024))
    {
        throw std::runtime_error{"Keymap settings file exceeds 4MB."};
    }

    try
    {
        auto const json = nlohmann::json::parse(*text);
        if (json.at("schema_version").get<int>() != KEYMAP_SCHEMA_VERSION)
        {
            throw std::invalid_argument{"Unsupported keymap schema."};
        }
        revision_ = json.at("revision").get<std::uint64_t>();
        if (revision_ == 0)
        {
            throw std::invalid_argument{"Keymap revision must be positive."};
        }
        for (auto const &entry : json.at("overrides"))
        {
            auto value = override_from_json(entry);
            auto const duplicate = std::ranges::find_if(
                overrides_, [&value](KeymapOverride const &existing) {
                    return existing.context == value.context &&
                           existing.trigger == value.trigger;
                });
            if (duplicate != overrides_.end())
            {
                throw std::invalid_argument{"Duplicate keymap override."};
            }
            overrides_.push_back(std::move(value));
        }
    }
    catch (std::exception const &error)
    {
        throw std::runtime_error{"Unable to load keymap settings " + file_.string() +
                                 ": " + error.what()};
    }
}

void KeymapStore::save() const
{
    auto entries = nlohmann::json::array();
    for (auto const &entry : overrides_)
    {
        entries.push_back(override_to_json(entry));
    }
    auto const json = nlohmann::json{
        {"schema_version", KEYMAP_SCHEMA_VERSION},
        {"revision", revision_},
        {"overrides", std::move(entries)},
    };

    atomic_write_text_file(file_, json.dump(2));
}

void KeymapStore::require_revision(std::uint64_t expected_revision) const
{
    if (expected_revision != revision_)
    {
        throw std::invalid_argument{"Stale keymap revision: expected " +
                                    std::to_string(expected_revision) + ", current " +
                                    std::to_string(revision_)};
    }
}

auto KeymapStore::snapshot() const -> KeymapSnapshot
{
    return {
        .revision = revision_,
        .bindings = effective_keymap(default_keymap(), overrides_),
        .overrides = overrides_,
    };
}

auto KeymapStore::revision() const noexcept -> std::uint64_t
{
    return revision_;
}

auto KeymapStore::set_override(std::uint64_t expected_revision, std::string context,
                               KeymapTrigger binding_trigger,
                               std::optional<KeymapTarget> target) -> KeymapSnapshot
{
    require_revision(expected_revision);
    auto value = KeymapOverride{
        .context = std::move(context),
        .trigger = std::move(binding_trigger),
        .target = std::move(target),
    };
    validate(value);
    auto const existing =
        std::ranges::find_if(overrides_, [&value](KeymapOverride const &entry) {
            return entry.context == value.context && entry.trigger == value.trigger;
        });
    if (existing != overrides_.end() && existing->target == value.target)
    {
        return snapshot();
    }
    auto const previous_overrides = overrides_;
    auto const previous_revision = revision_;
    if (existing == overrides_.end())
    {
        overrides_.push_back(std::move(value));
    }
    else
    {
        existing->target = std::move(value.target);
    }
    ++revision_;
    try
    {
        save();
    }
    catch (...)
    {
        overrides_ = previous_overrides;
        revision_ = previous_revision;
        throw;
    }
    return snapshot();
}

auto KeymapStore::remove_override(std::uint64_t expected_revision,
                                  std::string const &context,
                                  KeymapTrigger const &binding_trigger)
    -> KeymapSnapshot
{
    require_revision(expected_revision);
    auto const existing =
        std::ranges::find_if(overrides_, [&](KeymapOverride const &entry) {
            return entry.context == context && entry.trigger == binding_trigger;
        });
    if (existing == overrides_.end())
    {
        return snapshot();
    }
    auto const previous_overrides = overrides_;
    overrides_.erase(existing);
    auto const previous_revision = revision_;
    ++revision_;
    try
    {
        save();
    }
    catch (...)
    {
        overrides_ = previous_overrides;
        revision_ = previous_revision;
        throw;
    }
    return snapshot();
}

auto KeymapStore::reset(std::uint64_t expected_revision) -> KeymapSnapshot
{
    require_revision(expected_revision);
    if (overrides_.empty())
    {
        return snapshot();
    }
    auto const previous_overrides = overrides_;
    auto const previous_revision = revision_;
    overrides_.clear();
    ++revision_;
    try
    {
        save();
    }
    catch (...)
    {
        overrides_ = previous_overrides;
        revision_ = previous_revision;
        throw;
    }
    return snapshot();
}

auto KeymapStore::file() const -> std::filesystem::path const &
{
    return file_;
}

} // namespace xen
