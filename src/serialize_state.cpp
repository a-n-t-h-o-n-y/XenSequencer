#include <xen/serialize_state.hpp>

#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>
#include <sequence/sequence.hpp>

#include <xen/state.hpp>

namespace sequence
{

// Serialization -------------------------------------------------------------

static auto to_json(nlohmann::json &j, Note const &note) -> void
{
    j = nlohmann::json{{"type", "Note"},
                       {"interval", note.interval},
                       {"velocity", note.velocity},
                       {"delay", note.delay},
                       {"gate", note.gate}};
}

static auto to_json(nlohmann::json &j, Rest const &) -> void
{
    j = nlohmann::json{{"type", "Rest"}};
}

// Forward declare for Sequence implementation.
static auto to_json(nlohmann::json &j, Cell const &cell) -> void;

static auto to_json(nlohmann::json &j, Sequence const &sequence) -> void
{
    j = nlohmann::json{{"type", "Sequence"}, {"cells", sequence.cells}};
}

static auto to_json(nlohmann::json &j, Cell const &cell) -> void
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
    note.interval = j.at("interval").get<int>();
    note.velocity = j.at("velocity").get<float>();
    note.delay = j.at("delay").get<float>();
    note.gate = j.at("gate").get<float>();
}

static void from_json(nlohmann::json const &, Rest &)
{
    // Empty since Rest has no members
}

// Forward declare for Sequence implementation.
static auto from_json(nlohmann::json const &j, Cell &cell) -> void;

static void from_json(nlohmann::json const &j, Sequence &sequence)
{
    sequence.cells = j.at("cells").get<std::vector<Cell>>();
}

static auto from_json(nlohmann::json const &j, Cell &cell) -> void
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

auto serialize(State const &state) -> std::string
{
    auto const json = nlohmann::json{
        {"phrase", state.phrase},
        {"tuning", state.tuning},
        {"base_frequency", state.base_frequency},
    };

    return json.dump();
}

auto deserialize(std::string const &json_str) -> State
{
    auto const json = nlohmann::json::parse(json_str);

    auto state = State{
        json.at("phrase").get<sequence::Phrase>(),
        json.at("tuning").get<sequence::Tuning>(),
        json.at("base_frequency").get<float>(),
    };

    return state;
}

} // namespace xen