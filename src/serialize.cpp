#include <xen/serialize.hpp>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include <sequence/sequence.hpp>

#include <xen/scale.hpp>
#include <xen/state.hpp>

namespace sequence
{

static void to_json(nlohmann::json &j, Note const &note)
{
    j = nlohmann::json{{"type", "Note"},
                       {"pitch", note.pitch},
                       {"velocity", note.velocity},
                       {"delay", note.delay},
                       {"gate", note.gate}};
}

static void to_json(nlohmann::json &j, MusicElement const &element);
static void from_json(nlohmann::json const &j, MusicElement &element);

static void to_json(nlohmann::json &j, Cell const &cell);
static void from_json(nlohmann::json const &j, Cell &cell);

static void to_json(nlohmann::json &j, Sequence const &sequence)
{
    j = nlohmann::json{{"type", "Sequence"}, {"cells", sequence.cells}};
}

static void to_json(nlohmann::json &j, MusicElement const &element)
{
    std::visit([&j](auto const &typed) { to_json(j, typed); }, element);
}

static void to_json(nlohmann::json &j, Cell const &cell)
{
    j = nlohmann::json{
        {"weight", cell.weight},
        {"elements", cell.elements},
    };
}

static void to_json(nlohmann::json &j, TimeSignature const &ts)
{
    j = nlohmann::json{
        {"numerator", ts.numerator},
        {"denominator", ts.denominator},
    };
}

static void to_json(nlohmann::json &j, Tuning const &tuning)
{
    j = nlohmann::json{
        {"intervals", tuning.intervals},
        {"octave", tuning.octave},
    };
}

static void from_json(nlohmann::json const &j, Note &note)
{
    note.pitch = j.at("pitch").get<int>();
    note.velocity = j.at("velocity").get<float>();
    note.delay = j.at("delay").get<float>();
    note.gate = j.at("gate").get<float>();
}

static void from_json(nlohmann::json const &j, Sequence &sequence)
{
    sequence.cells = j.at("cells").get<std::vector<Cell>>();
}

static void from_json(nlohmann::json const &j, MusicElement &element)
{
    auto const type = j.at("type").get<std::string>();
    if (type == "Note")
    {
        element = j.get<Note>();
    }
    else if (type == "Sequence")
    {
        element = j.get<Sequence>();
    }
    else if (type == "Rest")
    {
        throw std::invalid_argument("Legacy rest data is unsupported.");
    }
    else
    {
        throw std::invalid_argument("Unknown type for MusicElement.");
    }
}

static void from_json(nlohmann::json const &j, Cell &cell)
{
    if (!j.contains("elements"))
    {
        throw std::invalid_argument("Legacy cell format is unsupported.");
    }

    cell.weight = j.value("weight", 1.f);
    cell.elements = j.at("elements").get<std::vector<MusicElement>>();
}

static void from_json(nlohmann::json const &j, TimeSignature &ts)
{
    ts.numerator = j.at("numerator").get<unsigned>();
    ts.denominator = j.at("denominator").get<unsigned>();
}

static void from_json(nlohmann::json const &j, Tuning &tuning)
{
    tuning.intervals = j.at("intervals").get<std::vector<Tuning::Interval_t>>();
    tuning.octave = j.at("octave").get<Tuning::Interval_t>();
}

} // namespace sequence

namespace xen
{

static void to_json(nlohmann::json &j, Measure const &measure)
{
    j = nlohmann::json{
        {"cell", measure.cell},
        {"time_signature", measure.time_signature},
    };
}

static void from_json(nlohmann::json const &j, Measure &measure)
{
    measure.cell = j.at("cell").get<sequence::Cell>();
    measure.time_signature = j.at("time_signature").get<sequence::TimeSignature>();
}

} // namespace xen

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

static void to_json(nlohmann::json &j, EngineState const &state)
{
    j = nlohmann::json{
        {"measure", state.measure},
        {"tuning", state.tuning},
        {"tuning_name", state.tuning_name},
        {"scale", state.scale},
        {"key", state.key},
        {"scale_translate_direction", state.scale_translate_direction},
        {"base_frequency", state.base_frequency},
    };
}

static void from_json(nlohmann::json const &j, EngineState &state)
{
    if (j.contains("sequence_bank") || j.contains("sequence_names"))
    {
        throw std::invalid_argument("Legacy sequence bank plugin state is unsupported.");
    }

    state.measure = j.at("measure").get<Measure>();
    state.tuning = j.at("tuning").get<sequence::Tuning>();
    state.tuning_name = j.at("tuning_name").get<std::string>();
    state.scale = j.at("scale").get<std::optional<Scale>>();
    state.key = j.at("key").get<int>();
    state.scale_translate_direction =
        j.at("scale_translate_direction").get<TranslateDirection>();
    state.base_frequency = j.at("base_frequency").get<float>();
}

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

auto serialize_measure(Measure const &m) -> std::string
{
    auto json = nlohmann::json{};
    to_json(json, m);
    return json.dump();
}

auto deserialize_measure(std::string const &json_str) -> Measure
{
    auto const json = nlohmann::json::parse(json_str);
    auto measure = Measure{};
    from_json(json, measure);
    return measure;
}

auto serialize_plugin(EngineState const &state) -> std::string
{
    auto json = nlohmann::json{};
    to_json(json, state);
    return json.dump();
}

auto deserialize_plugin(std::string const &json_str) -> EngineState
{
    return nlohmann::json::parse(json_str).get<EngineState>();
}

auto serialize_copy_buffer_content(CopyBufferContent const &content) -> std::string
{
    auto json = nlohmann::json{};
    std::visit(
        [&json](auto const &typed) {
            using Typed = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<Typed, sequence::Cell>)
            {
                json = nlohmann::json{
                    {"kind", "Cell"},
                    {"content", typed},
                };
            }
            else
            {
                json = nlohmann::json{
                    {"kind", "MusicElement"},
                    {"content", typed},
                };
            }
        },
        content);
    return json.dump();
}

auto deserialize_copy_buffer_content(std::string const &json_str)
    -> CopyBufferContent
{
    auto const json = nlohmann::json::parse(json_str);
    auto const kind = json.at("kind").get<std::string>();

    if (kind == "Cell")
    {
        return json.at("content").get<sequence::Cell>();
    }

    if (kind == "MusicElement")
    {
        return json.at("content").get<sequence::MusicElement>();
    }

    throw std::invalid_argument("Unknown copy buffer content kind.");
}

} // namespace xen
