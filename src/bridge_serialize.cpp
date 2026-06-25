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
    };
}

auto time_signature_to_json(sequence::TimeSignature const &time_signature)
    -> nlohmann::json
{
    return nlohmann::json{
        {"numerator", time_signature.numerator},
        {"denominator", time_signature.denominator},
    };
}

auto measure_bank_to_json(xen::MeasureBank const &bank) -> nlohmann::json
{
    auto measures = nlohmann::json::array();
    for (auto const &entry : bank.measures)
    {
        auto measure = nlohmann::json{
            {"id", entry.id},
            {"measure", measure_to_json(entry.measure)},
        };
        if (entry.name.has_value())
        {
            measure["name"] = *entry.name;
        }
        measures.push_back(std::move(measure));
    }
    return nlohmann::json{
        {"next_id", bank.next_id},
        {"measures", std::move(measures)},
    };
}

auto composition_to_json(xen::Composition const &composition) -> nlohmann::json
{
    auto columns = nlohmann::json::array();
    for (auto const &column : composition.columns)
    {
        columns.push_back({{"length", time_signature_to_json(column.length)}});
    }

    auto rows = nlohmann::json::array();
    for (auto const &row : composition.rows)
    {
        auto cells = nlohmann::json::array();
        for (auto const &cell : row.cells)
        {
            cells.push_back(cell.has_value() ? nlohmann::json(*cell)
                                             : nlohmann::json(nullptr));
        }
        auto row_json = nlohmann::json{
            {"output_id", row.output_id},
            {"cells", std::move(cells)},
        };
        if (row.name.has_value())
        {
            row_json["name"] = *row.name;
        }
        rows.push_back(std::move(row_json));
    }

    auto loop_region = nlohmann::json::object();
    loop_region["start_column"] = composition.loop_region.start_column;
    loop_region["end_column"] = composition.loop_region.end_column;

    return nlohmann::json{
        {"columns", std::move(columns)},
        {"rows", std::move(rows)},
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

auto project_to_json(xen::ProjectState const &project) -> nlohmann::json
{
    auto pitch = nlohmann::json{
        {"tuning",
         {
             {"name", project.pitch.tuning.name},
             {"definition", tuning_to_json(project.pitch.tuning.definition)},
         }},
        {"scale", nullptr},
        {"transposition", project.pitch.transposition},
        {"translation_direction",
         direction_to_json(project.pitch.translation_direction)},
        {"base_frequency", project.pitch.base_frequency},
    };
    if (project.pitch.scale.has_value())
    {
        pitch["scale"] = active_scale_to_json(*project.pitch.scale);
    }
    return nlohmann::json{
        {"measure_bank", measure_bank_to_json(project.measure_bank)},
        {"composition", composition_to_json(project.composition)},
        {"pitch", std::move(pitch)},
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
    return nlohmann::json{
        {"schema_version", project_schema_version},
        {"history_entry_id", snapshot.history_entry_id.value()},
        {"project_revision", snapshot.project_revision.value()},
        {"project", detail::project_to_json(snapshot.project)},
    };
}

auto make_instance_binding(InstanceBinding const &binding) -> nlohmann::json
{
    return nlohmann::json{
        {"session_id", binding.session_id},
        {"instance_id", binding.instance_id},
        {"output_id", binding.output_id},
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

auto make_keymap_payload(KeymapSnapshot const &snapshot) -> nlohmann::json
{
    auto trigger_to_json = [](KeymapTrigger const &trigger) {
        auto json = nlohmann::json{
            {"key", trigger.key},
            {"modifiers",
             {
                 {"shift", trigger.shift},
                 {"command", trigger.command},
                 {"alt", trigger.alt},
             }},
        };
        if (trigger.input_mode.has_value())
        {
            json["when"] = {{"input_mode", *trigger.input_mode}};
        }
        return json;
    };
    auto target_to_json = [](KeymapTarget const &target) {
        if (target.type == KeymapTargetType::Command)
        {
            return nlohmann::json{
                {"type", "command"},
                {"command", target.value},
            };
        }
        return nlohmann::json{
            {"type", "ui_action"},
            {"action", target.value},
            {"arguments", target.arguments},
        };
    };

    auto bindings_json = nlohmann::json::object();
    for (auto const &[context, bindings] : snapshot.bindings)
    {
        auto context_json = nlohmann::json::array();
        for (auto const &binding : bindings)
        {
            context_json.push_back({
                {"trigger", trigger_to_json(binding.trigger)},
                {"target", target_to_json(binding.target)},
            });
        }
        bindings_json[context] = std::move(context_json);
    }

    auto overrides_json = nlohmann::json::array();
    for (auto const &entry : snapshot.overrides)
    {
        overrides_json.push_back({
            {"context", entry.context},
            {"trigger", trigger_to_json(entry.trigger)},
            {"target", entry.target.has_value() ? target_to_json(*entry.target)
                                                : nlohmann::json(nullptr)},
        });
    }
    return nlohmann::json{
        {"schema_version", KEYMAP_SCHEMA_VERSION},
        {"revision", snapshot.revision},
        {"key_semantics", "KeyboardEvent.key"},
        {"bindings", std::move(bindings_json)},
        {"overrides", std::move(overrides_json)},
    };
}

} // namespace xen::bridge
