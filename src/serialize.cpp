#include <xen/serialize.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
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

    cell.weight = j.at("weight").get<float>();
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

constexpr auto PROJECT_SCHEMA_VERSION = 1;
constexpr auto PROCESSOR_STATE_SCHEMA_VERSION = 5;
constexpr auto CELL_SCHEMA_VERSION = 1;
constexpr auto RECOVERY_SCHEMA_VERSION = 1;
// A nested Cell adds several JSON container levels per musical nesting level.
constexpr auto MAX_JSON_DEPTH = std::size_t{320};
constexpr auto MAX_JSON_EVENTS = std::size_t{8'000'000};
constexpr auto MAX_PROJECT_BYTES = std::size_t{64 * 1'024 * 1'024};
constexpr auto MAX_CELL_BYTES = std::size_t{16 * 1'024 * 1'024};
constexpr auto MAX_PERSISTED_TEXT_BYTES = MAX_PERSISTED_STATE_BYTES;

auto parse_bounded(std::string const &text, std::size_t maximum_bytes) -> nlohmann::json
{
    if (text.size() > maximum_bytes)
    {
        throw std::invalid_argument{"Serialized state exceeds the permitted size."};
    }
    auto events = std::size_t{};
    return nlohmann::json::parse(
        text, [&events](int depth, nlohmann::json::parse_event_t, nlohmann::json &) {
            ++events;
            if (depth < 0 || static_cast<std::size_t>(depth) > MAX_JSON_DEPTH ||
                events > MAX_JSON_EVENTS)
            {
                throw std::invalid_argument{"Serialized state is too complex."};
            }
            return true;
        });
}

auto valid_digest(std::string const &value) -> bool
{
    if (!value.starts_with("sha256:") || value.size() != 71)
    {
        return false;
    }
    return std::ranges::all_of(value.begin() + 7, value.end(),
                               [](unsigned char ch) { return std::isxdigit(ch) != 0; });
}

void validate_persisted_revision(std::uint64_t value, char const *name)
{
    if (value == 0 || value == std::numeric_limits<std::uint64_t>::max())
    {
        throw std::invalid_argument{std::string{name} + " is out of range."};
    }
}

void validate_document_state(ProjectDocumentState const &document)
{
    if (document.relative_path.has_value() && document.relative_path->empty())
    {
        throw std::invalid_argument{"Persisted project path must not be empty."};
    }
    if (document.relative_path.has_value() &&
        document.relative_path->size() > MAX_PERSISTED_STRING_BYTES)
    {
        throw std::invalid_argument{"Persisted project path is too long."};
    }
    if (document.relative_path.has_value() != document.file_revision.has_value())
    {
        throw std::invalid_argument{
            "Persisted project path and file revision must appear together."};
    }
    if (!document.dirty && !document.saved_project_digest.has_value())
    {
        throw std::invalid_argument{
            "A clean persisted document requires a saved project digest."};
    }
    if ((document.file_revision.has_value() &&
         !valid_digest(*document.file_revision)) ||
        (document.saved_project_digest.has_value() &&
         !valid_digest(*document.saved_project_digest)))
    {
        throw std::invalid_argument{"Persisted project digest is malformed."};
    }
}

void validate_binding(InstanceBinding const &binding)
{
    if (binding.session_id.empty() || binding.instance_id.empty() ||
        binding.channel_id.empty())
    {
        throw std::invalid_argument{"Persisted binding fields must not be empty."};
    }
    if (binding.session_id.size() > MAX_PERSISTED_STRING_BYTES ||
        binding.instance_id.size() > MAX_PERSISTED_STRING_BYTES ||
        binding.channel_id.size() > MAX_PERSISTED_STRING_BYTES)
    {
        throw std::invalid_argument{"Persisted binding field is too long."};
    }
}

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

static void to_json(nlohmann::json &j, ProjectDocumentState const &document)
{
    j = nlohmann::json{
        {"relative_path", document.relative_path},
        {"file_revision", document.file_revision},
        {"saved_project_digest", document.saved_project_digest},
        {"dirty", document.dirty},
    };
}

static void from_json(nlohmann::json const &j, ProjectDocumentState &document)
{
    document.relative_path = j.at("relative_path").get<std::optional<std::string>>();
    document.file_revision = j.at("file_revision").get<std::optional<std::string>>();
    document.saved_project_digest =
        j.at("saved_project_digest").get<std::optional<std::string>>();
    document.dirty = j.at("dirty").get<bool>();
    validate_document_state(document);
}

auto serialize_cell_file(sequence::Cell const &cell) -> std::string
{
    validate_cell_file(cell);
    auto const text = nlohmann::json{
        {"schema", CELL_SCHEMA_VERSION},
        {"kind", "xen_cell"},
        {"cell", cell}}.dump();
    if (text.size() > MAX_CELL_BYTES)
    {
        throw std::length_error{"Cell file exceeds the permitted size."};
    }
    return text;
}

auto deserialize_cell_file(std::string const &json_str) -> sequence::Cell
{
    auto const json = parse_bounded(json_str, MAX_CELL_BYTES);
    if (json.at("kind").get<std::string>() != "xen_cell" ||
        json.at("schema").get<int>() != CELL_SCHEMA_VERSION)
        throw std::invalid_argument{"Unsupported Cell schema."};
    auto cell = json.at("cell").get<sequence::Cell>();
    validate_cell_file(cell);
    return cell;
}

auto serialize_project(ProjectState const &project) -> std::string
{
    validate(project);
    auto const text =
        nlohmann::json{
            {"schema", PROJECT_SCHEMA_VERSION},
            {"kind", "xen_project"},
            {"project", project},
        }
            .dump();
    if (text.size() > MAX_PROJECT_BYTES)
    {
        throw std::length_error{"Project file exceeds the permitted size."};
    }
    return text;
}

auto deserialize_project(std::string const &json_str) -> ProjectState
{
    auto const json = parse_bounded(json_str, MAX_PROJECT_BYTES);
    if (json.at("kind").get<std::string>() != "xen_project" ||
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
    validate_persisted_revision(snapshot.project_revision.value(),
                                "Saved project revision");
    validate_persisted_revision(snapshot.state_revision.value(),
                                "Saved state revision");
    validate_document_state(snapshot.document);
    validate_binding(binding);

    auto const text =
        nlohmann::json{
            {"schema", PROCESSOR_STATE_SCHEMA_VERSION},
            {"kind", "xen_processor_state"},
            {"binding", binding},
            {"shared_snapshot",
             {
                 {"project_revision", snapshot.project_revision.value()},
                 {"state_revision", snapshot.state_revision.value()},
                 {"document", snapshot.document},
                 {"project", snapshot.project},
             }},
        }
            .dump();
    if (text.size() > MAX_PERSISTED_TEXT_BYTES)
    {
        throw std::length_error{"Processor state exceeds the permitted size."};
    }
    return text;
}

auto deserialize_processor_state(std::string const &json_str) -> PersistedProcessorState
{
    auto const json = parse_bounded(json_str, MAX_PERSISTED_TEXT_BYTES);
    if (json.at("kind").get<std::string>() != "xen_processor_state" ||
        json.at("schema").get<int>() != PROCESSOR_STATE_SCHEMA_VERSION)
    {
        throw std::invalid_argument{"Unsupported processor state schema."};
    }

    auto const &snapshot = json.at("shared_snapshot");
    auto state = PersistedProcessorState{
        .binding = json.at("binding").get<InstanceBinding>(),
        .project = snapshot.at("project").get<ProjectState>(),
        .saved_project_revision =
            ProjectRevision{snapshot.at("project_revision").get<std::uint64_t>()},
        .saved_state_revision =
            StateRevision{snapshot.at("state_revision").get<std::uint64_t>()},
        .document = snapshot.at("document").get<ProjectDocumentState>(),
    };
    validate_persisted_processor_state(state);
    return state;
}

void validate_persisted_processor_state(PersistedProcessorState const &state)
{
    validate_binding(state.binding);
    validate_persisted_revision(state.saved_project_revision.value(),
                                "Saved project revision");
    validate_persisted_revision(state.saved_state_revision.value(),
                                "Saved state revision");
    validate_document_state(state.document);
    validate(state.project);
}

auto serialize_recovery_state(PersistedRecoveryState const &state) -> std::string
{
    if (state.session_id.empty())
    {
        throw std::invalid_argument{"Recovery session ID must not be empty."};
    }
    if (state.session_id.size() > MAX_PERSISTED_STRING_BYTES)
    {
        throw std::invalid_argument{"Recovery session ID is too long."};
    }
    validate_persisted_revision(state.project_revision.value(),
                                "Recovery project revision");
    validate_persisted_revision(state.state_revision.value(),
                                "Recovery state revision");
    validate_document_state(state.document);
    validate(state.project);
    auto const text =
        nlohmann::json{
            {"schema", RECOVERY_SCHEMA_VERSION},
            {"kind", "xen_recovery"},
            {"session_id", state.session_id},
            {"project_revision", state.project_revision.value()},
            {"state_revision", state.state_revision.value()},
            {"saved_at_unix_ms", state.saved_at_unix_ms},
            {"document", state.document},
            {"project", state.project},
        }
            .dump();
    if (text.size() > MAX_PERSISTED_TEXT_BYTES)
    {
        throw std::length_error{"Recovery state exceeds the permitted size."};
    }
    return text;
}

auto deserialize_recovery_state(std::string const &json_str) -> PersistedRecoveryState
{
    auto const json = parse_bounded(json_str, MAX_PERSISTED_TEXT_BYTES);
    if (json.at("kind").get<std::string>() != "xen_recovery" ||
        json.at("schema").get<int>() != RECOVERY_SCHEMA_VERSION)
    {
        throw std::invalid_argument{"Unsupported recovery schema."};
    }
    auto state = PersistedRecoveryState{
        .session_id = json.at("session_id").get<std::string>(),
        .project = json.at("project").get<ProjectState>(),
        .project_revision =
            ProjectRevision{json.at("project_revision").get<std::uint64_t>()},
        .state_revision = StateRevision{json.at("state_revision").get<std::uint64_t>()},
        .document = json.at("document").get<ProjectDocumentState>(),
        .saved_at_unix_ms = json.at("saved_at_unix_ms").get<std::uint64_t>(),
    };
    if (state.session_id.empty())
    {
        throw std::invalid_argument{"Recovery session ID must not be empty."};
    }
    if (state.session_id.size() > MAX_PERSISTED_STRING_BYTES)
    {
        throw std::invalid_argument{"Recovery session ID is too long."};
    }
    validate_persisted_revision(state.project_revision.value(),
                                "Recovery project revision");
    validate_persisted_revision(state.state_revision.value(),
                                "Recovery state revision");
    validate(state.project);
    return state;
}

} // namespace xen
