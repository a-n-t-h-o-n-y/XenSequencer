#include <xen/midi_engine.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

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

using LiveVoiceSet = xen::MidiEngine::LiveVoiceSet;

void push_live_voice(LiveVoiceSet &set, xen::midi_internal::LiveVoice voice) noexcept
{
    if (set.size < set.voices.size())
    {
        set.voices[set.size] = voice;
        ++set.size;
    }
}

[[nodiscard]] auto normalize_live_voices(LiveVoiceSet voices) -> LiveVoiceSet
{
    auto const begin = voices.voices.begin();
    auto const end = begin + static_cast<std::ptrdiff_t>(voices.size);
    std::sort(begin, end, live_voice_less);
    auto const unique_end = std::unique(begin, end);
    voices.size = static_cast<std::size_t>(std::distance(begin, unique_end));
    return voices;
}

[[nodiscard]] auto loop_column_indices(xen::Composition const &composition)
    -> std::vector<std::size_t>
{
    auto indices = std::vector<std::size_t>{};
    auto const column_count = composition.columns.size();
    indices.reserve(column_count);

    auto const start = composition.loop_region.start_column;
    auto const end = composition.loop_region.end_column;
    if (start <= end)
    {
        for (auto index = start; index <= end; ++index)
        {
            indices.push_back(index);
        }
        return indices;
    }

    for (auto index = start; index < column_count; ++index)
    {
        indices.push_back(index);
    }
    for (auto index = std::size_t{0}; index <= end; ++index)
    {
        indices.push_back(index);
    }
    return indices;
}

[[nodiscard]] auto live_voices_at(
    std::vector<xen::midi_internal::AssignedMidiNote> const &assigned_notes,
    xen::SampleCount sample_count, xen::SampleIndex position) -> LiveVoiceSet
{
    if (sample_count == 0)
    {
        return {};
    }

    auto const loop_position = position % sample_count;
    auto voices = LiveVoiceSet{};
    for (auto const &assigned : assigned_notes)
    {
        if (assigned.note.begin < loop_position && loop_position < assigned.note.end)
        {
            push_live_voice(voices, xen::midi_internal::live_voice_from(assigned));
        }
    }

    return normalize_live_voices(voices);
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

[[nodiscard]] auto normalize_live_voice_continuations(LiveVoiceSet voices)
    -> LiveVoiceSet
{
    auto const begin = voices.voices.begin();
    auto const end = begin + static_cast<std::ptrdiff_t>(voices.size);
    std::sort(begin, end, live_voice_continuation_less);
    return voices;
}

void reconcile_live_voices(juce::MidiBuffer &buffer, LiveVoiceSet &active_live_voices,
                           LiveVoiceSet const &desired_live_voices)
{
    auto const current_live_voices =
        normalize_live_voice_continuations(active_live_voices);
    auto const sorted_desired_live_voices =
        normalize_live_voice_continuations(desired_live_voices);

    auto current_index = std::size_t{0};
    auto desired_index = std::size_t{0};
    auto removed_voices = LiveVoiceSet{};
    auto pitch_bend_updates = LiveVoiceSet{};
    auto added_voices = LiveVoiceSet{};
    auto next_active_live_voices = LiveVoiceSet{};

    while (current_index < current_live_voices.size &&
           desired_index < sorted_desired_live_voices.size)
    {
        auto const &current = current_live_voices.voices[current_index];
        auto desired = sorted_desired_live_voices.voices[desired_index];

        if (live_voice_continuation_less(current, desired))
        {
            push_live_voice(removed_voices, current);
            ++current_index;
            continue;
        }

        if (live_voice_continuation_less(desired, current))
        {
            push_live_voice(added_voices, desired);
            push_live_voice(next_active_live_voices, desired);
            ++desired_index;
            continue;
        }

        desired.channel = current.channel;

        if (desired.note.velocity != current.note.velocity ||
            desired.note.note != current.note.note)
        {
            push_live_voice(removed_voices, current);
            push_live_voice(added_voices, desired);
        }
        else if (desired.note.pitch_bend != current.note.pitch_bend)
        {
            push_live_voice(pitch_bend_updates, desired);
        }

        push_live_voice(next_active_live_voices, desired);
        ++current_index;
        ++desired_index;
    }

    while (current_index < current_live_voices.size)
    {
        push_live_voice(removed_voices, current_live_voices.voices[current_index]);
        ++current_index;
    }

    while (desired_index < sorted_desired_live_voices.size)
    {
        auto const &desired = sorted_desired_live_voices.voices[desired_index];
        push_live_voice(added_voices, desired);
        push_live_voice(next_active_live_voices, desired);
        ++desired_index;
    }

    for (auto i = std::size_t{0}; i < removed_voices.size; ++i)
    {
        emit_live_voice_note_off(buffer, removed_voices.voices[i]);
    }

    for (auto i = std::size_t{0}; i < pitch_bend_updates.size; ++i)
    {
        emit_live_voice_pitch_bend(buffer, pitch_bend_updates.voices[i]);
    }

    for (auto i = std::size_t{0}; i < added_voices.size; ++i)
    {
        emit_live_voice_note_on(buffer, added_voices.voices[i]);
    }

    active_live_voices = normalize_live_voices(next_active_live_voices);
}

[[nodiscard]] auto render_cell_timeline(
    sequence::Cell const &cell, sequence::TimeSignature duration,
    sequence::Tuning const &tuning, float base_frequency, xen::DAWState const &daw,
    std::optional<xen::Scale> const &scale, int key,
    xen::TranslateDirection scale_translate_direction)
    -> std::vector<sequence::midi::TimedMidiNote>
{
    return xen::state_to_timeline(cell, duration, tuning, base_frequency, daw, scale,
                                  key, scale_translate_direction);
}

[[nodiscard]] auto checked_add_sample_count(xen::SampleCount lhs, xen::SampleCount rhs)
    -> xen::SampleCount
{
    if (lhs > std::numeric_limits<xen::SampleCount>::max() - rhs)
    {
        throw std::overflow_error{"Composition sample count overflow."};
    }
    return lhs + rhs;
}

void offset_timeline(std::vector<sequence::midi::TimedMidiNote> &timeline,
                     xen::SampleCount offset)
{
    for (auto &note : timeline)
    {
        note.begin = checked_add_sample_count(note.begin, offset);
        note.end = checked_add_sample_count(note.end, offset);
    }
}

[[nodiscard]] auto phase_relative_offset(xen::SampleIndex offset,
                                         xen::SampleCount sample_count,
                                         xen::SampleCount phase_origin)
    -> xen::SampleIndex
{
    if (sample_count == 0)
    {
        return offset;
    }
    return offset >= phase_origin ? offset - phase_origin : offset;
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
        for (auto i = std::size_t{0}; i < active_live_voices_.size; ++i)
        {
            emit_live_voice_note_off(out_buffer, active_live_voices_.voices[i]);
        }
        active_live_voices_ = {};
        return out_buffer;
    }

    auto const relative_offset = phase_relative_offset(
        offset, rendered_midi_.sample_count, rendered_midi_.phase_origin);
    auto const desired_start_live = live_voices_at(
        rendered_midi_.assigned_notes, rendered_midi_.sample_count, relative_offset);
    reconcile_live_voices(out_buffer, active_live_voices_, desired_start_live);

    if (length > std::numeric_limits<SampleIndex>::max() - relative_offset)
    {
        active_live_voices_ = {};
        return out_buffer;
    }
    auto const window_end = relative_offset + length;

    auto const looped = extract_window(rendered_midi_.midi, rendered_midi_.sample_count,
                                       relative_offset, window_end);
    out_buffer.addEvents(looped, 0, -1, 0);

    auto desired_end_live = live_voices_at(rendered_midi_.assigned_notes,
                                           rendered_midi_.sample_count, window_end);
    auto const desired_end_live_sorted =
        normalize_live_voice_continuations(std::move(desired_end_live));
    auto active_live_voices_sorted =
        normalize_live_voice_continuations(active_live_voices_);
    auto end_index = std::size_t{0};
    auto active_index = std::size_t{0};
    auto next_active_live_voices = LiveVoiceSet{};
    while (end_index < desired_end_live_sorted.size &&
           active_index < active_live_voices_sorted.size)
    {
        auto desired = desired_end_live_sorted.voices[end_index];
        auto const &active = active_live_voices_sorted.voices[active_index];
        if (live_voice_continuation_less(active, desired))
        {
            ++active_index;
            continue;
        }
        if (live_voice_continuation_less(desired, active))
        {
            push_live_voice(next_active_live_voices, desired);
            ++end_index;
            continue;
        }

        desired.channel = active.channel;
        push_live_voice(next_active_live_voices, desired);
        ++end_index;
        ++active_index;
    }

    while (end_index < desired_end_live_sorted.size)
    {
        push_live_voice(next_active_live_voices,
                        desired_end_live_sorted.voices[end_index]);
        ++end_index;
    }

    active_live_voices_ = normalize_live_voices(next_active_live_voices);
    return out_buffer;
}

auto MidiEngine::render(ProjectState const &project, DAWState const &daw,
                        ChannelId const &channel_id) -> std::optional<MidiSequence>
{
    try
    {
        auto const loop_columns = loop_column_indices(project.composition);
        auto column_offsets = std::vector<SampleCount>{};
        column_offsets.reserve(loop_columns.size());

        auto phase_origin = SampleCount{};
        for (auto column_index = std::size_t{0};
             column_index < project.composition.loop_region.start_column;
             ++column_index)
        {
            auto const &column = project.composition.columns[column_index];
            auto const column_samples = midi_internal::checked_duration_sample_count(
                column.duration, daw.sample_rate, daw.bpm);
            phase_origin = checked_add_sample_count(phase_origin, column_samples);
        }

        auto sample_count = SampleCount{};
        for (auto const column_index : loop_columns)
        {
            column_offsets.push_back(sample_count);
            auto const &column = project.composition.columns[column_index];
            auto const column_samples = midi_internal::checked_duration_sample_count(
                column.duration, daw.sample_rate, daw.bpm);
            sample_count = checked_add_sample_count(sample_count, column_samples);
        }

        auto timeline = std::vector<sequence::midi::TimedMidiNote>{};
        for (auto const &row : project.composition.rows)
        {
            if (row.channel_id != channel_id)
            {
                continue;
            }
            for (auto loop_index = std::size_t{0}; loop_index < loop_columns.size();
                 ++loop_index)
            {
                auto const column_index = loop_columns[loop_index];
                auto const sequence_id = row.cells[column_index];
                if (!sequence_id.has_value())
                {
                    continue;
                }

                auto const *cell = find_sequence(project.sequence_bank, *sequence_id);
                if (cell == nullptr)
                {
                    throw std::invalid_argument{
                        "Composition references an unknown sequence ID."};
                }
                auto const &column = project.composition.columns[column_index];
                auto cell_timeline = render_cell_timeline(
                    *cell, column.duration, column.pitch.tuning.definition,
                    column.pitch.base_frequency, daw,
                    column.pitch.scale.has_value()
                        ? std::optional<Scale>{column.pitch.scale->definition}
                        : std::nullopt,
                    column.pitch.transposition, column.pitch.translation_direction);
                offset_timeline(cell_timeline, column_offsets[loop_index]);
                timeline.insert(timeline.end(),
                                std::make_move_iterator(cell_timeline.begin()),
                                std::make_move_iterator(cell_timeline.end()));
            }
        }

        auto assigned_notes = midi_internal::assign_mpe_channels(timeline);
        return MidiSequence{
            .midi = midi_internal::render_assigned_notes(assigned_notes),
            .assigned_notes = std::move(assigned_notes),
            .sample_count = sample_count,
            .phase_origin = phase_origin,
        };
    }
    catch (...)
    {
        return std::nullopt;
    }
}

void MidiEngine::update(ProjectState const &project, DAWState const &daw)
{
    update(project, daw, DEFAULT_CHANNEL_ID);
}

void MidiEngine::update(ProjectState const &project, DAWState const &daw,
                        ChannelId const &channel_id)
{
    if (auto rendered = render(project, daw, channel_id))
    {
        rendered_midi_ = std::move(*rendered);
    }
}

auto MidiEngine::get_loop_phase(SampleIndex offset, DAWState const &daw) const -> double
{
    if (!daw.is_playing || daw.sample_rate == 0 || daw.bpm <= 0.f ||
        rendered_midi_.sample_count == 0)
    {
        return 0.0;
    }

    auto const relative_offset = phase_relative_offset(
        offset, rendered_midi_.sample_count, rendered_midi_.phase_origin);
    return static_cast<double>(relative_offset) /
           static_cast<double>(rendered_midi_.sample_count);
}

} // namespace xen
