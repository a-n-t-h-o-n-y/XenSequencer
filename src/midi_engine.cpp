#include <xen/midi_engine.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>

#include <sequence/timing.hpp>

#include <xen/midi.hpp>

namespace
{

[[nodiscard]] auto live_voice_less(xen::midi_internal::LiveVoice const &lhs,
                                   xen::midi_internal::LiveVoice const &rhs) -> bool
{
    return std::tie(lhs.channel, lhs.note.begin, lhs.note.end, lhs.note.note,
                    lhs.note.pitch_bend, lhs.note.velocity) <
           std::tie(rhs.channel, rhs.note.begin, rhs.note.end, rhs.note.note,
                    rhs.note.pitch_bend, rhs.note.velocity);
}

[[nodiscard]] auto normalize_live_voices(
    std::vector<xen::midi_internal::LiveVoice> voices)
    -> std::vector<xen::midi_internal::LiveVoice>
{
    std::sort(voices.begin(), voices.end(), live_voice_less);
    voices.erase(std::unique(voices.begin(), voices.end()), voices.end());
    return voices;
}

[[nodiscard]] auto live_voices_at(
    std::vector<xen::midi_internal::AssignedMidiNote> const &assigned_notes,
    xen::SampleCount sample_count, xen::SampleIndex position)
    -> std::vector<xen::midi_internal::LiveVoice>
{
    if (sample_count == 0)
    {
        return {};
    }

    auto const loop_position = position % sample_count;
    auto voices = std::vector<xen::midi_internal::LiveVoice>{};
    for (auto const &assigned : assigned_notes)
    {
        if (assigned.note.begin < loop_position && loop_position < assigned.note.end)
        {
            voices.push_back(xen::midi_internal::live_voice_from(assigned));
        }
    }

    return normalize_live_voices(std::move(voices));
}

void emit_live_voice_note_off(juce::MidiBuffer &buffer,
                              xen::midi_internal::LiveVoice const &voice)
{
    buffer.addEvent(juce::MidiMessage::noteOff(voice.channel, voice.note.note), 0);
}

void emit_live_voice_pitch_bend(juce::MidiBuffer &buffer,
                                xen::midi_internal::LiveVoice const &voice)
{
    buffer.addEvent(juce::MidiMessage::pitchWheel(voice.channel, voice.note.pitch_bend),
                    0);
}

void emit_live_voice_note_on(juce::MidiBuffer &buffer,
                             xen::midi_internal::LiveVoice const &voice)
{
    emit_live_voice_pitch_bend(buffer, voice);
    buffer.addEvent(juce::MidiMessage::noteOn(voice.channel, voice.note.note,
                                              (juce::uint8)voice.note.velocity),
                    0);
}

[[nodiscard]] auto continuation_key(xen::midi_internal::LiveVoice const &voice)
    -> std::tuple<xen::SampleIndex, xen::SampleIndex, int>
{
    return std::tie(voice.note.begin, voice.note.end, voice.note.note);
}

[[nodiscard]] auto live_voice_continuation_less(
    xen::midi_internal::LiveVoice const &lhs, xen::midi_internal::LiveVoice const &rhs)
    -> bool
{
    return continuation_key(lhs) < continuation_key(rhs);
}

[[nodiscard]] auto normalize_live_voice_continuations(
    std::vector<xen::midi_internal::LiveVoice> voices)
    -> std::vector<xen::midi_internal::LiveVoice>
{
    std::sort(voices.begin(), voices.end(), live_voice_continuation_less);
    return voices;
}

void reconcile_live_voices(
    juce::MidiBuffer &buffer,
    std::vector<xen::midi_internal::LiveVoice> &active_live_voices,
    std::vector<xen::midi_internal::LiveVoice> const &desired_live_voices)
{
    auto const current_live_voices =
        normalize_live_voice_continuations(active_live_voices);
    auto const sorted_desired_live_voices =
        normalize_live_voice_continuations(desired_live_voices);

    auto current_index = std::size_t{0};
    auto desired_index = std::size_t{0};
    auto removed_voices = std::vector<xen::midi_internal::LiveVoice>{};
    auto pitch_bend_updates = std::vector<xen::midi_internal::LiveVoice>{};
    auto added_voices = std::vector<xen::midi_internal::LiveVoice>{};
    auto next_active_live_voices = std::vector<xen::midi_internal::LiveVoice>{};

    while (current_index < current_live_voices.size() &&
           desired_index < sorted_desired_live_voices.size())
    {
        auto const &current = current_live_voices[current_index];
        auto desired = sorted_desired_live_voices[desired_index];

        if (live_voice_continuation_less(current, desired))
        {
            removed_voices.push_back(current);
            ++current_index;
            continue;
        }

        if (live_voice_continuation_less(desired, current))
        {
            added_voices.push_back(desired);
            next_active_live_voices.push_back(desired);
            ++desired_index;
            continue;
        }

        desired.channel = current.channel;

        if (desired.note.velocity != current.note.velocity ||
            desired.note.note != current.note.note)
        {
            removed_voices.push_back(current);
            added_voices.push_back(desired);
        }
        else if (desired.note.pitch_bend != current.note.pitch_bend)
        {
            pitch_bend_updates.push_back(desired);
        }

        next_active_live_voices.push_back(desired);
        ++current_index;
        ++desired_index;
    }

    while (current_index < current_live_voices.size())
    {
        removed_voices.push_back(current_live_voices[current_index]);
        ++current_index;
    }

    while (desired_index < sorted_desired_live_voices.size())
    {
        auto const &desired = sorted_desired_live_voices[desired_index];
        added_voices.push_back(desired);
        next_active_live_voices.push_back(desired);
        ++desired_index;
    }

    for (auto const &voice : removed_voices)
    {
        emit_live_voice_note_off(buffer, voice);
    }

    for (auto const &voice : pitch_bend_updates)
    {
        emit_live_voice_pitch_bend(buffer, voice);
    }

    for (auto const &voice : added_voices)
    {
        emit_live_voice_note_on(buffer, voice);
    }

    active_live_voices = normalize_live_voices(std::move(next_active_live_voices));
}

[[nodiscard]] auto render_measure(xen::Measure const &measure,
                                  sequence::Tuning const &tuning, float base_frequency,
                                  xen::DAWState const &daw,
                                  std::optional<xen::Scale> const &scale, int key,
                                  xen::TranslateDirection scale_translate_direction)
    -> std::vector<xen::midi_internal::AssignedMidiNote>
{
    return xen::midi_internal::assign_mpe_channels(xen::state_to_timeline(
        measure, tuning, base_frequency, daw, scale, key, scale_translate_direction));
}

} // namespace

namespace xen
{

auto MidiEngine::step(juce::MidiBuffer const &midi_input, SampleIndex offset,
                      SampleCount length, DAWState const &daw) -> juce::MidiBuffer
{
    auto out_buffer = juce::MidiBuffer{};
    for (auto const metadata : midi_input)
    {
        auto const message = metadata.getMessage();
        if (message.isNoteOnOrOff() || message.isAllNotesOff())
        {
            continue;
        }

        out_buffer.addEvent(message, metadata.samplePosition);
    }

    if (!daw.is_playing || daw.sample_rate == 0 || daw.bpm <= 0.f ||
        rendered_midi_.sample_count == 0)
    {
        for (auto const &voice : active_live_voices_)
        {
            emit_live_voice_note_off(out_buffer, voice);
        }
        active_live_voices_.clear();
        return out_buffer;
    }

    auto const desired_start_live = live_voices_at(rendered_midi_.assigned_notes,
                                                   rendered_midi_.sample_count, offset);
    reconcile_live_voices(out_buffer, active_live_voices_, desired_start_live);

    if (length > std::numeric_limits<SampleIndex>::max() - offset)
    {
        throw std::overflow_error{"MIDI processing window exceeds uint64."};
    }
    auto const window_end = offset + length;

    auto const looped = extract_window(rendered_midi_.midi, rendered_midi_.sample_count,
                                       offset, window_end);
    out_buffer.addEvents(looped, 0, -1, 0);

    auto desired_end_live = live_voices_at(rendered_midi_.assigned_notes,
                                           rendered_midi_.sample_count, window_end);
    auto const desired_end_live_sorted =
        normalize_live_voice_continuations(std::move(desired_end_live));
    auto active_live_voices_sorted =
        normalize_live_voice_continuations(active_live_voices_);
    auto end_index = std::size_t{0};
    auto active_index = std::size_t{0};
    auto next_active_live_voices = std::vector<xen::midi_internal::LiveVoice>{};
    while (end_index < desired_end_live_sorted.size() &&
           active_index < active_live_voices_sorted.size())
    {
        auto desired = desired_end_live_sorted[end_index];
        auto const &active = active_live_voices_sorted[active_index];
        if (live_voice_continuation_less(active, desired))
        {
            ++active_index;
            continue;
        }
        if (live_voice_continuation_less(desired, active))
        {
            next_active_live_voices.push_back(desired);
            ++end_index;
            continue;
        }

        desired.channel = active.channel;
        next_active_live_voices.push_back(desired);
        ++end_index;
        ++active_index;
    }

    while (end_index < desired_end_live_sorted.size())
    {
        next_active_live_voices.push_back(desired_end_live_sorted[end_index]);
        ++end_index;
    }

    active_live_voices_ = normalize_live_voices(std::move(next_active_live_voices));
    return out_buffer;
}

void MidiEngine::update(ProjectState const &project, DAWState const &daw)
{
    auto assigned_notes = render_measure(
        project.measure, project.pitch.tuning.definition, project.pitch.base_frequency,
        daw,
        project.pitch.scale.has_value()
            ? std::optional<Scale>{project.pitch.scale->definition}
            : std::nullopt,
        project.pitch.transposition, project.pitch.translation_direction);
    rendered_midi_ = {
        .midi = midi_internal::render_assigned_notes(assigned_notes),
        .assigned_notes = std::move(assigned_notes),
        .sample_count = midi_internal::checked_measure_sample_count(
            project.measure.time_signature, daw.sample_rate, daw.bpm),
    };
}

auto MidiEngine::get_loop_phase(SampleIndex offset, DAWState const &daw) const -> double
{
    if (!daw.is_playing || daw.sample_rate == 0 || daw.bpm <= 0.f ||
        rendered_midi_.sample_count == 0)
    {
        return 0.0;
    }

    return static_cast<double>(offset % rendered_midi_.sample_count) /
           static_cast<double>(rendered_midi_.sample_count);
}

} // namespace xen
