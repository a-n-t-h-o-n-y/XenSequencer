#include <xen/midi_compiler.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

#include <sequence/sequence.hpp>
#include <sequence/tuning.hpp>

#include <xen/composition.hpp>
#include <xen/project_validation.hpp>
#include <xen/scale.hpp>

namespace
{

constexpr auto pitch_bend_range = 48.0;
constexpr auto fnv_offset = std::uint64_t{1469598103934665603ULL};
constexpr auto fnv_prime = std::uint64_t{1099511628211ULL};

struct PathHash
{
    std::uint64_t high{fnv_offset};
    std::uint64_t low{fnv_offset ^ 0x9e3779b97f4a7c15ULL};
};

[[nodiscard]] auto append_path(PathHash hash, std::uint64_t value) noexcept -> PathHash
{
    hash.high = (hash.high ^ value) * fnv_prime;
    hash.low ^= value + 0x9e3779b97f4a7c15ULL + (hash.low << 6U) + (hash.low >> 2U);
    return hash;
}

[[nodiscard]] auto duration_beats(sequence::TimeSignature const &duration) -> double
{
    if (duration.numerator == 0 || duration.denominator == 0)
    {
        throw std::invalid_argument{"Column duration values must be nonzero."};
    }
    auto const beats = static_cast<double>(duration.numerator) * 4.0 /
                       static_cast<double>(duration.denominator);
    if (!std::isfinite(beats) || beats <= 0.0)
    {
        throw std::overflow_error{"Column duration is not representable in beats."};
    }
    return beats;
}

struct ColumnOffsets
{
    double default_duration{};
    std::vector<xen::CompositionCoordinate> coordinates{};
    std::vector<double> prefix_deltas{0.0};
};

[[nodiscard]] auto make_column_offsets(xen::Composition const &composition,
                                       xen::CompositionCoordinate loop_start,
                                       xen::CompositionCoordinate loop_end)
    -> ColumnOffsets
{
    auto result = ColumnOffsets{
        .default_duration = duration_beats(composition.default_column.duration)};
    for (auto const &[coordinate, column] : composition.columns)
    {
        if (coordinate < loop_start || coordinate > loop_end)
        {
            continue;
        }
        result.coordinates.push_back(coordinate);
        result.prefix_deltas.push_back(result.prefix_deltas.back() +
                                       duration_beats(column.duration) -
                                       result.default_duration);
    }
    return result;
}

[[nodiscard]] auto beat_offset(ColumnOffsets const &offsets,
                               xen::CompositionCoordinate loop_start,
                               std::int64_t column) -> double
{
    auto const span = column - std::int64_t{loop_start};
    if (span < 0)
    {
        throw std::invalid_argument{"Column precedes the composition loop."};
    }
    auto const delta_end = std::lower_bound(offsets.coordinates.begin(),
                                            offsets.coordinates.end(), column);
    auto const count =
        static_cast<std::size_t>(std::distance(offsets.coordinates.begin(), delta_end));
    auto const result = static_cast<double>(span) * offsets.default_duration +
                        offsets.prefix_deltas[count];
    if (!std::isfinite(result))
    {
        throw std::overflow_error{"Composition beat offset overflow."};
    }
    return result;
}

struct MidiPitch
{
    std::uint8_t note{};
    std::uint16_t pitch_bend{8192};
};

[[nodiscard]] auto to_midi_pitch(int pitch, sequence::Tuning const &tuning,
                                 float base_frequency) -> MidiPitch
{
    if (tuning.intervals.empty() ||
        tuning.intervals.size() >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        !std::isfinite(base_frequency) || base_frequency <= 0.0F)
    {
        throw std::invalid_argument{"Invalid tuning or base frequency."};
    }

    auto const length = static_cast<int>(tuning.intervals.size());
    auto const octave = pitch / length;
    auto interval = pitch % length;
    auto cents = static_cast<double>(octave) * static_cast<double>(tuning.octave);
    if (interval < 0)
    {
        interval += length;
        cents +=
            static_cast<double>(tuning.intervals[static_cast<std::size_t>(interval)]) -
            static_cast<double>(tuning.octave);
    }
    else
    {
        cents +=
            static_cast<double>(tuning.intervals[static_cast<std::size_t>(interval)]);
    }

    constexpr auto a4_note = 69.0;
    constexpr auto a4_hz = 440.0;
    auto const fractional_note =
        12.0 * std::log2(static_cast<double>(base_frequency) / a4_hz) + a4_note +
        cents / 100.0;
    if (!std::isfinite(fractional_note))
    {
        throw std::overflow_error{"MIDI note calculation is not finite."};
    }

    auto integral = 0.0;
    auto const fractional =
        std::modf(std::clamp(fractional_note, 0.0, 127.0), &integral);
    auto const bend = 8192.0 + fractional * 8192.0 / pitch_bend_range;
    if (!std::isfinite(bend) || bend < 0.0 || bend > 16383.0)
    {
        throw std::overflow_error{"Pitch bend exceeds the MIDI range."};
    }
    return {static_cast<std::uint8_t>(integral), static_cast<std::uint16_t>(bend)};
}

struct FlattenContext
{
    xen::CompiledMidiSchedule &schedule;
    xen::CompositionPosition placement;
    xen::SequenceId sequence_id{};
    sequence::Tuning const &tuning;
    float base_frequency{};
    std::optional<std::vector<int>> valid_scale_pitches{};
    std::size_t tuning_length{};
    int transposition{};
    xen::TranslateDirection translate_direction{xen::TranslateDirection::Up};
};

void flatten_elements(std::vector<sequence::MusicElement> const &elements, double begin,
                      double length, PathHash path, FlattenContext &context);

void append_note(sequence::Note const &source, double begin, double length,
                 PathHash path, FlattenContext &context)
{
    auto pitch = source.pitch;
    if (context.valid_scale_pitches.has_value())
    {
        pitch =
            xen::map_pitch_to_scale(pitch, *context.valid_scale_pitches,
                                    context.tuning_length, context.translate_direction);
    }
    auto const widened = static_cast<std::int64_t>(pitch) + context.transposition;
    if (widened < std::numeric_limits<int>::min() ||
        widened > std::numeric_limits<int>::max())
    {
        throw std::overflow_error{"Transposed pitch exceeds int."};
    }

    auto const note_begin = begin + length * static_cast<double>(source.delay);
    auto const note_end =
        note_begin + (length - length * static_cast<double>(source.delay)) *
                         static_cast<double>(source.gate);
    if (!std::isfinite(note_begin) || !std::isfinite(note_end))
    {
        throw std::overflow_error{"Note timing is not finite."};
    }
    if (note_end <= note_begin)
    {
        return;
    }

    auto const midi = to_midi_pitch(static_cast<int>(widened), context.tuning,
                                    context.base_frequency);
    context.schedule.notes.push_back({
        .key =
            {
                .row = context.placement.row_coordinate,
                .column = context.placement.column_coordinate,
                .sequence_id = context.sequence_id,
                .path_hash_high = path.high,
                .path_hash_low = path.low,
            },
        .begin_beat = note_begin,
        .end_beat = note_end,
        .note = midi.note,
        .velocity = static_cast<std::uint8_t>(source.velocity * 127.0F),
        .pitch_bend = midi.pitch_bend,
    });
}

void flatten_sequence(sequence::Sequence const &sequence, double begin, double length,
                      PathHash path, FlattenContext &context)
{
    auto total_weight = 0.0;
    for (auto const &cell : sequence.cells)
    {
        total_weight += static_cast<double>(cell.weight);
    }
    if (!std::isfinite(total_weight) || total_weight <= 0.0)
    {
        throw std::invalid_argument{"Sequence weights must be finite and positive."};
    }

    auto cursor = begin;
    for (auto index = std::size_t{0}; index < sequence.cells.size(); ++index)
    {
        auto const &cell = sequence.cells[index];
        auto const cell_length =
            index + 1 == sequence.cells.size()
                ? begin + length - cursor
                : length * static_cast<double>(cell.weight) / total_weight;
        auto const child_path = append_path(append_path(path, 0x43454c4cULL), index);
        flatten_elements(cell.elements, cursor, cell_length, child_path, context);
        cursor += cell_length;
    }
}

void flatten_elements(std::vector<sequence::MusicElement> const &elements, double begin,
                      double length, PathHash path, FlattenContext &context)
{
    for (auto index = std::size_t{0}; index < elements.size(); ++index)
    {
        auto const element_path = append_path(append_path(path, 0x454c454dULL), index);
        std::visit(
            [&](auto const &element) {
                using Element = std::decay_t<decltype(element)>;
                if constexpr (std::is_same_v<Element, sequence::Note>)
                {
                    append_note(element, begin, length, element_path, context);
                }
                else
                {
                    flatten_sequence(element, begin, length, element_path, context);
                }
            },
            elements[index]);
    }
}

struct SimulationVoice
{
    bool active{};
    std::uint32_t note_index{};
    double begin{};
    std::uint64_t order{};
};

void build_boundaries_and_seek_spans(xen::CompiledMidiSchedule &schedule)
{
    auto simulation = std::vector<xen::CompiledMidiBoundary>{};
    simulation.reserve(schedule.notes.size() * 2U);
    for (auto index = std::size_t{0}; index < schedule.notes.size(); ++index)
    {
        auto const note_index = static_cast<std::uint32_t>(index);
        auto const &note = schedule.notes[index];
        simulation.push_back({note.end_beat, note_index, xen::MidiBoundaryKind::End});
        simulation.push_back(
            {note.begin_beat, note_index, xen::MidiBoundaryKind::Start});
    }
    auto const less = [&](xen::CompiledMidiBoundary const &lhs,
                          xen::CompiledMidiBoundary const &rhs) {
        auto const &lhs_note = schedule.notes[lhs.note_index];
        auto const &rhs_note = schedule.notes[rhs.note_index];
        return std::tie(lhs.beat, lhs.kind, lhs_note.key) <
               std::tie(rhs.beat, rhs.kind, rhs_note.key);
    };
    std::sort(simulation.begin(), simulation.end(), less);

    auto voices = std::array<SimulationVoice, xen::MPE_MEMBER_CHANNEL_COUNT>{};
    auto activation_order = std::uint64_t{0};
    auto close_voice = [&](std::size_t channel, double end) {
        auto &voice = voices[channel];
        if (!voice.active)
        {
            return;
        }
        schedule.seek_spans[channel].push_back({voice.begin, end, voice.note_index});
        voice = {};
    };

    for (auto const &boundary : simulation)
    {
        if (boundary.kind == xen::MidiBoundaryKind::End)
        {
            for (auto channel = std::size_t{0}; channel < voices.size(); ++channel)
            {
                if (voices[channel].active &&
                    voices[channel].note_index == boundary.note_index)
                {
                    close_voice(channel, boundary.beat);
                    break;
                }
            }
            continue;
        }

        auto channel = voices.size();
        for (auto index = std::size_t{0}; index < voices.size(); ++index)
        {
            if (!voices[index].active)
            {
                channel = index;
                break;
            }
        }
        if (channel == voices.size())
        {
            channel = 0;
            for (auto index = std::size_t{1}; index < voices.size(); ++index)
            {
                if (std::tie(voices[index].order, index) <
                    std::tie(voices[channel].order, channel))
                {
                    channel = index;
                }
            }
            close_voice(channel, boundary.beat);
        }
        voices[channel] = {
            .active = true,
            .note_index = boundary.note_index,
            .begin = boundary.beat,
            .order = activation_order++,
        };
    }

    schedule.boundaries = std::move(simulation);
    for (auto &boundary : schedule.boundaries)
    {
        if (boundary.beat == schedule.loop_beats)
        {
            boundary.beat = 0.0;
        }
    }
    std::sort(schedule.boundaries.begin(), schedule.boundaries.end(), less);
}

} // namespace

namespace xen
{

auto MidiCompiler::compile(ProjectState const &project, ChannelId const &channel_id,
                           std::uint64_t generation) -> CompiledMidiSchedule
{
    validate(project);
    auto schedule = CompiledMidiSchedule{
        .generation = generation,
        .channel_id = channel_id,
    };

    auto const loop_start = project.composition.loop_region.start_column;
    auto const loop_end = project.composition.loop_region.end_column;
    auto const offsets = make_column_offsets(project.composition, loop_start, loop_end);
    schedule.loop_beats = beat_offset(offsets, loop_start, std::int64_t{loop_end} + 1);
    if (!std::isfinite(schedule.loop_beats) || schedule.loop_beats <= 0.0)
    {
        throw std::overflow_error{"Composition loop duration is invalid."};
    }

    for (auto const &[position, sequence_id] : project.composition.placements)
    {
        if (position.column_coordinate < loop_start ||
            position.column_coordinate > loop_end)
        {
            continue;
        }
        auto const &row = composition_row(project.composition, position.row_coordinate);
        if (row.channel_id != channel_id)
        {
            continue;
        }
        auto const *cell = find_sequence(project.sequence_bank, sequence_id);
        if (cell == nullptr)
        {
            throw std::invalid_argument{
                "Composition references an unknown sequence ID."};
        }
        auto const &column =
            composition_column(project.composition, position.column_coordinate);
        auto valid_scale_pitches = std::optional<std::vector<int>>{};
        if (column.pitch.scale.has_value())
        {
            valid_scale_pitches =
                generate_valid_pitches(column.pitch.scale->definition);
        }
        auto context = FlattenContext{
            .schedule = schedule,
            .placement = position,
            .sequence_id = sequence_id,
            .tuning = column.pitch.tuning.definition,
            .base_frequency = column.pitch.base_frequency,
            .valid_scale_pitches = std::move(valid_scale_pitches),
            .tuning_length = column.pitch.tuning.definition.intervals.size(),
            .transposition = column.pitch.transposition,
            .translate_direction = column.pitch.translation_direction,
        };
        flatten_elements(cell->elements,
                         beat_offset(offsets, loop_start, position.column_coordinate),
                         duration_beats(column.duration), PathHash{}, context);
    }

    auto const timing_tolerance = std::numeric_limits<double>::epsilon() * 8.0 *
                                  std::max(1.0, schedule.loop_beats);
    for (auto &note : schedule.notes)
    {
        if (std::abs(note.begin_beat) <= timing_tolerance)
        {
            note.begin_beat = 0.0;
        }
        if (std::abs(note.end_beat - schedule.loop_beats) <= timing_tolerance)
        {
            note.end_beat = schedule.loop_beats;
        }
        if (note.begin_beat < 0.0 || note.end_beat > schedule.loop_beats ||
            note.end_beat <= note.begin_beat)
        {
            throw std::overflow_error{
                "Compiled note lies outside the composition loop."};
        }
    }

    std::sort(schedule.notes.begin(), schedule.notes.end(),
              [](auto const &lhs, auto const &rhs) {
                  return std::tie(lhs.begin_beat, lhs.end_beat, lhs.key) <
                         std::tie(rhs.begin_beat, rhs.end_beat, rhs.key);
              });
    if (schedule.notes.size() > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::overflow_error{"Compiled MIDI note count exceeds uint32."};
    }
    build_boundaries_and_seek_spans(schedule);
    return schedule;
}

} // namespace xen
