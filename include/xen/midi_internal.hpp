#pragma once

#include <cstdint>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include <sequence/midi.hpp>
#include <sequence/time_signature.hpp>

namespace xen::midi_internal
{

inline constexpr int mpe_first_member_channel = 2;
inline constexpr int mpe_last_member_channel = 16;

[[nodiscard]] auto checked_duration_sample_count(
    sequence::TimeSignature const &time_signature, std::uint32_t sample_rate, float bpm)
    -> std::uint32_t;

struct AssignedMidiNote
{
    sequence::midi::TimedMidiNote note{};
    int channel{mpe_first_member_channel};

    auto operator==(AssignedMidiNote const &) const -> bool = default;
};

struct LiveVoice
{
    sequence::midi::TimedMidiNote note{};
    int channel{};

    auto operator==(LiveVoice const &) const -> bool = default;
};

[[nodiscard]] auto assign_mpe_channels(
    std::vector<sequence::midi::TimedMidiNote> const &timeline)
    -> std::vector<AssignedMidiNote>;

[[nodiscard]] auto render_assigned_notes(
    std::vector<AssignedMidiNote> const &assigned_notes) -> juce::MidiBuffer;

[[nodiscard]] auto live_voice_from(AssignedMidiNote const &assigned) -> LiveVoice;

} // namespace xen::midi_internal
