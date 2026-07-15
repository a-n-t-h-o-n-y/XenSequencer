#include <xen/bridge_serialize.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
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
    auto midi_cc = nlohmann::json::array();
    for (auto const &[controller, value] : note.midi_cc)
    {
        midi_cc.push_back({
            {"controller", static_cast<unsigned>(controller)},
            {"value", value},
        });
    }
    return nlohmann::json{
        {"type", "Note"},      {"pitch", note.pitch}, {"velocity", note.velocity},
        {"delay", note.delay}, {"gate", note.gate},   {"midi_cc", std::move(midi_cc)},
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

auto pitch_to_json(xen::PitchSystem const &pitch) -> nlohmann::json;

auto time_signature_to_json(sequence::TimeSignature const &time_signature)
    -> nlohmann::json
{
    return nlohmann::json{
        {"numerator", time_signature.numerator},
        {"denominator", time_signature.denominator},
    };
}

auto sequence_bank_to_json(xen::SequenceBank const &bank) -> nlohmann::json
{
    auto sequences = nlohmann::json::array();
    for (auto const &entry : bank.sequences)
    {
        auto sequence = nlohmann::json{
            {"id", entry.id},
            {"cell", cell_to_json(entry.cell)},
        };
        if (entry.name.has_value())
        {
            sequence["name"] = *entry.name;
        }
        sequences.push_back(std::move(sequence));
    }
    return nlohmann::json{
        {"next_id", bank.next_id},
        {"sequences", std::move(sequences)},
    };
}

auto composition_to_json(xen::Composition const &composition) -> nlohmann::json
{
    auto columns = nlohmann::json::array();
    for (auto const &[coordinate, column] : composition.columns)
    {
        columns.push_back({{"coordinate", coordinate},
                           {"duration", time_signature_to_json(column.duration)},
                           {"pitch", pitch_to_json(column.pitch)}});
    }

    auto rows = nlohmann::json::array();
    for (auto const &[coordinate, row] : composition.rows)
    {
        auto row_json = nlohmann::json{
            {"coordinate", coordinate},
            {"channel_id", row.channel_id},
        };
        if (row.name.has_value())
        {
            row_json["name"] = *row.name;
        }
        rows.push_back(std::move(row_json));
    }

    auto placements = nlohmann::json::array();
    for (auto const &[position, sequence_id] : composition.placements)
    {
        placements.push_back({{"row", position.row_coordinate},
                              {"column", position.column_coordinate},
                              {"sequence_id", sequence_id}});
    }

    auto loop_region = nlohmann::json::object();
    loop_region["start_column"] = composition.loop_region.start_column;
    loop_region["end_column"] = composition.loop_region.end_column;

    return nlohmann::json{
        {"default_column",
         {{"duration", time_signature_to_json(composition.default_column.duration)},
          {"pitch", pitch_to_json(composition.default_column.pitch)}}},
        {"columns", std::move(columns)},
        {"rows", std::move(rows)},
        {"placements", std::move(placements)},
        {"loop_region", std::move(loop_region)},
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

auto active_scale_to_json(xen::ActiveScale const &scale) -> nlohmann::json
{
    return nlohmann::json{
        {"source_id", scale.source_id.has_value() ? nlohmann::json(*scale.source_id)
                                                  : nlohmann::json(nullptr)},
        {"definition", scale_to_json(scale.definition)},
    };
}

auto pitch_to_json(xen::PitchSystem const &pitch) -> nlohmann::json
{
    auto json = nlohmann::json{
        {"tuning",
         {
             {"name", pitch.tuning.name},
             {"definition", tuning_to_json(pitch.tuning.definition)},
         }},
        {"scale", nullptr},
        {"transposition", pitch.transposition},
        {"translation_direction", direction_to_json(pitch.translation_direction)},
        {"base_frequency", pitch.base_frequency},
    };
    if (pitch.scale.has_value())
    {
        json["scale"] = active_scale_to_json(*pitch.scale);
    }
    return json;
}

auto project_to_json(xen::ProjectState const &project) -> nlohmann::json
{
    auto midi_cc_labels = nlohmann::json::array();
    for (auto const &[controller, label] : project.midi_cc_labels)
    {
        midi_cc_labels.push_back({
            {"controller", static_cast<unsigned>(controller)},
            {"label", label},
        });
    }
    return nlohmann::json{
        {"sequence_bank", sequence_bank_to_json(project.sequence_bank)},
        {"composition", composition_to_json(project.composition)},
        {"midi_cc_labels", std::move(midi_cc_labels)},
    };
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
                            ? nlohmann::json(*constraint.minimum)
                            : nlohmann::json(nullptr)},
            {"maximum", constraint.maximum.has_value()
                            ? nlohmann::json(*constraint.maximum)
                            : nlohmann::json(nullptr)},
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
        {"keywords", command.keywords},
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

auto make_project_snapshot(ProjectSnapshot const &snapshot) -> nlohmann::json
{
    auto const optional_text = [](std::optional<std::string> const &value) {
        return value.has_value() ? nlohmann::json(*value) : nlohmann::json(nullptr);
    };
    auto display_name = std::string{"Untitled"};
    if (snapshot.document.relative_path.has_value())
    {
        display_name =
            std::filesystem::path{*snapshot.document.relative_path}.stem().string();
    }
    return nlohmann::json{
        {"schema_version", project_schema_version},
        {"history_entry_id", std::to_string(snapshot.history_entry_id.value())},
        {"project_revision", std::to_string(snapshot.project_revision.value())},
        {"state_revision", std::to_string(snapshot.state_revision.value())},
        {"preview_active", snapshot.preview_active},
        {"document",
         {{"relative_path", optional_text(snapshot.document.relative_path)},
          {"display_name", display_name},
          {"dirty", snapshot.document.dirty},
          {"file_revision", optional_text(snapshot.document.file_revision)}}},
        {"recovery",
         snapshot.recovery.has_value()
             ? nlohmann::json{
                   {"revision", snapshot.recovery->revision},
                   {"saved_at_unix_ms",
                    std::to_string(snapshot.recovery->saved_at_unix_ms)},
                   {"relative_path",
                    optional_text(snapshot.recovery->relative_path)},
                   {"project_revision",
                    std::to_string(snapshot.recovery->project_revision.value())},
               }
             : nlohmann::json(nullptr)},
        {"project", detail::project_to_json(snapshot.project)},
    };
}

auto make_instance_binding(InstanceBinding const &binding) -> nlohmann::json
{
    return nlohmann::json{
        {"session_id", binding.session_id},
        {"instance_id", binding.instance_id},
        {"channel_id", binding.channel_id},
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

auto make_modulation_catalog_payload() -> nlohmann::json
{
    return modulation_catalog_to_json();
}

auto make_keymap_payload(KeymapResource const &resource) -> nlohmann::json
{
    return nlohmann::json{
        {"revision", std::to_string(resource.revision)},
        {"document",
         resource.document.has_value() ? *resource.document : nlohmann::json(nullptr)},
    };
}

auto make_preferences_payload(PreferencesResource const &resource) -> nlohmann::json
{
    return nlohmann::json{
        {"revision", std::to_string(resource.revision)},
        {"document",
         resource.document.has_value() ? *resource.document : nlohmann::json(nullptr)},
    };
}

} // namespace xen::bridge
