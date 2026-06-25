#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <juce_core/juce_core.h>

#include <sequence/tuning.hpp>

#include <xen/chord.hpp>
#include <xen/clock.hpp>
#include <xen/command.hpp>
#include <xen/composition.hpp>
#include <xen/measure.hpp>
#include <xen/scale.hpp>
#include <xen/timeline.hpp>

namespace xen
{

using SampleIndex = std::uint64_t;

using SampleCount = std::uint64_t;

struct NamedTuning
{
    std::string name{"12-TET"};
    sequence::Tuning definition{
        .intervals = {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100},
        .octave = 1200,
        .description = "",
    };

    auto operator==(NamedTuning const &) const -> bool = default;
};

struct ActiveScale
{
    std::optional<std::string> source_id{};
    Scale definition{};

    auto operator==(ActiveScale const &) const -> bool = default;
};

struct PitchSystem
{
    NamedTuning tuning{};
    std::optional<ActiveScale> scale{}; // null is chromatic
    int transposition{0};
    TranslateDirection translation_direction{TranslateDirection::Up};
    float base_frequency{440.f};

    auto operator==(PitchSystem const &) const -> bool = default;
};

struct ProjectState
{
    MeasureBank measure_bank{make_default_measure_bank()};
    Composition composition{make_default_composition()};
    PitchSystem pitch{};

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif
    auto operator==(ProjectState const &) const -> bool = default;
    auto operator!=(ProjectState const &) const -> bool = default;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
};

[[nodiscard]] inline auto default_measure(ProjectState &project) -> Measure &
{
    return default_arranged_measure(project.measure_bank, project.composition);
}

[[nodiscard]] inline auto default_measure(ProjectState const &project)
    -> Measure const &
{
    return default_arranged_measure(project.measure_bank, project.composition);
}

[[nodiscard]] inline auto default_measure_length(ProjectState &project)
    -> sequence::TimeSignature &
{
    return default_column_length(project.composition);
}

[[nodiscard]] inline auto default_measure_length(ProjectState const &project)
    -> sequence::TimeSignature const &
{
    return default_column_length(project.composition);
}

void validate_timeline_state(ProjectState const &project);

enum class SelectionStepKind
{
    Element,
    SequenceCell,
};

struct SelectionStep
{
    SelectionStepKind kind{SelectionStepKind::Element};
    std::size_t index{};

    auto operator==(SelectionStep const &) const -> bool = default;
    auto operator!=(SelectionStep const &) const -> bool = default;
};

/**
 * The state of the current selection in the sequencer.
 */
struct SelectionPath
{
    /// Explicit alternating path through Cell elements and Sequence child cells.
    std::vector<SelectionStep> path{};

    auto operator==(SelectionPath const &other) const -> bool = default;
    auto operator!=(SelectionPath const &other) const -> bool = default;
};

enum class TransformKind : std::uint8_t
{
    Arpeggio,
    Chord,
};

using TargetSnapshot = std::variant<sequence::Cell, sequence::MusicElement>;

struct TransformCycleSession
{
    TransformKind kind{TransformKind::Chord};
    SelectionPath target{};
    TargetSnapshot baseline{sequence::Cell{}};
    ProjectRevision project_revision{};
    HistoryEntryId history_entry_id{};
    LibraryRevision library_revision{};
    std::string previous_chord_name{};
    int previous_inversion{-1};
    bool committed{false};
};

struct CommandSessionState
{
    std::optional<TransformCycleSession> transform_cycle{};
    std::vector<CommandInvocation> repeat_chain{};
};

using XenTimeline = Timeline<ProjectState>;

struct WorkspaceSettings
{
    std::filesystem::path sequence_directory{};
    std::filesystem::path tuning_directory{};

    auto operator==(WorkspaceSettings const &) const -> bool = default;
};

struct ContentLibrary
{
    std::vector<LibraryScale> scales{};
    std::vector<Chord> chords{};

    auto operator==(ContentLibrary const &) const -> bool = default;
};

struct PluginState
{
    WorkspaceSettings workspace{};
    ContentLibrary library{};
    LibraryRevision library_revision{detail::allocate_library_revision()};
    CommandSessionState command_session{};
    XenTimeline timeline;
};

struct ProjectSnapshot
{
    ProjectState project{};
    HistoryEntryId history_entry_id{};
    ProjectRevision project_revision{};
};

struct LibrarySnapshot
{
    ContentLibrary library{};
    WorkspaceSettings workspace{};
    LibraryRevision library_revision{};
};

struct AudioProjectSnapshot
{
    ProjectState project{};

    auto operator==(AudioProjectSnapshot const &) const -> bool = default;
};

/**
 * The state of the DAW.
 */
struct DAWState
{
    float bpm = 0.f;
    std::uint32_t sample_rate = 0;
    bool is_playing = false;
};

struct AudioThreadStateForGUI
{
    DAWState daw;
    double loop_phase{0.0};
    bool transport_active{false};
};

} // namespace xen
