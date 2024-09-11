#include <xen/serialize.hpp>

#include <array>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include <sequence/sequence.hpp>

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
    std::visit([&j](auto const &variant_item) { to_json(j, variant_item); }, cell);
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
    auto const type = j.at("type").get<std::string>();
    if (type == "Note")
    {
        cell = j.get<Note>();
    }
    else if (type == "Rest")
    {
        cell = j.get<Rest>();
    }
    else if (type == "Sequence")
    {
        cell = j.get<Sequence>();
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

namespace xen
{

static void to_json(nlohmann::json &j, SequencerState const &state)
{
    j = nlohmann::json{
        {"sequence_bank", state.sequence_bank},
        {"measure_names", state.measure_names},
        {"tuning", state.tuning},
        {"tuning_name", state.tuning_name},
        {"base_frequency", state.base_frequency},
    };
}

static void from_json(nlohmann::json const &j, SequencerState &state)
{
    state.sequence_bank = j.at("sequence_bank").get<SequenceBank>();
    state.measure_names = j.at("measure_names").get<std::array<std::string, 16>>();
    state.tuning = j.at("tuning").get<sequence::Tuning>();
    state.tuning_name = j.at("tuning_name").get<std::string>();
    state.base_frequency = j.at("base_frequency").get<float>();
}

// -------------------------------------------------------------------------------------

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

auto serialize_plugin(SequencerState const &state,
                      std::string const &display_name) -> std::string
{
    auto const json = nlohmann::json{
        {"state", state},
        {"display_name", display_name},
    };

    return json.dump();
}

auto deserialize_plugin(std::string const &json_str)
    -> std::pair<SequencerState, std::string>
{
    auto const json = nlohmann::json::parse(json_str);

    return {
        json.at("state").get<SequencerState>(),
        json.at("display_name").get<std::string>(),
    };
}

} // namespace xen