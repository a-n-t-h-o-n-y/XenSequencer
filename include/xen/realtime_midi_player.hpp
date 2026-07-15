#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <juce_audio_basics/juce_audio_basics.h>

#include <xen/midi_schedule.hpp>
#include <xen/state.hpp>

namespace xen
{

struct RealtimeMidiLimits
{
    static constexpr auto max_messages_per_block = std::size_t{4096};
    static constexpr auto max_boundaries_per_block = std::size_t{4096};
    static constexpr auto midi_storage_bytes = std::size_t{256 * 1024};
};

struct TransportBlock
{
    bool playing{};
    bool has_ppq{};
    double ppq{};
    double bpm{};
    std::uint32_t sample_rate{};
    int sample_count{};
};

class RealtimeMidiPlayer
{
  public:
    void prepare(std::uint32_t sample_rate, int maximum_block_size);
    void release() noexcept;
    // The update must remain alive until the next adoption or final process call.
    void adopt(CompiledMidiUpdate const &update) noexcept;
    void process(TransportBlock const &transport,
                 juce::MidiBuffer &midi_buffer) noexcept;

    [[nodiscard]] auto loop_phase() const noexcept -> double;
    [[nodiscard]] auto status() const noexcept -> RealtimeMidiStatus;

  private:
    struct ActiveVoice
    {
        bool active{};
        LogicalNoteKey key{};
        std::uint8_t note{};
        std::uint8_t velocity{};
        std::uint16_t pitch_bend{8192};
        std::uint8_t channel{};
        std::uint64_t activation_order{};
    };

    struct ShortMessage
    {
        int sample_position{};
        std::array<std::uint8_t, 3> data{};
    };

    using VoiceSet = std::array<ActiveVoice, MPE_MEMBER_CHANNEL_COUNT>;

    [[nodiscard]] auto stage_message(int sample_position, std::uint8_t status,
                                     std::uint8_t data1, std::uint8_t data2) noexcept
        -> bool;
    [[nodiscard]] auto stage_note_off(ActiveVoice const &voice,
                                      int sample_position) noexcept -> bool;
    [[nodiscard]] auto stage_note_start(ActiveVoice const &voice,
                                        CompiledMidiNote const &note,
                                        int sample_position) noexcept -> bool;
    [[nodiscard]] auto stage_pitch_bend(ActiveVoice const &voice,
                                        int sample_position) noexcept -> bool;
    [[nodiscard]] auto stage_note_controllers(ActiveVoice const &voice,
                                              CompiledMidiNote const &note,
                                              int sample_position) noexcept -> bool;
    [[nodiscard]] auto stage_all_note_offs(VoiceSet const &voices,
                                           int sample_position) noexcept -> bool;
    [[nodiscard]] auto reconcile(VoiceSet &voices, std::uint64_t &activation_order,
                                 double phase) noexcept -> bool;
    [[nodiscard]] auto render_boundaries(VoiceSet &voices,
                                         std::uint64_t &activation_order,
                                         TransportBlock const &transport,
                                         double phase) noexcept -> bool;
    [[nodiscard]] auto find_voice(VoiceSet const &voices,
                                  LogicalNoteKey const &key) const noexcept
        -> std::size_t;
    [[nodiscard]] auto allocate_voice(VoiceSet &voices) const noexcept -> std::size_t;
    void fail_block(juce::MidiBuffer &output, RealtimeMidiFault fault) noexcept;
    void record_fault(RealtimeMidiFault fault) noexcept;

    CompiledMidiSchedule const *schedule_{};
    VoiceSet voices_{};
    std::array<ShortMessage, RealtimeMidiLimits::max_messages_per_block> staged_{};
    std::size_t staged_count_{};
    std::size_t retained_count_{};
    std::size_t estimated_bytes_{};
    std::uint64_t activation_order_{};
    std::uint32_t prepared_sample_rate_{};
    int maximum_block_size_{};
    bool prepared_{};
    bool bootstrap_output_{true};
    bool force_reconcile_{true};
    bool compilation_failed_{};
    bool was_playing_{};
    double expected_next_ppq_{};
    double loop_phase_{};
    RealtimeMidiStatus status_{};
    juce::MidiBuffer bootstrap_buffer_{};
    juce::MidiBuffer output_buffer_{};
};

} // namespace xen
