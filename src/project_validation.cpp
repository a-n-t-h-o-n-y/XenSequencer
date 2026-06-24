#include <xen/project_validation.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
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

} // namespace

void validate(ProjectState const &project)
{
    validate_cell(project.measure.cell);

    auto const &time_signature = project.measure.time_signature;
    if (time_signature.numerator == 0 || time_signature.denominator == 0)
    {
        throw std::invalid_argument{"Time signature values must be nonzero."};
    }
    auto const whole_notes = static_cast<long double>(time_signature.numerator) /
                             static_cast<long double>(time_signature.denominator);
    if (whole_notes > 64.0L)
    {
        throw std::invalid_argument{"Measure duration must not exceed 64 whole notes."};
    }

    auto const &pitch = project.pitch;
    validate_tuning(pitch.tuning.definition);
    if (!std::isfinite(pitch.base_frequency) || pitch.base_frequency <= 0.f)
    {
        throw std::invalid_argument{"Base frequency must be finite and positive."};
    }
    if (pitch.transposition < -127 || pitch.transposition > 127)
    {
        throw std::invalid_argument{"Transposition must be in [-127, 127]."};
    }
    if (pitch.scale.has_value())
    {
        validate_scale(pitch.scale->definition);
        if (pitch.scale->definition.tuning_length !=
            pitch.tuning.definition.intervals.size())
        {
            throw std::invalid_argument{
                "Active scale tuning length must match the active tuning."};
        }
        if (pitch.scale->source_id.has_value() && pitch.scale->source_id->empty())
        {
            throw std::invalid_argument{"Active scale source ID must not be empty."};
        }
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
    if (!std::filesystem::is_directory(workspace.sequence_directory))
    {
        throw std::invalid_argument{"Sequence directory does not exist: " +
                                    workspace.sequence_directory.string()};
    }
    if (!std::filesystem::is_directory(workspace.tuning_directory))
    {
        throw std::invalid_argument{"Tuning directory does not exist: " +
                                    workspace.tuning_directory.string()};
    }
}

} // namespace xen
