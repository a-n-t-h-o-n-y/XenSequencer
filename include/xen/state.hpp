#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <juce_core/juce_core.h>

#include <xen/chord.hpp>
#include <xen/command.hpp>
#include <xen/composition.hpp>
#include <xen/copy_paste.hpp>
#include <xen/pitch_system.hpp>
#include <xen/scale.hpp>
#include <xen/timeline.hpp>

namespace xen
{

inline constexpr auto MAX_PERSISTED_STRING_BYTES = std::size_t{4'096};

using SessionId = std::string;

using InstanceId = std::string;

using PreviewId = std::string;

using MidiCcLabels = std::map<sequence::MidiControllerNumber, std::string>;

struct ProjectState
{
    SequenceBank sequence_bank{make_default_sequence_bank()};
    Composition composition{make_default_composition()};
    MidiCcLabels midi_cc_labels{};

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif
    [[nodiscard]] auto operator==(ProjectState const &other) const -> bool
    {
        return sequence_bank == other.sequence_bank &&
               composition == other.composition &&
               midi_cc_labels == other.midi_cc_labels;
    }
    [[nodiscard]] auto operator!=(ProjectState const &other) const -> bool
    {
        return !(*this == other);
    }
#ifdef __clang__
#pragma clang diagnostic pop
#endif
};

[[nodiscard]] inline auto selected_sequence(ProjectState &project,
                                            CompositionCursor const &cursor)
    -> sequence::Cell &
{
    return arranged_sequence(project.sequence_bank, project.composition, cursor);
}

[[nodiscard]] inline auto selected_sequence(ProjectState const &project,
                                            CompositionCursor const &cursor)
    -> sequence::Cell const &
{
    return arranged_sequence(project.sequence_bank, project.composition, cursor);
}

[[nodiscard]] inline auto selected_column(ProjectState &project,
                                          CompositionCursor const &cursor)
    -> CompositionColumn &
{
    return composition_column(project.composition, cursor.column_coordinate);
}

[[nodiscard]] inline auto selected_column(ProjectState const &project,
                                          CompositionCursor const &cursor)
    -> CompositionColumn const &
{
    return composition_column(project.composition, cursor.column_coordinate);
}

[[nodiscard]] inline auto selected_duration(ProjectState &project,
                                            CompositionCursor const &cursor)
    -> sequence::TimeSignature &
{
    return selected_column(project, cursor).duration;
}

[[nodiscard]] inline auto selected_duration(ProjectState const &project,
                                            CompositionCursor const &cursor)
    -> sequence::TimeSignature const &
{
    return selected_column(project, cursor).duration;
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
    CompositionCursor cursor{};
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
    std::filesystem::path content_directory{};
    std::filesystem::path tuning_directory{};

    auto operator==(WorkspaceSettings const &) const -> bool = default;
};

struct ContentLibrary
{
    std::vector<LibraryScale> scales{};
    std::vector<Chord> chords{};

    auto operator==(ContentLibrary const &) const -> bool = default;
};

struct ProjectDocumentState
{
    std::optional<std::string> relative_path{};
    std::optional<std::string> file_revision{};
    std::optional<std::string> saved_project_digest{};
    bool dirty{false};

    auto operator==(ProjectDocumentState const &) const -> bool = default;
};

struct RecoveryMetadata
{
    std::string revision{};
    std::uint64_t saved_at_unix_ms{};
    std::optional<std::string> relative_path{};
    ProjectRevision project_revision{};

    auto operator==(RecoveryMetadata const &) const -> bool = default;
};

struct PluginState
{
    WorkspaceSettings workspace{};
    ContentLibrary library{};
    LibraryRevision library_revision{detail::allocate_library_revision()};
    CommandSessionState command_session{};
    std::optional<CopyBufferContent> copy_buffer{};
    ProjectDocumentState document{};
    std::optional<RecoveryMetadata> recovery{};
    StateRevision state_revision{detail::allocate_state_revision()};
    XenTimeline timeline;
};

struct ProjectSnapshot
{
    ProjectState project{};
    HistoryEntryId history_entry_id{};
    ProjectRevision project_revision{};
    StateRevision state_revision{};
    bool preview_active{false};
    ProjectDocumentState document{};
    std::optional<RecoveryMetadata> recovery{};
};

struct InstanceBinding
{
    SessionId session_id{};
    InstanceId instance_id{};
    ChannelId channel_id{DEFAULT_CHANNEL_ID};

    auto operator==(InstanceBinding const &) const -> bool = default;
};

struct PersistedProcessorState
{
    InstanceBinding binding{};
    ProjectState project{};
    ProjectRevision saved_project_revision{};
    StateRevision saved_state_revision{};
    ProjectDocumentState document{};

    auto operator==(PersistedProcessorState const &) const -> bool = default;
};

struct PersistedRecoveryState
{
    SessionId session_id{};
    ProjectState project{};
    ProjectRevision project_revision{};
    StateRevision state_revision{};
    ProjectDocumentState document{};
    std::uint64_t saved_at_unix_ms{};

    auto operator==(PersistedRecoveryState const &) const -> bool = default;
};

struct LibrarySnapshot
{
    ContentLibrary library{};
    WorkspaceSettings workspace{};
    LibraryRevision library_revision{};
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

enum class RealtimeMidiFault : std::uint8_t
{
    None,
    CompilationFailed,
    MissingPpq,
    InvalidTransport,
    BlockTooLarge,
    EventCapacityExceeded,
    MidiByteCapacityExceeded,
};

struct RealtimeMidiStatus
{
    RealtimeMidiFault current_fault{RealtimeMidiFault::None};
    RealtimeMidiFault last_fault{RealtimeMidiFault::None};
    std::uint64_t fault_count{};
};

struct AudioThreadStateForGUI
{
    DAWState daw;
    double loop_phase{0.0};
    bool transport_active{false};
    RealtimeMidiStatus midi_status{};
};

} // namespace xen
