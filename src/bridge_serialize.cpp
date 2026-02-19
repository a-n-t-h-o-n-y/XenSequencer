#include <xen/bridge_serialize.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <sequence/measure.hpp>
#include <sequence/sequence.hpp>

#include <xen/input_mode.hpp>
#include <xen/scale.hpp>

namespace xen::bridge::detail
{

auto cell_to_json(sequence::Cell const &cell) -> nlohmann::json;

auto note_to_json(sequence::Note const &note, float weight) -> nlohmann::json
{
    return nlohmann::json{
        {"type", "Note"},
        {"weight", weight},
        {"pitch", note.pitch},
        {"velocity", note.velocity},
        {"delay", note.delay},
        {"gate", note.gate},
    };
}

auto rest_to_json(sequence::Rest const &, float weight) -> nlohmann::json
{
    return nlohmann::json{
        {"type", "Rest"},
        {"weight", weight},
    };
}

auto sequence_to_json(sequence::Sequence const &sequence, float weight)
    -> nlohmann::json
{
    auto cells = nlohmann::json::array();
    for (auto const &cell : sequence.cells)
    {
        cells.push_back(cell_to_json(cell));
    }

    return nlohmann::json{
        {"type", "Sequence"},
        {"weight", weight},
        {"cells", std::move(cells)},
    };
}

auto cell_to_json(sequence::Cell const &cell) -> nlohmann::json
{
    return std::visit(
        [&](auto const &typed) {
            using Typed = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<Typed, sequence::Note>)
            {
                return note_to_json(typed, cell.weight);
            }
            else if constexpr (std::is_same_v<Typed, sequence::Rest>)
            {
                return rest_to_json(typed, cell.weight);
            }
            else
            {
                return sequence_to_json(typed, cell.weight);
            }
        },
        cell.element);
}

auto measure_to_json(sequence::Measure const &measure) -> nlohmann::json
{
    return nlohmann::json{
        {"cell", cell_to_json(measure.cell)},
        {"time_signature",
         {
             {"numerator", measure.time_signature.numerator},
             {"denominator", measure.time_signature.denominator},
         }},
    };
}

auto tuning_to_json(sequence::Tuning const &tuning) -> nlohmann::json
{
    return nlohmann::json{
        {"intervals", tuning.intervals},
        {"octave", tuning.octave},
    };
}

auto scale_to_json(xen::Scale const &scale) -> nlohmann::json
{
    return nlohmann::json{
        {"name", scale.name},
        {"tuning_length", scale.tuning_length},
        {"intervals", scale.intervals},
        {"mode", scale.mode},
    };
}

auto chord_to_json(xen::Chord const &chord) -> nlohmann::json
{
    return nlohmann::json{
        {"name", chord.name},
        {"intervals", chord.intervals},
    };
}

auto direction_to_json(xen::TranslateDirection direction) -> nlohmann::json
{
    switch (direction)
    {
    case xen::TranslateDirection::Up:
        return "up";
    case xen::TranslateDirection::Down:
        return "down";
    }
    return "up";
}

auto engine_to_json(xen::EngineState const &engine) -> nlohmann::json
{
    auto sequence_bank = nlohmann::json::array();
    for (auto const &measure : engine.sequence_bank)
    {
        sequence_bank.push_back(measure_to_json(measure));
    }

    auto sequence_names = nlohmann::json::array();
    for (auto const &name : engine.sequence_names)
    {
        sequence_names.push_back(name);
    }

    auto result = nlohmann::json{
        {"sequence_bank", std::move(sequence_bank)},
        {"sequence_names", std::move(sequence_names)},
        {"tuning", tuning_to_json(engine.tuning)},
        {"tuning_name", engine.tuning_name},
        {"scale", nullptr},
        {"key", engine.key},
        {"scale_translate_direction", direction_to_json(engine.scale_translate_direction)},
        {"base_frequency", engine.base_frequency},
    };

    if (engine.scale.has_value())
    {
        result["scale"] = scale_to_json(*engine.scale);
    }

    return result;
}

auto editor_to_json(xen::EditorSessionState const &editor) -> nlohmann::json
{
    return nlohmann::json{
        {"selected",
         {
             {"measure", editor.selected.measure},
             {"cell", editor.selected.cell},
         }},
        {"input_mode", xen::to_string(editor.input_mode)},
    };
}

auto catalog_argument_to_json(xen::CatalogArgumentMetadata const &argument)
    -> nlohmann::json
{
    auto result = nlohmann::json{
        {"type", argument.type},
        {"name", argument.name},
        {"default_value", nullptr},
    };
    if (argument.default_value.has_value())
    {
        result["default_value"] = *argument.default_value;
    }
    return result;
}

auto catalog_command_to_json(xen::CatalogCommandMetadata const &command)
    -> nlohmann::json
{
    auto arguments = nlohmann::json::array();
    for (auto const &arg : command.arguments)
    {
        arguments.push_back(catalog_argument_to_json(arg));
    }

    return nlohmann::json{
        {"path", command.path},
        {"accepts_pattern_prefix", command.accepts_pattern_prefix},
        {"arguments", std::move(arguments)},
        {"description", command.description},
    };
}

auto command_reference_to_json(xen::Documentation const &doc) -> nlohmann::json
{
    auto signature = std::string{};
    if (doc.signature.pattern_arg)
    {
        signature += "[pattern] ";
    }
    signature += doc.signature.id;
    for (auto const &argument : doc.signature.arguments)
    {
        signature += " " + argument;
    }

    return nlohmann::json{
        {"id", doc.signature.id},
        {"signature", signature},
        {"description", doc.description},
    };
}

} // namespace xen::bridge::detail

namespace xen::bridge
{

auto to_string(MessageLevel level) -> std::string
{
    switch (level)
    {
    case MessageLevel::Debug:
        return "debug";
    case MessageLevel::Info:
        return "info";
    case MessageLevel::Warning:
        return "warning";
    case MessageLevel::Error:
        return "error";
    }
    return "error";
}

auto make_ui_state_snapshot(EngineSnapshot const &snapshot,
                            ContentLibraryState const &library) -> nlohmann::json
{
    auto scales = nlohmann::json::array();
    for (auto const &scale : library.scales)
    {
        scales.push_back(detail::scale_to_json(scale));
    }

    auto chords = nlohmann::json::array();
    for (auto const &chord : library.chords)
    {
        chords.push_back(detail::chord_to_json(chord));
    }

    return nlohmann::json{
        {"schema_version", snapshot_schema_version},
        {"snapshot_version", snapshot.snapshot_version},
        {"commit_id", snapshot.commit_id},
        {"engine", detail::engine_to_json(snapshot.engine)},
        {"editor", detail::editor_to_json(snapshot.editor)},
        {"library",
         {
             {"scales", std::move(scales)},
             {"chords", std::move(chords)},
         }},
    };
}

auto make_catalog_payload(std::vector<CatalogCommandMetadata> const &commands)
    -> nlohmann::json
{
    auto list = nlohmann::json::array();
    for (auto const &command : commands)
    {
        list.push_back(detail::catalog_command_to_json(command));
    }

    return nlohmann::json{
        {"commands", std::move(list)},
    };
}

auto make_keymap_payload(
    std::map<std::string, std::map<std::string, std::string>> const &keymap)
    -> nlohmann::json
{
    auto out = nlohmann::json::object();
    for (auto const &[component, mappings] : keymap)
    {
        auto mapping_json = nlohmann::json::object();
        for (auto const &[key_combo, command] : mappings)
        {
            mapping_json[key_combo] = command;
        }
        out[component] = std::move(mapping_json);
    }

    return nlohmann::json{
        {"keymap", std::move(out)},
    };
}

auto make_reference_payload(
    std::vector<Documentation> const &docs,
    std::map<std::string, std::map<std::string, std::string>> const &keymap)
    -> nlohmann::json
{
    auto commands = nlohmann::json::array();
    for (auto const &doc : docs)
    {
        commands.push_back(detail::command_reference_to_json(doc));
    }

    auto keybindings = nlohmann::json::array();
    for (auto const &[component, mappings] : keymap)
    {
        auto bindings = nlohmann::json::array();
        for (auto const &[key, command] : mappings)
        {
            bindings.push_back(nlohmann::json{
                {"key", key},
                {"command", command},
            });
        }

        keybindings.push_back(nlohmann::json{
            {"component", component},
            {"bindings", std::move(bindings)},
        });
    }

    return nlohmann::json{
        {"commands", std::move(commands)},
        {"keybindings", std::move(keybindings)},
    };
}

} // namespace xen::bridge
