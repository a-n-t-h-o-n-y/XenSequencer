#include <xen/project_validation.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <variant>

#include <sequence/sequence.hpp>

#include <xen/scale.hpp>

namespace xen
{
namespace
{

void require_unit_interval(float value, char const *name)
{
    if (!std::isfinite(value) || value < 0.f || value > 1.f)
    {
        throw std::invalid_argument{std::string{name} + " must be in [0, 1]."};
    }
}

void validate_cell(sequence::Cell const &cell)
{
    if (!std::isfinite(cell.weight) || cell.weight <= 0.f)
    {
        throw std::invalid_argument{"Cell weight must be finite and positive."};
    }

    for (auto const &element : cell.elements)
    {
        std::visit(
            [](auto const &typed) {
                using Typed = std::decay_t<decltype(typed)>;
                if constexpr (std::is_same_v<Typed, sequence::Note>)
                {
                    require_unit_interval(typed.velocity, "Note velocity");
                    require_unit_interval(typed.delay, "Note delay");
                    require_unit_interval(typed.gate, "Note gate");
                }
                else
                {
                    for (auto const &child : typed.cells)
                    {
                        validate_cell(child);
                    }
                }
            },
            element);
    }
}

void validate_tuning(sequence::Tuning const &tuning)
{
    if (tuning.intervals.empty())
    {
        throw std::invalid_argument{"Tuning intervals must not be empty."};
    }
    if (!std::isfinite(tuning.octave) || tuning.octave <= 0.f)
    {
        throw std::invalid_argument{"Tuning octave must be finite and positive."};
    }
    if (!std::ranges::equal_to{}(tuning.intervals.front(), 0.f))
    {
        throw std::invalid_argument{"Tuning intervals must begin at zero."};
    }
    for (auto const interval : tuning.intervals)
    {
        if (!std::isfinite(interval) || interval < 0.f || interval >= tuning.octave)
        {
            throw std::invalid_argument{
                "Tuning intervals must be finite and within the octave."};
        }
    }
    if (!std::ranges::is_sorted(tuning.intervals) ||
        std::adjacent_find(tuning.intervals.begin(), tuning.intervals.end()) !=
            tuning.intervals.end())
    {
        throw std::invalid_argument{"Tuning intervals must be strictly ordered."};
    }
}

auto fallback_sequence_name(SequenceId id) -> std::string
{
    return "S" + std::to_string(id);
}

auto sequence_name_key(std::string const &name) -> std::string
{
    auto key = name;
    std::ranges::transform(key, key.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return key;
}

} // namespace

void validate(ProjectState const &project)
{
    auto sequence_ids = std::unordered_set<SequenceId>{};
    auto sequence_names = std::unordered_set<std::string>{};
    for (auto const &entry : project.sequence_bank.sequences)
    {
        if (entry.id == 0)
        {
            throw std::invalid_argument{"Sequence IDs must be nonzero."};
        }
        if (!sequence_ids.insert(entry.id).second)
        {
            throw std::invalid_argument{"Duplicate sequence ID."};
        }
        if (entry.name.has_value() && entry.name->empty())
        {
            throw std::invalid_argument{"Sequence name must not be empty."};
        }
        auto const effective_name =
            entry.name.has_value() ? *entry.name : fallback_sequence_name(entry.id);
        if (!sequence_names.insert(sequence_name_key(effective_name)).second)
        {
            throw std::invalid_argument{"Duplicate sequence name."};
        }
        validate_cell(entry.cell);
    }
    if (project.sequence_bank.next_id == 0)
    {
        throw std::invalid_argument{"Next sequence ID must be nonzero."};
    }
    if (sequence_ids.contains(project.sequence_bank.next_id))
    {
        throw std::invalid_argument{"Next sequence ID is already in use."};
    }

    if (project.composition.loop_region.start_column >
        project.composition.loop_region.end_column)
    {
        throw std::invalid_argument{"Composition loop start must not exceed loop end."};
    }

    auto const validate_column = [](CompositionColumn const &column) {
        auto const &time_signature = column.duration;
        if (time_signature.numerator == 0 || time_signature.denominator == 0)
            throw std::invalid_argument{"Column duration values must be nonzero."};
        auto const whole_notes = static_cast<long double>(time_signature.numerator) /
                                 static_cast<long double>(time_signature.denominator);
        if (whole_notes > 64.0L)
            throw std::invalid_argument{
                "Column duration must not exceed 64 whole notes."};
        auto const &pitch = column.pitch;
        validate_tuning(pitch.tuning.definition);
        if (!std::isfinite(pitch.base_frequency) || pitch.base_frequency <= 0.f)
            throw std::invalid_argument{"Base frequency must be finite and positive."};
        if (pitch.transposition < -127 || pitch.transposition > 127)
            throw std::invalid_argument{"Transposition must be in [-127, 127]."};
        if (pitch.scale.has_value())
        {
            validate_scale(pitch.scale->definition);
            if (pitch.scale->definition.tuning_length !=
                pitch.tuning.definition.intervals.size())
                throw std::invalid_argument{
                    "Active scale tuning length must match the active tuning."};
            if (pitch.scale->source_id.has_value() && pitch.scale->source_id->empty())
                throw std::invalid_argument{
                    "Active scale source ID must not be empty."};
        }
    };

    validate_column(project.composition.default_column);
    for (auto const &[coordinate, column] : project.composition.columns)
    {
        (void)coordinate;
        validate_column(column);
    }
    for (auto const &[coordinate, row] : project.composition.rows)
    {
        (void)coordinate;
        if (row.name.has_value() && row.name->empty())
        {
            throw std::invalid_argument{"Composition row name must not be empty."};
        }
        if (row.channel_id.empty())
        {
            throw std::invalid_argument{
                "Composition row channel ID must not be empty."};
        }
    }
    for (auto const &[position, sequence_id] : project.composition.placements)
    {
        if (!project.composition.rows.contains(position.row_coordinate) ||
            !project.composition.columns.contains(position.column_coordinate))
            throw std::invalid_argument{
                "Composition placement must reference materialized axes."};
        if (!sequence_ids.contains(sequence_id))
            throw std::invalid_argument{
                "Composition references an unknown sequence ID."};
    }
    for (auto const &[coordinate, row] : project.composition.rows)
    {
        (void)row;
        if (!std::ranges::any_of(project.composition.placements,
                                 [coordinate](auto const &entry) {
                                     return entry.first.row_coordinate == coordinate;
                                 }))
            throw std::invalid_argument{
                "Composition rows must contain at least one placement."};
    }
    for (auto const &[coordinate, column] : project.composition.columns)
    {
        (void)column;
        if (!std::ranges::any_of(project.composition.placements,
                                 [coordinate](auto const &entry) {
                                     return entry.first.column_coordinate == coordinate;
                                 }))
            throw std::invalid_argument{
                "Composition columns must contain at least one placement."};
    }
}

void validate_timeline_state(ProjectState const &project)
{
    validate(project);
}

void validate(ContentLibrary const &library)
{
    auto ids = std::unordered_set<std::string>{};
    for (auto const &entry : library.scales)
    {
        if (entry.id.empty())
        {
            throw std::invalid_argument{"Scale library IDs must not be empty."};
        }
        if (!ids.insert(entry.id).second)
        {
            throw std::invalid_argument{"Duplicate scale library ID: " + entry.id};
        }
        validate_scale(entry.definition);
    }
    auto chord_names = std::unordered_set<std::string>{};
    for (auto const &chord : library.chords)
    {
        if (chord.name.empty() || chord.intervals.empty())
        {
            throw std::invalid_argument{
                "Chord names and interval lists must not be empty."};
        }
        if (!chord_names.insert(chord.name).second)
        {
            throw std::invalid_argument{"Duplicate chord name: " + chord.name};
        }
        if (chord.intervals.front() != 0 || !std::ranges::is_sorted(chord.intervals) ||
            std::adjacent_find(chord.intervals.begin(), chord.intervals.end()) !=
                chord.intervals.end())
        {
            throw std::invalid_argument{
                "Chord intervals must begin at zero and be strictly ordered."};
        }
    }
}

void validate(WorkspaceSettings const &workspace)
{
    if (!std::filesystem::is_directory(workspace.content_directory))
    {
        throw std::invalid_argument{"Content directory does not exist: " +
                                    workspace.content_directory.string()};
    }
    if (!std::filesystem::is_directory(workspace.tuning_directory))
    {
        throw std::invalid_argument{"Tuning directory does not exist: " +
                                    workspace.tuning_directory.string()};
    }
}

} // namespace xen
