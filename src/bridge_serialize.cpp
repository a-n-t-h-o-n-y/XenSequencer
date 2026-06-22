#include <xen/bridge_serialize.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <sequence/sequence.hpp>

#include <xen/scale.hpp>

namespace xen::bridge::detail
{

auto cell_to_json(sequence::Cell const &cell) -> nlohmann::json;
auto element_to_json(sequence::MusicElement const &element) -> nlohmann::json;

auto note_to_json(sequence::Note const &note) -> nlohmann::json
{
    return nlohmann::json{
        {"type", "Note"},      {"pitch", note.pitch}, {"velocity", note.velocity},
        {"delay", note.delay}, {"gate", note.gate},
    };
}

auto sequence_to_json(sequence::Sequence const &sequence) -> nlohmann::json
{
    auto cells = nlohmann::json::array();
    for (auto const &cell : sequence.cells)
    {
        cells.push_back(cell_to_json(cell));
    }

    return nlohmann::json{
        {"type", "Sequence"},
        {"cells", std::move(cells)},
    };
}

auto element_to_json(sequence::MusicElement const &element) -> nlohmann::json
{
    return std::visit(
        [&](auto const &typed) {
            using Typed = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<Typed, sequence::Note>)
            {
                return note_to_json(typed);
            }
            else
            {
                return sequence_to_json(typed);
            }
        },
        element);
}

auto cell_to_json(sequence::Cell const &cell) -> nlohmann::json
{
    auto elements = nlohmann::json::array();
    for (auto const &element : cell.elements)
    {
        elements.push_back(element_to_json(element));
    }

    return nlohmann::json{
        {"weight", cell.weight},
        {"elements", std::move(elements)},
    };
}

auto measure_to_json(xen::Measure const &measure) -> nlohmann::json
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
    auto result = nlohmann::json{
        {"measure", measure_to_json(engine.measure)},
        {"tuning", tuning_to_json(engine.tuning)},
        {"tuning_name", engine.tuning_name},
        {"scale", nullptr},
        {"key", engine.key},
        {"scale_translate_direction",
         direction_to_json(engine.scale_translate_direction)},
        {"base_frequency", engine.base_frequency},
    };

    if (engine.scale.has_value())
    {
        result["scale"] = scale_to_json(*engine.scale);
    }

    return result;
}

auto catalog_argument_to_json(xen::CatalogArgumentMetadata const &argument)
    -> nlohmann::json
{
    auto constraints = nlohmann::json::array();
    for (auto const &constraint : argument.constraints)
    {
        constraints.push_back({
            {"kind", constraint.kind},
            {"minimum", constraint.minimum.has_value()
                            ? nlohmann::json{*constraint.minimum}
                            : nlohmann::json{nullptr}},
            {"maximum", constraint.maximum.has_value()
                            ? nlohmann::json{*constraint.maximum}
                            : nlohmann::json{nullptr}},
            {"values", constraint.values},
        });
    }
    auto result = nlohmann::json{
        {"kind", argument.kind},
        {"display_name", argument.display_name},
        {"required", argument.required},
        {"default_value", nullptr},
        {"constraints", std::move(constraints)},
    };
    if (argument.default_value.has_value())
    {
        result["default_value"] = *argument.default_value;
    }
    return result;
}

auto target_requirement_to_string(xen::TargetRequirement target) -> std::string
{
    switch (target)
    {
    case xen::TargetRequirement::None:
        return "none";
    case xen::TargetRequirement::Cell:
        return "cell";
    case xen::TargetRequirement::Element:
        return "element";
    case xen::TargetRequirement::CellOrElement:
        return "cell_or_element";
    }
    return "none";
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
        {"target_requirement", target_requirement_to_string(command.target)},
        {"arguments", std::move(arguments)},
        {"description", command.description},
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
        {"history_entry_id", snapshot.history_entry_id.value()},
        {"project_revision", snapshot.project_revision.value()},
        {"engine", detail::engine_to_json(snapshot.engine)},
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
        {"schema_version", catalog_schema_version},
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

} // namespace xen::bridge
