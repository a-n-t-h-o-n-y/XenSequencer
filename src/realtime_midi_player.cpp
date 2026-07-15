#include <xen/realtime_midi_player.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <tuple>

namespace
{

constexpr auto midi_record_overhead = std::size_t{16};

[[nodiscard]] auto positive_mod(double value, double modulus) noexcept -> double
{
    auto result = std::fmod(value, modulus);
    if (result < 0.0)
    {
        result += modulus;
    }
    return result;
}

[[nodiscard]] auto suppress_input_message(
    juce::MidiMessageMetadata const &message) noexcept -> bool
{
    if (message.numBytes <= 0 || message.data == nullptr)
    {
        return false;
    }
    auto const status = message.data[0];
    auto const type = static_cast<std::uint8_t>(status & 0xf0U);
    if (type == 0x80U || type == 0x90U)
    {
        return true;
    }
    return type == 0xb0U && message.numBytes >= 2 && message.data[1] == 123U;
}

[[nodiscard]] auto channel_status(std::uint8_t type, std::uint8_t channel) noexcept
    -> std::uint8_t
{
    return static_cast<std::uint8_t>(type | ((channel - 1U) & 0x0fU));
}

} // namespace

namespace xen
{

void RealtimeMidiPlayer::prepare(std::uint32_t sample_rate, int maximum_block_size)
{
    prepared_sample_rate_ = sample_rate;
    maximum_block_size_ = std::max(maximum_block_size, 0);
    bootstrap_buffer_.ensureSize(RealtimeMidiLimits::midi_storage_bytes);
    output_buffer_.ensureSize(RealtimeMidiLimits::midi_storage_bytes);
    bootstrap_buffer_.clear();
    output_buffer_.clear();
    bootstrap_output_ = true;
    prepared_ = sample_rate > 0 && maximum_block_size_ > 0;
    force_reconcile_ = true;
}

void RealtimeMidiPlayer::release() noexcept
{
    voices_ = {};
    prepared_ = false;
    was_playing_ = false;
    force_reconcile_ = true;
    loop_phase_ = 0.0;
}

void RealtimeMidiPlayer::adopt(CompiledMidiUpdate const &update) noexcept
{
    if (!update.ready())
    {
        schedule_ = nullptr;
        compilation_failed_ = true;
    }
    else
    {
        schedule_ = &update.schedule;
        compilation_failed_ = false;
    }
    force_reconcile_ = true;
}

auto RealtimeMidiPlayer::loop_phase() const noexcept -> double
{
    return loop_phase_;
}

auto RealtimeMidiPlayer::status() const noexcept -> RealtimeMidiStatus
{
    return status_;
}

auto RealtimeMidiPlayer::stage_message(int sample_position, std::uint8_t status,
                                       std::uint8_t data1, std::uint8_t data2) noexcept
    -> bool
{
    if (staged_count_ + retained_count_ >= RealtimeMidiLimits::max_messages_per_block)
    {
        return false;
    }
    if (estimated_bytes_ + 3U + midi_record_overhead >
        RealtimeMidiLimits::midi_storage_bytes)
    {
        return false;
    }
    staged_[staged_count_++] = {
        .sample_position = sample_position,
        .data = {status, data1, data2},
    };
    estimated_bytes_ += 3U + midi_record_overhead;
    return true;
}

auto RealtimeMidiPlayer::stage_note_off(ActiveVoice const &voice,
                                        int sample_position) noexcept -> bool
{
    return stage_message(sample_position, channel_status(0x80U, voice.channel),
                         voice.note, 0U);
}

auto RealtimeMidiPlayer::stage_pitch_bend(ActiveVoice const &voice,
                                          int sample_position) noexcept -> bool
{
    auto const low = static_cast<std::uint8_t>(voice.pitch_bend & 0x7fU);
    auto const high = static_cast<std::uint8_t>((voice.pitch_bend >> 7U) & 0x7fU);
    return stage_message(sample_position, channel_status(0xe0U, voice.channel), low,
                         high);
}

auto RealtimeMidiPlayer::stage_note_start(ActiveVoice const &voice,
                                          CompiledMidiNote const &note,
                                          int sample_position) noexcept -> bool
{
    return stage_pitch_bend(voice, sample_position) &&
           stage_note_controllers(voice, note, sample_position) &&
           stage_message(sample_position, channel_status(0x90U, voice.channel),
                         voice.note, voice.velocity);
}

auto RealtimeMidiPlayer::stage_note_controllers(ActiveVoice const &voice,
                                                CompiledMidiNote const &note,
                                                int sample_position) noexcept -> bool
{
    for (auto const &controller : note.midi_cc)
    {
        if (!stage_message(sample_position, channel_status(0xb0U, voice.channel),
                           controller.controller, controller.value))
        {
            return false;
        }
    }
    return true;
}

auto RealtimeMidiPlayer::stage_all_note_offs(VoiceSet const &voices,
                                             int sample_position) noexcept -> bool
{
    for (auto const &voice : voices)
    {
        if (voice.active && !stage_note_off(voice, sample_position))
        {
            return false;
        }
    }
    return true;
}

auto RealtimeMidiPlayer::find_voice(VoiceSet const &voices,
                                    LogicalNoteKey const &key) const noexcept
    -> std::size_t
{
    for (auto index = std::size_t{0}; index < voices.size(); ++index)
    {
        if (voices[index].active && voices[index].key == key)
        {
            return index;
        }
    }
    return voices.size();
}

auto RealtimeMidiPlayer::allocate_voice(VoiceSet &voices) const noexcept -> std::size_t
{
    for (auto index = std::size_t{0}; index < voices.size(); ++index)
    {
        if (!voices[index].active)
        {
            return index;
        }
    }
    auto oldest = std::size_t{0};
    for (auto index = std::size_t{1}; index < voices.size(); ++index)
    {
        if (std::tie(voices[index].activation_order, voices[index].channel) <
            std::tie(voices[oldest].activation_order, voices[oldest].channel))
        {
            oldest = index;
        }
    }
    return oldest;
}

auto RealtimeMidiPlayer::reconcile(VoiceSet &voices, std::uint64_t &activation_order,
                                   double phase) noexcept -> bool
{
    if (schedule_ == nullptr)
    {
        auto const ok = stage_all_note_offs(voices, 0);
        voices = {};
        return ok;
    }

    auto desired = std::array<std::uint32_t, MPE_MEMBER_CHANNEL_COUNT>{};
    auto desired_count = std::size_t{0};
    for (auto const &spans : schedule_->seek_spans)
    {
        auto const next =
            std::lower_bound(spans.begin(), spans.end(), phase,
                             [](CompiledMidiSeekSpan const &span, double position) {
                                 return span.begin_beat < position;
                             });
        if (next == spans.begin())
        {
            continue;
        }
        auto const &span = *std::prev(next);
        if (span.begin_beat < phase && phase <= span.end_beat)
        {
            desired[desired_count++] = span.note_index;
        }
    }
    std::sort(desired.begin(),
              desired.begin() + static_cast<std::ptrdiff_t>(desired_count),
              [&](auto lhs, auto rhs) {
                  auto const &lhs_note = schedule_->notes[lhs];
                  auto const &rhs_note = schedule_->notes[rhs];
                  return std::tie(lhs_note.begin_beat, lhs_note.key) <
                         std::tie(rhs_note.begin_beat, rhs_note.key);
              });

    auto matched = std::array<bool, MPE_MEMBER_CHANNEL_COUNT>{};
    auto restart = std::array<bool, MPE_MEMBER_CHANNEL_COUNT>{};
    auto bend_update = std::array<bool, MPE_MEMBER_CHANNEL_COUNT>{};
    auto restart_note = std::array<std::uint32_t, MPE_MEMBER_CHANNEL_COUNT>{};
    auto controller_note = std::array<std::uint32_t, MPE_MEMBER_CHANNEL_COUNT>{};
    restart_note.fill(std::numeric_limits<std::uint32_t>::max());
    controller_note.fill(std::numeric_limits<std::uint32_t>::max());
    for (auto voice_index = std::size_t{0}; voice_index < voices.size(); ++voice_index)
    {
        auto &voice = voices[voice_index];
        if (!voice.active)
        {
            continue;
        }
        auto desired_index = desired_count;
        for (auto index = std::size_t{0}; index < desired_count; ++index)
        {
            if (!matched[index] && schedule_->notes[desired[index]].key == voice.key)
            {
                desired_index = index;
                break;
            }
        }
        if (desired_index == desired_count)
        {
            if (!stage_note_off(voice, 0))
            {
                return false;
            }
            voice = {};
            continue;
        }

        matched[desired_index] = true;
        auto const &note = schedule_->notes[desired[desired_index]];
        if (voice.note != note.note || voice.velocity != note.velocity)
        {
            if (!stage_note_off(voice, 0))
            {
                return false;
            }
            voice.note = note.note;
            voice.velocity = note.velocity;
            voice.pitch_bend = note.pitch_bend;
            restart[voice_index] = true;
            restart_note[voice_index] = desired[desired_index];
        }
        else if (voice.pitch_bend != note.pitch_bend)
        {
            voice.pitch_bend = note.pitch_bend;
            bend_update[voice_index] = true;
        }
        if (!restart[voice_index])
        {
            controller_note[voice_index] = desired[desired_index];
        }
    }

    for (auto index = std::size_t{0}; index < voices.size(); ++index)
    {
        if (bend_update[index] && !stage_pitch_bend(voices[index], 0))
        {
            return false;
        }
    }
    for (auto index = std::size_t{0}; index < voices.size(); ++index)
    {
        if (controller_note[index] != std::numeric_limits<std::uint32_t>::max() &&
            !stage_note_controllers(voices[index],
                                    schedule_->notes[controller_note[index]], 0))
        {
            return false;
        }
    }
    for (auto index = std::size_t{0}; index < voices.size(); ++index)
    {
        if (restart[index] &&
            !stage_note_start(voices[index], schedule_->notes[restart_note[index]], 0))
        {
            return false;
        }
    }

    for (auto index = std::size_t{0}; index < desired_count; ++index)
    {
        if (matched[index])
        {
            continue;
        }
        auto const &note = schedule_->notes[desired[index]];
        auto const voice_index = allocate_voice(voices);
        if (voices[voice_index].active)
        {
            return false;
        }
        voices[voice_index] = {
            .active = true,
            .key = note.key,
            .note = note.note,
            .velocity = note.velocity,
            .pitch_bend = note.pitch_bend,
            .channel =
                static_cast<std::uint8_t>(MPE_FIRST_MEMBER_CHANNEL + voice_index),
            .activation_order = activation_order++,
        };
        if (!stage_note_start(voices[voice_index], note, 0))
        {
            return false;
        }
    }
    return true;
}

auto RealtimeMidiPlayer::render_boundaries(VoiceSet &voices,
                                           std::uint64_t &activation_order,
                                           TransportBlock const &transport,
                                           double phase) noexcept -> bool
{
    auto const block_beats = static_cast<double>(transport.sample_count) *
                             transport.bpm /
                             (60.0 * static_cast<double>(transport.sample_rate));
    auto remaining = block_beats;
    auto segment_phase = phase;
    auto elapsed = 0.0;
    auto visited = std::size_t{0};
    while (remaining > 0.0)
    {
        auto const segment_length =
            std::min(schedule_->loop_beats - segment_phase, remaining);
        auto const segment_end = segment_phase + segment_length;
        auto boundary = std::lower_bound(
            schedule_->boundaries.begin(), schedule_->boundaries.end(), segment_phase,
            [](CompiledMidiBoundary const &item, double position) {
                return item.beat < position;
            });
        for (; boundary != schedule_->boundaries.end() && boundary->beat < segment_end;
             ++boundary)
        {
            if (++visited > RealtimeMidiLimits::max_boundaries_per_block)
            {
                return false;
            }
            auto const relative_beat = elapsed + boundary->beat - segment_phase;
            auto const exact_sample = relative_beat * 60.0 *
                                      static_cast<double>(transport.sample_rate) /
                                      transport.bpm;
            auto const rounded = static_cast<long long>(std::llround(exact_sample));
            auto const sample_position = static_cast<int>(std::clamp<long long>(
                rounded, 0, std::max(transport.sample_count - 1, 0)));
            auto const &note = schedule_->notes[boundary->note_index];
            if (boundary->kind == MidiBoundaryKind::End)
            {
                auto const voice_index = find_voice(voices, note.key);
                if (voice_index != voices.size())
                {
                    if (!stage_note_off(voices[voice_index], sample_position))
                    {
                        return false;
                    }
                    voices[voice_index] = {};
                }
                continue;
            }

            auto const voice_index = allocate_voice(voices);
            if (voices[voice_index].active &&
                !stage_note_off(voices[voice_index], sample_position))
            {
                return false;
            }
            voices[voice_index] = {
                .active = true,
                .key = note.key,
                .note = note.note,
                .velocity = note.velocity,
                .pitch_bend = note.pitch_bend,
                .channel =
                    static_cast<std::uint8_t>(MPE_FIRST_MEMBER_CHANNEL + voice_index),
                .activation_order = activation_order++,
            };
            if (!stage_note_start(voices[voice_index], note, sample_position))
            {
                return false;
            }
        }
        if (segment_length <= 0.0)
        {
            return false;
        }
        remaining -= segment_length;
        elapsed += segment_length;
        segment_phase = 0.0;
        auto const tolerance =
            std::numeric_limits<double>::epsilon() * 8.0 * std::max(1.0, block_beats);
        if (remaining <= tolerance)
        {
            break;
        }
    }
    return true;
}

void RealtimeMidiPlayer::record_fault(RealtimeMidiFault fault) noexcept
{
    if (status_.current_fault != fault)
    {
        ++status_.fault_count;
    }
    status_.current_fault = fault;
    status_.last_fault = fault;
}

void RealtimeMidiPlayer::fail_block(juce::MidiBuffer &output,
                                    RealtimeMidiFault fault) noexcept
{
    output.clear();
    for (auto const &voice : voices_)
    {
        if (!voice.active)
        {
            continue;
        }
        auto const data = std::array<std::uint8_t, 3>{
            channel_status(0x80U, voice.channel), voice.note, 0U};
        output.addEvent(data.data(), static_cast<int>(data.size()), 0);
    }
    voices_ = {};
    force_reconcile_ = true;
    record_fault(fault);
}

void RealtimeMidiPlayer::process(TransportBlock const &transport,
                                 juce::MidiBuffer &midi_buffer) noexcept
{
    auto &output = bootstrap_output_ ? bootstrap_buffer_ : output_buffer_;
    output.clear();
    staged_count_ = 0;
    retained_count_ = 0;
    estimated_bytes_ = 0;
    auto input_fault = RealtimeMidiFault::None;
    for (auto const metadata : midi_buffer)
    {
        if (suppress_input_message(metadata))
        {
            continue;
        }
        ++retained_count_;
        if (retained_count_ > RealtimeMidiLimits::max_messages_per_block)
        {
            input_fault = RealtimeMidiFault::EventCapacityExceeded;
            break;
        }
        estimated_bytes_ += static_cast<std::size_t>(std::max(metadata.numBytes, 0)) +
                            midi_record_overhead;
        if (estimated_bytes_ > RealtimeMidiLimits::midi_storage_bytes)
        {
            input_fault = RealtimeMidiFault::MidiByteCapacityExceeded;
            break;
        }
    }

    auto next_voices = voices_;
    auto next_activation_order = activation_order_;
    auto render_fault = input_fault;
    auto valid_transport = prepared_ && transport.sample_count >= 0 &&
                           transport.sample_count <= maximum_block_size_ &&
                           transport.sample_rate == prepared_sample_rate_ &&
                           std::isfinite(transport.bpm) && transport.bpm > 0.0 &&
                           std::isfinite(transport.ppq);

    if (render_fault == RealtimeMidiFault::None && compilation_failed_)
    {
        (void)stage_all_note_offs(next_voices, 0);
        next_voices = {};
        render_fault = RealtimeMidiFault::CompilationFailed;
    }
    else if (render_fault == RealtimeMidiFault::None && !transport.playing)
    {
        if (!stage_all_note_offs(next_voices, 0))
        {
            render_fault = RealtimeMidiFault::EventCapacityExceeded;
        }
        next_voices = {};
        was_playing_ = false;
        loop_phase_ = 0.0;
        force_reconcile_ = true;
    }
    else if (render_fault == RealtimeMidiFault::None && !transport.has_ppq)
    {
        (void)stage_all_note_offs(next_voices, 0);
        next_voices = {};
        render_fault = RealtimeMidiFault::MissingPpq;
    }
    else if (render_fault == RealtimeMidiFault::None && !valid_transport)
    {
        (void)stage_all_note_offs(next_voices, 0);
        next_voices = {};
        render_fault = transport.sample_count > maximum_block_size_
                           ? RealtimeMidiFault::BlockTooLarge
                           : RealtimeMidiFault::InvalidTransport;
    }
    else if (render_fault == RealtimeMidiFault::None &&
             (schedule_ == nullptr || schedule_->loop_beats <= 0.0))
    {
        if (!stage_all_note_offs(next_voices, 0))
        {
            render_fault = RealtimeMidiFault::EventCapacityExceeded;
        }
        next_voices = {};
        loop_phase_ = 0.0;
    }
    else if (render_fault == RealtimeMidiFault::None)
    {
        auto const phase = positive_mod(transport.ppq, schedule_->loop_beats);
        loop_phase_ = phase / schedule_->loop_beats;
        auto const tolerance =
            1.5 * transport.bpm / (60.0 * static_cast<double>(transport.sample_rate));
        auto const discontinuity =
            !was_playing_ || !std::isfinite(transport.ppq) ||
            std::abs(transport.ppq - expected_next_ppq_) > tolerance;
        if ((force_reconcile_ || discontinuity) &&
            !reconcile(next_voices, next_activation_order, phase))
        {
            render_fault = RealtimeMidiFault::EventCapacityExceeded;
        }
        if (render_fault == RealtimeMidiFault::None && transport.sample_count > 0 &&
            !render_boundaries(next_voices, next_activation_order, transport, phase))
        {
            render_fault = RealtimeMidiFault::EventCapacityExceeded;
        }
        auto const block_beats = static_cast<double>(transport.sample_count) *
                                 transport.bpm /
                                 (60.0 * static_cast<double>(transport.sample_rate));
        expected_next_ppq_ = transport.ppq + block_beats;
        was_playing_ = true;
        force_reconcile_ = false;
    }

    if (render_fault != RealtimeMidiFault::None)
    {
        fail_block(output, render_fault);
    }
    else
    {
        for (auto const metadata : midi_buffer)
        {
            if (!suppress_input_message(metadata))
            {
                output.addEvent(metadata.data, metadata.numBytes,
                                metadata.samplePosition);
            }
        }
        for (auto index = std::size_t{0}; index < staged_count_; ++index)
        {
            auto const &message = staged_[index];
            output.addEvent(message.data.data(), static_cast<int>(message.data.size()),
                            message.sample_position);
        }
        voices_ = next_voices;
        activation_order_ = next_activation_order;
        status_.current_fault = RealtimeMidiFault::None;
    }

    midi_buffer.swapWith(output);
    bootstrap_output_ = false;
}

} // namespace xen
