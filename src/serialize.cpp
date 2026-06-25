#include <xen/serialize.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include <sequence/sequence.hpp>

#include <xen/project_validation.hpp>
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
    };
}

static void from_json(nlohmann::json const &j, Measure &measure)
{
    measure.cell = j.at("cell").get<sequence::Cell>();
}

static void to_json(nlohmann::json &j, MeasureBankEntry const &entry)
{
    j = nlohmann::json{
        {"id", entry.id},
        {"measure", entry.measure},
    };
}

static void from_json(nlohmann::json const &j, MeasureBankEntry &entry)
{
    entry.id = j.at("id").get<MeasureId>();
    entry.measure = j.at("measure").get<Measure>();
}

static void to_json(nlohmann::json &j, MeasureBank const &bank)
{
    j = nlohmann::json{
        {"next_id", bank.next_id},
        {"measures", bank.measures},
    };
}

static void from_json(nlohmann::json const &j, MeasureBank &bank)
{
    bank.next_id = j.at("next_id").get<MeasureId>();
    bank.measures = j.at("measures").get<std::vector<MeasureBankEntry>>();
}

static void to_json(nlohmann::json &j, CompositionColumn const &column)
{
    j = nlohmann::json{{"length", column.length}};
}

static void from_json(nlohmann::json const &j, CompositionColumn &column)
{
    column.length = j.at("length").get<sequence::TimeSignature>();
}

static void to_json(nlohmann::json &j, CompositionRow const &row)
{
    auto cells = nlohmann::json::array();
    for (auto const &cell : row.cells)
    {
        cells.push_back(cell.has_value() ? nlohmann::json(*cell)
                                         : nlohmann::json(nullptr));
    }
    j = nlohmann::json{
        {"output_id", row.output_id},
        {"cells", std::move(cells)},
    };
}

static void from_json(nlohmann::json const &j, CompositionRow &row)
{
    row.output_id = j.at("output_id").get<OutputId>();
    row.cells.clear();
    for (auto const &cell : j.at("cells"))
    {
        row.cells.push_back(cell.is_null()
                                ? std::optional<MeasureId>{}
                                : std::optional<MeasureId>{cell.get<MeasureId>()});
    }
}

static void to_json(nlohmann::json &j, LoopRegion const &region)
{
    j = nlohmann::json::object();
    j["start_column"] = region.start_column;
    j["end_column"] = region.end_column;
}

static void from_json(nlohmann::json const &j, LoopRegion &region)
{
    region.start_column = j.at("start_column").get<std::size_t>();
    region.end_column = j.at("end_column").get<std::size_t>();
}

static void to_json(nlohmann::json &j, Composition const &composition)
{
    j = nlohmann::json::object();
    j["columns"] = composition.columns;
    j["rows"] = composition.rows;
    j["loop_region"] = composition.loop_region;
}

static void from_json(nlohmann::json const &j, Composition &composition)
{
    composition.columns = j.at("columns").get<std::vector<CompositionColumn>>();
    composition.rows = j.at("rows").get<std::vector<CompositionRow>>();
    if (j.contains("loop_region"))
    {
        composition.loop_region = j.at("loop_region").get<LoopRegion>();
    }
    else if (!composition.columns.empty())
    {
        composition.loop_region = LoopRegion{
            .start_column = 0,
            .end_column = composition.columns.size() - 1,
        };
    }
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
namespace
{

constexpr auto PROJECT_SCHEMA_VERSION = 2;
constexpr auto PROCESSOR_STATE_SCHEMA_VERSION = 1;

} // namespace

static void to_json(nlohmann::json &j, TranslateDirection direction)
{
    j = direction == TranslateDirection::Up ? "up" : "down";
}

static void from_json(nlohmann::json const &j, TranslateDirection &direction)
{
    auto const value = j.get<std::string>();
    if (value == "up")
    {
        direction = TranslateDirection::Up;
    }
    else if (value == "down")
    {
        direction = TranslateDirection::Down;
    }
    else
    {
        throw std::invalid_argument{"Unknown translation direction."};
    }
}

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
    auto const intervals = j.at("intervals").get<std::vector<unsigned>>();
    scale.intervals.clear();
    scale.intervals.reserve(intervals.size());
    for (auto const interval : intervals)
    {
        if (interval == 0 || interval > std::numeric_limits<std::uint8_t>::max())
        {
            throw std::invalid_argument{
                "Scale intervals must be in the range [1, 255]."};
        }
        scale.intervals.push_back(static_cast<std::uint8_t>(interval));
    }
    auto const mode = j.at("mode").get<unsigned>();
    if (mode > std::numeric_limits<std::uint8_t>::max())
    {
        throw std::invalid_argument{"Scale mode is out of range."};
    }
    scale.mode = static_cast<std::uint8_t>(mode);
    validate_scale(scale);
}

static void to_json(nlohmann::json &j, ActiveScale const &scale)
{
    j = nlohmann::json{
        {"source_id", scale.source_id},
        {"definition", scale.definition},
    };
}

static void from_json(nlohmann::json const &j, ActiveScale &scale)
{
    scale.source_id = j.at("source_id").get<std::optional<std::string>>();
    scale.definition = j.at("definition").get<Scale>();
}

static void to_json(nlohmann::json &j, NamedTuning const &tuning)
{
    j = nlohmann::json{
        {"name", tuning.name},
        {"definition", tuning.definition},
    };
}

static void from_json(nlohmann::json const &j, NamedTuning &tuning)
{
    tuning.name = j.at("name").get<std::string>();
    tuning.definition = j.at("definition").get<sequence::Tuning>();
}

static void to_json(nlohmann::json &j, PitchSystem const &pitch)
{
    j = nlohmann::json{
        {"tuning", pitch.tuning},
        {"scale", pitch.scale},
        {"transposition", pitch.transposition},
        {"translation_direction", pitch.translation_direction},
        {"base_frequency", pitch.base_frequency},
    };
}

static void from_json(nlohmann::json const &j, PitchSystem &pitch)
{
    pitch.tuning = j.at("tuning").get<NamedTuning>();
    pitch.scale = j.at("scale").get<std::optional<ActiveScale>>();
    pitch.transposition = j.at("transposition").get<int>();
    pitch.translation_direction =
        j.at("translation_direction").get<TranslateDirection>();
    pitch.base_frequency = j.at("base_frequency").get<float>();
}

static void to_json(nlohmann::json &j, ProjectState const &project)
{
    j = nlohmann::json{
        {"measure_bank", project.measure_bank},
        {"composition", project.composition},
        {"pitch", project.pitch},
    };
}

static void from_json(nlohmann::json const &j, ProjectState &project)
{
    project.measure_bank = j.at("measure_bank").get<MeasureBank>();
    project.composition = j.at("composition").get<Composition>();
    project.pitch = j.at("pitch").get<PitchSystem>();
}

static void to_json(nlohmann::json &j, InstanceBinding const &binding)
{
    j = nlohmann::json{
        {"session_id", binding.session_id},
        {"instance_id", binding.instance_id},
        {"output_id", binding.output_id},
    };
}

static void from_json(nlohmann::json const &j, InstanceBinding &binding)
{
    binding.session_id = j.at("session_id").get<SessionId>();
    binding.instance_id = j.at("instance_id").get<InstanceId>();
    binding.output_id = j.at("output_id").get<OutputId>();
    if (binding.session_id.empty())
    {
        throw std::invalid_argument{"Session ID must not be empty."};
    }
    if (binding.instance_id.empty())
    {
        throw std::invalid_argument{"Instance ID must not be empty."};
    }
    if (binding.output_id.empty())
    {
        throw std::invalid_argument{"Output ID must not be empty."};
    }
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

auto serialize_project(ProjectState const &project) -> std::string
{
    validate(project);
    return nlohmann::json{
        {"schema", PROJECT_SCHEMA_VERSION},
        {"project", project},
    }
        .dump();
}

auto deserialize_project(std::string const &json_str) -> ProjectState
{
    auto const json = nlohmann::json::parse(json_str);
    if (json.at("schema").get<int>() != PROJECT_SCHEMA_VERSION)
    {
        throw std::invalid_argument{"Unsupported project schema."};
    }
    auto project = json.at("project").get<ProjectState>();
    validate(project);
    return project;
}

auto serialize_processor_state(InstanceBinding const &binding,
                               ProjectSnapshot const &snapshot) -> std::string
{
    validate(snapshot.project);
    if (binding.session_id.empty())
    {
        throw std::invalid_argument{"Session ID must not be empty."};
    }
    if (binding.instance_id.empty())
    {
        throw std::invalid_argument{"Instance ID must not be empty."};
    }
    if (binding.output_id.empty())
    {
        throw std::invalid_argument{"Output ID must not be empty."};
    }

    return nlohmann::json{
        {"schema", PROCESSOR_STATE_SCHEMA_VERSION},
        {"kind", "xen_processor_state"},
        {"binding", binding},
        {"shared_snapshot",
         {
             {"history_entry_id", snapshot.history_entry_id.value()},
             {"project_revision", snapshot.project_revision.value()},
             {"project", snapshot.project},
         }},
    }
        .dump();
}

auto deserialize_processor_state(std::string const &json_str) -> PersistedProcessorState
{
    auto const json = nlohmann::json::parse(json_str);
    if (json.at("kind").get<std::string>() != "xen_processor_state" ||
        json.at("schema").get<int>() != PROCESSOR_STATE_SCHEMA_VERSION)
    {
        throw std::invalid_argument{"Unsupported processor state schema."};
    }

    auto const &snapshot = json.at("shared_snapshot");
    auto state = PersistedProcessorState{
        .binding = json.at("binding").get<InstanceBinding>(),
        .project = snapshot.at("project").get<ProjectState>(),
        .saved_history_entry_id =
            HistoryEntryId{snapshot.at("history_entry_id").get<std::uint64_t>()},
        .saved_project_revision =
            ProjectRevision{snapshot.at("project_revision").get<std::uint64_t>()},
    };
    validate(state.project);
    return state;
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

auto deserialize_copy_buffer_content(std::string const &json_str) -> CopyBufferContent
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
