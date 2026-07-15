#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <xen/composition.hpp>

namespace xen
{

inline constexpr auto MPE_FIRST_MEMBER_CHANNEL = 2;
inline constexpr auto MPE_LAST_MEMBER_CHANNEL = 16;
inline constexpr auto MPE_MEMBER_CHANNEL_COUNT =
    static_cast<std::size_t>(MPE_LAST_MEMBER_CHANNEL - MPE_FIRST_MEMBER_CHANNEL + 1);

struct LogicalNoteKey
{
    CompositionCoordinate row{};
    CompositionCoordinate column{};
    SequenceId sequence_id{};
    std::uint64_t path_hash_high{};
    std::uint64_t path_hash_low{};

    auto operator<=>(LogicalNoteKey const &) const = default;
};

struct CompiledMidiCc
{
    std::uint8_t controller{};
    std::uint8_t value{};

    auto operator==(CompiledMidiCc const &) const -> bool = default;
};

struct CompiledMidiNote
{
    LogicalNoteKey key{};
    double begin_beat{};
    double end_beat{};
    std::uint8_t note{};
    std::uint8_t velocity{};
    std::uint16_t pitch_bend{8192};
    std::vector<CompiledMidiCc> midi_cc{};

    auto operator==(CompiledMidiNote const &) const -> bool = default;
};

enum class MidiBoundaryKind : std::uint8_t
{
    End,
    Start,
};

struct CompiledMidiBoundary
{
    double beat{};
    std::uint32_t note_index{};
    MidiBoundaryKind kind{MidiBoundaryKind::Start};
};

struct CompiledMidiSeekSpan
{
    double begin_beat{};
    double end_beat{};
    std::uint32_t note_index{};
};

struct CompiledMidiSchedule
{
    std::uint64_t generation{};
    std::string channel_id{};
    double loop_beats{};
    std::vector<CompiledMidiNote> notes{};
    std::vector<CompiledMidiBoundary> boundaries{};
    std::array<std::vector<CompiledMidiSeekSpan>, MPE_MEMBER_CHANNEL_COUNT>
        seek_spans{};
};

enum class MidiCompilationError : std::uint8_t
{
    None,
    InvalidProject,
    NumericOverflow,
    OutOfMemory,
    Internal,
};

struct CompiledMidiUpdate
{
    std::uint64_t generation{};
    MidiCompilationError error{MidiCompilationError::None};
    CompiledMidiSchedule schedule{};

    [[nodiscard]] auto ready() const noexcept -> bool
    {
        return error == MidiCompilationError::None;
    }
};

struct MidiCompilationStatus
{
    std::uint64_t requested_generation{};
    std::uint64_t published_generation{};
    MidiCompilationError error{MidiCompilationError::None};
    std::string message{};
};

} // namespace xen
