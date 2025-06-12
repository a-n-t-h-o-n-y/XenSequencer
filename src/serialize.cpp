#include <xen/serialize.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include <sequence/sequence.hpp>

#include <xen/scale.hpp>
#include <xen/state.hpp>

namespace sequence
{

// Serialization -------------------------------------------------------------

static void to_json(nlohmann::json &j, Note const &note)
{
    j = nlohmann::json{{"type", "Note"},
                       {"pitch", note.pitch},
                       {"velocity", note.velocity},
                       {"delay", note.delay},
                       {"gate", note.gate}};
}

static void to_json(nlohmann::json &j, Rest const &)
{
    j = nlohmann::json{{"type", "Rest"}};
}

// Forward declare for Sequence implementation.
static void to_json(nlohmann::json &j, Cell const &cell);

static void to_json(nlohmann::json &j, Sequence const &sequence)
{
    j = nlohmann::json{{"type", "Sequence"}, {"cells", sequence.cells}};
}

static void to_json(nlohmann::json &j, Cell const &cell)
{
    std::visit([&j](auto const &element) { to_json(j, element); }, cell.element);
    j["weight"] = cell.weight;
}

static void to_json(nlohmann::json &j, TimeSignature const &ts)
{
    j = nlohmann::json{
        {"numerator", ts.numerator},
        {"denominator", ts.denominator},
    };
}

static void to_json(nlohmann::json &j, Measure const &measure)
{
    j = nlohmann::json{
        {"cell", measure.cell},
        {"time_signature", measure.time_signature},
    };
}

static void to_json(nlohmann::json &j, Tuning const &tuning)
{
    j = nlohmann::json{
        {"intervals", tuning.intervals},
        {"octave", tuning.octave},
    };
}

// Deserialization -----------------------------------------------------------

static void from_json(nlohmann::json const &j, Note &note)
{
    note.pitch = j.at("pitch").get<int>();
    note.velocity = j.at("velocity").get<float>();
    note.delay = j.at("delay").get<float>();
    note.gate = j.at("gate").get<float>();
}

static void from_json(nlohmann::json const &, Rest &)
{
    // Empty; Rest has no members
}

// Forward declare for Sequence implementation.
static void from_json(nlohmann::json const &j, Cell &cell);

static void from_json(nlohmann::json const &j, Sequence &sequence)
{
    sequence.cells = j.at("cells").get<std::vector<Cell>>();
}

static void from_json(nlohmann::json const &j, Cell &cell)
{
    cell.weight = j.value("weight", 1.f);

    auto const type = j.at("type").get<std::string>();
    if (type == "Note")
    {
        cell.element = j.get<Note>();
    }
    else if (type == "Rest")
    {
        cell.element = j.get<Rest>();
    }
    else if (type == "Sequence")
    {
        cell.element = j.get<Sequence>();
    }
    else
    {
        throw std::invalid_argument("Unknown type for Cell");
    }
}

static void from_json(nlohmann::json const &j, TimeSignature &ts)
{
    ts.numerator = j.at("numerator").get<unsigned>();
    ts.denominator = j.at("denominator").get<unsigned>();
}

static void from_json(nlohmann::json const &j, Measure &measure)
{
    measure.cell = j.at("cell").get<Cell>();
    measure.time_signature = j.at("time_signature").get<TimeSignature>();
}

static void from_json(nlohmann::json const &j, Tuning &tuning)
{
    tuning.intervals = j.at("intervals").get<std::vector<Tuning::Interval_t>>();
    tuning.octave = j.at("octave").get<Tuning::Interval_t>();
}

} // namespace sequence

namespace nlohmann
{

template <typename T>
static void to_json(json &j, std::optional<T> const &opt)
{
    if (opt.has_value())
    {
        j = *opt;
    }
    else
    {
        j = nullptr;
    }
}

template <typename T>
static void from_json(json const &j, std::optional<T> &opt)
{
    if (j.is_null())
    {
        opt = std::nullopt;
    }
    else
    {
        opt = j.get<T>();
    }
}

} // namespace nlohmann

namespace xen
{

static void to_json(nlohmann::json &j, Scale const &scale)
{
    j = nlohmann::json{
        {"name", scale.name},
        {"tuning_length", scale.tuning_length},
        {"intervals", scale.intervals},
        {"mode", scale.mode},
    };
}

static void from_json(nlohmann::json const &j, Scale &scale)
{
    scale.name = j.at("name").get<std::string>();
    scale.tuning_length = j.at("tuning_length").get<std::size_t>();
    scale.intervals = j.at("intervals").get<std::vector<std::uint8_t>>();
    scale.mode = j.at("mode").get<std::uint8_t>();
}

static void to_json(nlohmann::json &j, SequencerState const &state)
{
    j = nlohmann::json{
        {"sequence_bank", state.sequence_bank},
        {"sequence_names", state.sequence_names},
        {"tuning", state.tuning},
        {"tuning_name", state.tuning_name},
        {"scale", state.scale},
        {"key", state.key},
        {"scale_translate_direction", state.scale_translate_direction},
        {"base_frequency", state.base_frequency},
    };
}

static void from_json(nlohmann::json const &j, SequencerState &state)
{
    state.sequence_bank = j.at("sequence_bank").get<SequenceBank>();
    state.sequence_names = j.at("sequence_names").get<std::array<std::string, 16>>();
    state.tuning = j.at("tuning").get<sequence::Tuning>();
    state.tuning_name = j.at("tuning_name").get<std::string>();
    state.scale = j.at("scale").get<std::optional<Scale>>();
    state.key = j.at("key").get<int>();
    state.scale_translate_direction =
        j.at("scale_translate_direction").get<TranslateDirection>();
    state.base_frequency = j.at("base_frequency").get<float>();
}

// -------------------------------------------------------------------------------------

auto serialize_cell(sequence::Cell const &c) -> std::string
{
    auto json = nlohmann::json{};
    to_json(json, c);
    return json.dump();
}

auto deserialize_cell(std::string const &json_str) -> sequence::Cell
{
    auto const json = nlohmann::json::parse(json_str);
    auto cell = sequence::Cell{};
    from_json(json, cell);
    return cell;
}

auto serialize_measure(sequence::Measure const &m) -> std::string
{
    auto json = nlohmann::json{};
    to_json(json, m);
    return json.dump();
}

auto deserialize_measure(std::string const &json_str) -> sequence::Measure
{
    auto const json = nlohmann::json::parse(json_str);
    auto measure = sequence::Measure{};
    from_json(json, measure);
    return measure;
}

auto serialize_sequence_bank(SequenceBank const &bank,
                             std::array<std::string, 16> const &sequence_names)
    -> std::string
{
    auto json = nlohmann::json{
        {"sequence_bank", bank},
        {"sequence_names", sequence_names},
    };
    return json.dump();
}

auto deserialize_sequence_bank(std::string const &json_str)
    -> std::pair<SequenceBank, std::array<std::string, 16>>
{
    auto const json = nlohmann::json::parse(json_str);
    auto bank = json.at("sequence_bank").get<SequenceBank>();
    auto const sequence_names =
        json.at("sequence_names").get<std::array<std::string, 16>>();
    return {bank, sequence_names};
}

auto serialize_plugin(SequencerState const &state) -> std::string
{
    auto json = nlohmann::json{};
    to_json(json, state);
    return json.dump();
}

auto deserialize_plugin(std::string const &json_str) -> SequencerState
{
    return nlohmann::json::parse(json_str).get<SequencerState>();
}

} // namespace xen