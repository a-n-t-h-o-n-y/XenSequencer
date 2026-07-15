#include <xen/modulation_json.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace xen
{
namespace
{

auto require_finite_number(nlohmann::json const &json, char const *field) -> double
{
    if (!json.contains(field) || !json.at(field).is_number())
    {
        throw std::invalid_argument{std::string{"Field must be numeric: "} + field};
    }
    auto const value = json.at(field).get<double>();
    if (!std::isfinite(value))
    {
        throw std::invalid_argument{std::string{"Field must be finite: "} + field};
    }
    return value;
}

auto require_float(nlohmann::json const &json, char const *field) -> float
{
    auto const value = require_finite_number(json, field);
    if (value < -std::numeric_limits<float>::max() ||
        value > std::numeric_limits<float>::max())
    {
        throw std::invalid_argument{std::string{"Field is out of range: "} + field};
    }
    return static_cast<float>(value);
}

auto require_size(nlohmann::json const &json, char const *field) -> std::size_t
{
    if (!json.contains(field) || !json.at(field).is_number_unsigned())
    {
        throw std::invalid_argument{std::string{"Field must be an unsigned integer: "} +
                                    field};
    }
    return json.at(field).get<std::size_t>();
}

auto shape_name(WaveformShape shape) -> std::string_view
{
    switch (shape)
    {
    case WaveformShape::Sine:
        return "sine";
    case WaveformShape::Triangle:
        return "triangle";
    case WaveformShape::SawtoothUp:
        return "sawtooth_up";
    case WaveformShape::SawtoothDown:
        return "sawtooth_down";
    case WaveformShape::Square:
        return "square";
    }
    throw std::invalid_argument{"Unknown waveform shape."};
}

auto shape_from_name(std::string const &name) -> WaveformShape
{
    if (name == "sine")
        return WaveformShape::Sine;
    if (name == "triangle")
        return WaveformShape::Triangle;
    if (name == "sawtooth_up")
        return WaveformShape::SawtoothUp;
    if (name == "sawtooth_down")
        return WaveformShape::SawtoothDown;
    if (name == "square")
        return WaveformShape::Square;
    throw std::invalid_argument{"Unknown waveform shape: " + name};
}

auto operation_name(ModulationOperation operation) -> std::string_view
{
    switch (operation)
    {
    case ModulationOperation::Average:
        return "average";
    case ModulationOperation::Sum:
        return "sum";
    case ModulationOperation::Product:
        return "product";
    case ModulationOperation::AmplitudeModulation:
        return "am";
    case ModulationOperation::RingModulation:
        return "ring";
    case ModulationOperation::FrequencyModulation:
        return "fm";
    case ModulationOperation::PhaseModulation:
        return "pm";
    }
    throw std::invalid_argument{"Unknown modulation operation."};
}

auto operation_from_name(std::string const &name) -> ModulationOperation
{
    if (name == "average")
        return ModulationOperation::Average;
    if (name == "sum")
        return ModulationOperation::Sum;
    if (name == "product")
        return ModulationOperation::Product;
    if (name == "am")
        return ModulationOperation::AmplitudeModulation;
    if (name == "ring")
        return ModulationOperation::RingModulation;
    if (name == "fm")
        return ModulationOperation::FrequencyModulation;
    if (name == "pm")
        return ModulationOperation::PhaseModulation;
    throw std::invalid_argument{"Unknown modulation operation: " + name};
}

auto destination_name(BuiltinModulationDestination destination) -> std::string_view
{
    switch (destination)
    {
    case BuiltinModulationDestination::Pitch:
        return "pitch";
    case BuiltinModulationDestination::Velocity:
        return "velocity";
    case BuiltinModulationDestination::Delay:
        return "delay";
    case BuiltinModulationDestination::Gate:
        return "gate";
    case BuiltinModulationDestination::Weight:
        return "weight";
    }
    throw std::invalid_argument{"Unknown modulation destination."};
}

auto selection_to_json(SelectionPath const &selection) -> nlohmann::json
{
    auto path = nlohmann::json::array();
    for (auto const &step : selection.path)
    {
        path.push_back({
            {"kind", step.kind == SelectionStepKind::Element ? "element" : "cell"},
            {"index", step.index},
        });
    }
    return {{"path", std::move(path)}};
}

auto selection_from_json(nlohmann::json const &json) -> SelectionPath
{
    if (!json.is_object() || !json.contains("path") || !json.at("path").is_array())
    {
        throw std::invalid_argument{"Selection path must be an array."};
    }
    auto selection = SelectionPath{};
    for (auto const &item : json.at("path"))
    {
        if (!item.is_object() || !item.contains("kind") || !item.at("kind").is_string())
        {
            throw std::invalid_argument{"Selection step must contain a kind."};
        }
        auto const kind = item.at("kind").get<std::string>();
        auto step = SelectionStep{.index = require_size(item, "index")};
        if (kind == "element")
            step.kind = SelectionStepKind::Element;
        else if (kind == "cell")
            step.kind = SelectionStepKind::SequenceCell;
        else
            throw std::invalid_argument{"Unknown selection step kind: " + kind};
        selection.path.push_back(step);
    }
    return selection;
}

} // namespace

auto modulation_definition_to_json(ModulationDefinition const &modulation)
    -> nlohmann::json
{
    validate(modulation);
    auto waveforms = nlohmann::json::array();
    for (auto const &waveform : modulation.waveforms)
    {
        waveforms.push_back({
            {"enabled", waveform.enabled},
            {"shape", shape_name(waveform.shape)},
            {"frequency", waveform.frequency},
            {"phase", waveform.phase},
            {"amplitude", waveform.amplitude},
            {"amplitude_offset", waveform.amplitude_offset},
        });
    }
    return {
        {"operation", operation_name(modulation.operation)},
        {"waveforms", std::move(waveforms)},
    };
}

auto modulation_definition_from_json(nlohmann::json const &json) -> ModulationDefinition
{
    if (!json.is_object() || !json.contains("operation") ||
        !json.at("operation").is_string() || !json.contains("waveforms") ||
        !json.at("waveforms").is_array())
    {
        throw std::invalid_argument{"Modulation must contain operation and waveforms."};
    }
    if (json.at("waveforms").size() > MAX_MODULATION_WAVEFORMS)
    {
        throw std::invalid_argument{"Too many modulation waveforms."};
    }
    auto result = ModulationDefinition{
        .operation = operation_from_name(json.at("operation").get<std::string>()),
    };
    result.waveforms.reserve(json.at("waveforms").size());
    for (auto const &item : json.at("waveforms"))
    {
        if (!item.is_object() || !item.contains("enabled") ||
            !item.at("enabled").is_boolean() || !item.contains("shape") ||
            !item.at("shape").is_string())
        {
            throw std::invalid_argument{"Invalid modulation waveform."};
        }
        result.waveforms.push_back({
            .enabled = item.at("enabled").get<bool>(),
            .shape = shape_from_name(item.at("shape").get<std::string>()),
            .frequency = require_float(item, "frequency"),
            .phase = require_float(item, "phase"),
            .amplitude = require_float(item, "amplitude"),
            .amplitude_offset = require_float(item, "amplitude_offset"),
        });
    }
    validate(result);
    return result;
}

auto modulation_destination_to_json(ModulationDestination destination) -> nlohmann::json
{
    if (auto const *builtin = std::get_if<BuiltinModulationDestination>(&destination))
    {
        return {{"id", destination_name(*builtin)}};
    }
    auto const controller =
        std::get<MidiCcModulationDestination>(destination).controller;
    if (controller < 0 || controller > sequence::MAX_MIDI_CONTROLLER_NUMBER)
    {
        throw std::invalid_argument{
            "MIDI CC modulation controller must be in [0, 127]."};
    }
    return {
        {"id", "midi_cc"},
        {"controller", static_cast<unsigned>(controller)},
    };
}

auto modulation_destination_from_json(nlohmann::json const &json)
    -> ModulationDestination
{
    if (!json.is_object() || !json.contains("id") || !json.at("id").is_string())
        throw std::invalid_argument{"Modulation destination must contain an ID."};
    auto const name = json.at("id").get<std::string>();
    if (name == "midi_cc")
    {
        if (!json.contains("controller") ||
            (!json.at("controller").is_number_integer() &&
             !json.at("controller").is_number_unsigned()))
        {
            throw std::invalid_argument{
                "MIDI CC modulation controller must be an integer."};
        }
        auto const controller = json.at("controller").get<std::int64_t>();
        if (controller < 0 || controller > sequence::MAX_MIDI_CONTROLLER_NUMBER)
        {
            throw std::invalid_argument{
                "MIDI CC modulation controller must be in [0, 127]."};
        }
        return MidiCcModulationDestination{
            .controller = static_cast<sequence::MidiControllerNumber>(controller)};
    }
    if (json.contains("controller"))
    {
        throw std::invalid_argument{
            "Built-in modulation destinations do not accept a controller."};
    }
    if (name == "pitch")
        return BuiltinModulationDestination::Pitch;
    if (name == "velocity")
        return BuiltinModulationDestination::Velocity;
    if (name == "delay")
        return BuiltinModulationDestination::Delay;
    if (name == "gate")
        return BuiltinModulationDestination::Gate;
    if (name == "weight")
        return BuiltinModulationDestination::Weight;
    throw std::invalid_argument{"Unknown modulation destination: " + name};
}

auto modulation_output_range_to_json(ModulationOutputRange const &range)
    -> nlohmann::json
{
    return {{"minimum", range.minimum}, {"maximum", range.maximum}};
}

auto modulation_output_range_from_json(nlohmann::json const &json)
    -> ModulationOutputRange
{
    if (!json.is_object())
        throw std::invalid_argument{"Modulation output range must be an object."};
    return {
        .minimum = require_finite_number(json, "minimum"),
        .maximum = require_finite_number(json, "maximum"),
    };
}

auto modulation_target_to_json(ModulationTarget const &target) -> nlohmann::json
{
    return {
        {"cursor",
         {{"row_coordinate", target.cursor.row_coordinate},
          {"column_coordinate", target.cursor.column_coordinate},
          {"sequence_id", target.cursor.sequence_id.has_value()
                              ? nlohmann::json(*target.cursor.sequence_id)
                              : nlohmann::json(nullptr)}}},
        {"selection", selection_to_json(target.selection)},
        {"pattern",
         {{"offset", target.pattern.offset}, {"intervals", target.pattern.intervals}}},
    };
}

auto modulation_target_from_json(nlohmann::json const &json) -> ModulationTarget
{
    if (!json.is_object() || !json.contains("cursor") ||
        !json.at("cursor").is_object() || !json.contains("selection") ||
        !json.contains("pattern") || !json.at("pattern").is_object())
    {
        throw std::invalid_argument{"Invalid modulation target."};
    }
    auto const &cursor = json.at("cursor");
    auto const coordinate = [](nlohmann::json const &object, char const *field) {
        if (!object.contains(field) || (!object.at(field).is_number_integer() &&
                                        !object.at(field).is_number_unsigned()))
            throw std::invalid_argument{"Composition coordinate must be an integer."};
        auto const value = object.at(field).get<std::int64_t>();
        if (value < std::numeric_limits<CompositionCoordinate>::min() ||
            value > std::numeric_limits<CompositionCoordinate>::max())
            throw std::invalid_argument{"Composition coordinate is out of range."};
        return static_cast<CompositionCoordinate>(value);
    };
    auto sequence_id = std::optional<SequenceId>{};
    if (!cursor.contains("sequence_id"))
        throw std::invalid_argument{"Modulation target cursor needs sequence_id."};
    if (!cursor.at("sequence_id").is_null())
        sequence_id = static_cast<SequenceId>(require_size(cursor, "sequence_id"));

    auto const &pattern = json.at("pattern");
    if (!pattern.contains("intervals") || !pattern.at("intervals").is_array())
        throw std::invalid_argument{"Pattern intervals must be an array."};
    auto intervals = std::vector<std::size_t>{};
    intervals.reserve(pattern.at("intervals").size());
    for (auto const &interval : pattern.at("intervals"))
    {
        if (!interval.is_number_unsigned())
            throw std::invalid_argument{"Pattern intervals must be unsigned."};
        intervals.push_back(interval.get<std::size_t>());
    }
    if (intervals.empty() ||
        std::ranges::any_of(intervals, [](auto interval) { return interval == 0; }))
        throw std::invalid_argument{"Pattern intervals must be positive."};

    return {
        .cursor = {.row_coordinate = coordinate(cursor, "row_coordinate"),
                   .column_coordinate = coordinate(cursor, "column_coordinate"),
                   .sequence_id = sequence_id},
        .selection = selection_from_json(json.at("selection")),
        .pattern = {.offset = require_size(pattern, "offset"),
                    .intervals = std::move(intervals)},
    };
}

auto modulation_catalog_to_json() -> nlohmann::json
{
    return {
        {"schema_version", MODULATION_SCHEMA_VERSION},
        {"maximum_waveforms", MAX_MODULATION_WAVEFORMS},
        {"waveform_shapes",
         {"sine", "triangle", "sawtooth_up", "sawtooth_down", "square"}},
        {"waveform_parameters",
         {{"frequency", {{"minimum", 0.0}, {"maximum", MAX_MODULATION_FREQUENCY}}},
          {"phase", {{"minimum", 0.0}, {"maximum", 1.0}}},
          {"amplitude", {{"minimum", -1.0}, {"maximum", 1.0}}},
          {"amplitude_offset", {{"minimum", -1.0}, {"maximum", 1.0}}}}},
        {"operations",
         {{{"id", "average"}, {"minimum_enabled_waveforms", 1}},
          {{"id", "sum"}, {"minimum_enabled_waveforms", 1}},
          {{"id", "product"}, {"minimum_enabled_waveforms", 1}},
          {{"id", "am"}, {"enabled_waveforms", 2}, {"roles", {"carrier", "modulator"}}},
          {{"id", "ring"},
           {"enabled_waveforms", 2},
           {"roles", {"carrier", "modulator"}}},
          {{"id", "fm"}, {"enabled_waveforms", 2}, {"roles", {"carrier", "modulator"}}},
          {{"id", "pm"},
           {"enabled_waveforms", 2},
           {"roles", {"carrier", "modulator"}}}}},
        {"destinations",
         {{{"id", "pitch"},
           {"range", "integer"},
           {"quantization", "nearest"},
           {"parameters", nlohmann::json::array()}},
          {{"id", "velocity"},
           {"range", "unit"},
           {"parameters", nlohmann::json::array()}},
          {{"id", "delay"}, {"range", "unit"}, {"parameters", nlohmann::json::array()}},
          {{"id", "gate"}, {"range", "unit"}, {"parameters", nlohmann::json::array()}},
          {{"id", "weight"},
           {"range", "positive"},
           {"parameters", nlohmann::json::array()}},
          {{"id", "midi_cc"},
           {"range", "unit"},
           {"parameters",
            {{{"id", "controller"},
              {"kind", "integer"},
              {"required", true},
              {"constraints",
               {{{"kind", "range"}, {"minimum", 0}, {"maximum", 127}}}}}}}}}},
        {"normalization", "clamp((raw + 1) / 2, 0, 1)"},
    };
}

} // namespace xen
