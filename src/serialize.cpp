#include <xen/serialize.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
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

static void to_json(nlohmann::json &j, PitchSystem const &pitch);
static void from_json(nlohmann::json const &j, PitchSystem &pitch);

static void to_json(nlohmann::json &j, SequenceBankEntry const &entry)
{
    j = nlohmann::json{
        {"id", entry.id},
        {"cell", entry.cell},
    };
    if (entry.name.has_value())
    {
        j["name"] = *entry.name;
    }
}

static void from_json(nlohmann::json const &j, SequenceBankEntry &entry)
{
    entry.id = j.at("id").get<SequenceId>();
    entry.name = j.contains("name") && !j.at("name").is_null()
                     ? std::optional<std::string>{j.at("name").get<std::string>()}
                     : std::nullopt;
    entry.cell = j.at("cell").get<sequence::Cell>();
}

static void to_json(nlohmann::json &j, SequenceBank const &bank)
{
    j = nlohmann::json{
        {"next_id", bank.next_id},
        {"sequences", bank.sequences},
    };
}

static void from_json(nlohmann::json const &j, SequenceBank &bank)
{
    bank.next_id = j.at("next_id").get<SequenceId>();
    bank.sequences = j.at("sequences").get<std::vector<SequenceBankEntry>>();
}

static void to_json(nlohmann::json &j, CompositionColumn const &column)
{
    j = nlohmann::json{{"duration", column.duration}, {"pitch", column.pitch}};
}

static void from_json(nlohmann::json const &j, CompositionColumn &column)
{
    column.duration = j.at("duration").get<sequence::TimeSignature>();
    column.pitch = j.at("pitch").get<PitchSystem>();
}

static void to_json(nlohmann::json &j, CompositionRow const &row)
{
    j = nlohmann::json{{"channel_id", row.channel_id}};
    if (row.name.has_value())
    {
        j["name"] = *row.name;
    }
}

static void from_json(nlohmann::json const &j, CompositionRow &row)
{
    row.name = j.contains("name") && !j.at("name").is_null()
                   ? std::optional<std::string>{j.at("name").get<std::string>()}
                   : std::nullopt;
    row.channel_id = j.at("channel_id").get<ChannelId>();
}

static void to_json(nlohmann::json &j, LoopRegion const &region)
{
    j = nlohmann::json::object();
    j["start_column"] = region.start_column;
    j["end_column"] = region.end_column;
}

static void from_json(nlohmann::json const &j, LoopRegion &region)
{
    region.start_column = j.at("start_column").get<CompositionCoordinate>();
    region.end_column = j.at("end_column").get<CompositionCoordinate>();
}

static void to_json(nlohmann::json &j, Composition const &composition)
{
    auto columns = nlohmann::json::array();
    for (auto const &[coordinate, column] : composition.columns)
    {
        auto value = nlohmann::json(column);
        value["coordinate"] = coordinate;
        columns.push_back(std::move(value));
    }

    auto rows = nlohmann::json::array();
    for (auto const &[coordinate, row] : composition.rows)
    {
        auto value = nlohmann::json(row);
        value["coordinate"] = coordinate;
        rows.push_back(std::move(value));
    }

    auto placements = nlohmann::json::array();
    for (auto const &[position, sequence_id] : composition.placements)
    {
        placements.push_back({{"row", position.row_coordinate},
                              {"column", position.column_coordinate},
                              {"sequence_id", sequence_id}});
    }

    j = nlohmann::json{{"default_column", composition.default_column},
                       {"columns", std::move(columns)},
                       {"rows", std::move(rows)},
                       {"placements", std::move(placements)},
                       {"loop_region", composition.loop_region}};
}

static void from_json(nlohmann::json const &j, Composition &composition)
{
    composition = {};
    composition.default_column = j.at("default_column").get<CompositionColumn>();
    for (auto const &value : j.at("columns"))
    {
        auto const coordinate = value.at("coordinate").get<CompositionCoordinate>();
        if (!composition.columns.emplace(coordinate, value.get<CompositionColumn>())
                 .second)
            throw std::invalid_argument{"Duplicate composition column coordinate."};
    }
    for (auto const &value : j.at("rows"))
    {
        auto const coordinate = value.at("coordinate").get<CompositionCoordinate>();
        if (!composition.rows.emplace(coordinate, value.get<CompositionRow>()).second)
            throw std::invalid_argument{"Duplicate composition row coordinate."};
    }
    for (auto const &value : j.at("placements"))
    {
        auto const position = CompositionPosition{
            .row_coordinate = value.at("row").get<CompositionCoordinate>(),
            .column_coordinate = value.at("column").get<CompositionCoordinate>(),
        };
        auto const sequence_id = value.at("sequence_id").get<SequenceId>();
        if (!composition.placements.emplace(position, sequence_id).second)
            throw std::invalid_argument{"Duplicate composition placement coordinate."};
    }
    composition.loop_region = j.at("loop_region").get<LoopRegion>();
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

constexpr auto PROJECT_SCHEMA_VERSION = 5;
constexpr auto PROCESSOR_STATE_SCHEMA_VERSION = 4;
constexpr auto CELL_SCHEMA_VERSION = 1;

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
        {"sequence_bank", project.sequence_bank},
        {"composition", project.composition},
    };
}

static void from_json(nlohmann::json const &j, ProjectState &project)
{
    project.sequence_bank = j.at("sequence_bank").get<SequenceBank>();
    project.composition = j.at("composition").get<Composition>();
}

static void to_json(nlohmann::json &j, InstanceBinding const &binding)
{
    j = nlohmann::json{
        {"session_id", binding.session_id},
        {"instance_id", binding.instance_id},
        {"channel_id", binding.channel_id},
    };
}

static void from_json(nlohmann::json const &j, InstanceBinding &binding)
{
    binding.session_id = j.at("session_id").get<SessionId>();
    binding.instance_id = j.at("instance_id").get<InstanceId>();
    binding.channel_id = j.at("channel_id").get<ChannelId>();
    if (binding.session_id.empty())
    {
        throw std::invalid_argument{"Session ID must not be empty."};
    }
    if (binding.instance_id.empty())
    {
        throw std::invalid_argument{"Instance ID must not be empty."};
    }
    if (binding.channel_id.empty())
    {
        throw std::invalid_argument{"Channel ID must not be empty."};
    }
}

auto serialize_cell_file(sequence::Cell const &cell) -> std::string
{
    return nlohmann::json{
        {"schema", CELL_SCHEMA_VERSION}, {"kind", "xen_cell"}, {"cell", cell}}
        .dump();
}

auto deserialize_cell_file(std::string const &json_str) -> sequence::Cell
{
    auto const json = nlohmann::json::parse(json_str);
    if (json.at("kind").get<std::string>() != "xen_cell" ||
        json.at("schema").get<int>() != CELL_SCHEMA_VERSION)
        throw std::invalid_argument{"Unsupported Cell schema."};
    return json.at("cell").get<sequence::Cell>();
}

auto serialize_composition(ProjectState const &project) -> std::string
{
    return serialize_project(project);
}

auto deserialize_composition(std::string const &json_str) -> ProjectState
{
    return deserialize_project(json_str);
}

auto serialize_project(ProjectState const &project) -> std::string
{
    validate(project);
    return nlohmann::json{
        {"schema", PROJECT_SCHEMA_VERSION},
        {"kind", "xen_composition"},
        {"project", project},
    }
        .dump();
}

auto deserialize_project(std::string const &json_str) -> ProjectState
{
    auto const json = nlohmann::json::parse(json_str);
    if (json.at("kind").get<std::string>() != "xen_composition" ||
        json.at("schema").get<int>() != PROJECT_SCHEMA_VERSION)
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
    if (binding.channel_id.empty())
    {
        throw std::invalid_argument{"Channel ID must not be empty."};
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

} // namespace xen
