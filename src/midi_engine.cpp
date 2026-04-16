#include <xen/midi_engine.hpp>

#include <utility>

#include <sequence/timing.hpp>

#include <xen/midi.hpp>

namespace
{

[[nodiscard]] auto render_measure(xen::Measure const &measure,
                                  sequence::Tuning const &tuning,
                                  float base_frequency,
                                  xen::DAWState const &daw,
                                  std::optional<xen::Scale> const &scale,
                                  int key,
                                  xen::TranslateDirection scale_translate_direction)
    -> juce::MidiBuffer
{
    return xen::render_to_midi(xen::state_to_timeline(
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

    if (daw.sample_rate == 0 || daw.bpm <= 0.f || rendered_midi_.sample_count == 0)
    {
        return out_buffer;
    }

    auto const looped =
        extract_window(rendered_midi_.midi, rendered_midi_.sample_count, offset,
                       offset + length);
    out_buffer.addEvents(looped, 0, -1, 0);
    return out_buffer;
}

void MidiEngine::update(EngineState const &sequencer, DAWState const &daw)
{
    rendered_midi_ = {
        .midi = render_measure(sequencer.measure, sequencer.tuning,
                               sequencer.base_frequency, daw, sequencer.scale,
                               sequencer.key, sequencer.scale_translate_direction),
        .sample_count =
            sequence::samples_count(sequencer.measure.time_signature,
                                    daw.sample_rate, daw.bpm),
    };
}

auto MidiEngine::get_loop_phase(SampleIndex offset, DAWState const &daw) const -> double
{
    if (daw.sample_rate == 0 || daw.bpm <= 0.f || rendered_midi_.sample_count == 0)
    {
        return 0.0;
    }

    return static_cast<double>(offset % rendered_midi_.sample_count) /
           static_cast<double>(rendered_midi_.sample_count);
}

} // namespace xen
