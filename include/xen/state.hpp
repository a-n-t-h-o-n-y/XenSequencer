#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/generate.hpp>
#include <sequence/measure.hpp>
#include <sequence/modify.hpp>
#include <sequence/sequence.hpp>
#include <sequence/tuning.hpp>

#include <signals_light/signal.hpp>

#include <xen/command_history.hpp>
#include <xen/gui/themes.hpp>
#include <xen/input_mode.hpp>
#include <xen/state.hpp>
#include <xen/timeline.hpp>
#include <xen/user_directory.hpp>

namespace xen
{

/**
 * The state of the internal sequencer for the plugin.
 */
struct SequencerState
{
    sequence::Phrase phrase;
    std::array<std::string, 16> measure_names;

    sequence::Tuning tuning;
    std::string tuning_name;

    float base_frequency;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
    auto operator==(SequencerState const &) const -> bool = default;
    auto operator!=(SequencerState const &) const -> bool = default;
#pragma clang diagnostic pop
};

/**
 * The state of the current selection in the sequencer.
 */
struct SelectedState
{
    /// The index of the currently selected Measure in the current Phrase.
    std::size_t measure{0};

    /// The index of the currently selected Cell in the current Measure.
    std::vector<std::size_t> cell{};
};

/**
 * The state of the auxiliary controls in the plugin, for Timeline use.
 */
struct AuxState
{
    SelectedState selected{};
    InputMode input_mode = InputMode::Movement;
};

struct TrackedState
{
    SequencerState sequencer;
    AuxState aux;
};

/**
 * The specific Timeline type for the Xen plugin.
 */
using XenTimeline = Timeline<TrackedState>;

// -------------------------------------------------------------------------------------

/**
 * State shared across plugin instances if the DAW does not sandbox.
 */
struct SharedState
{
    sl::Signal<void()> on_load_keys_request{};
    std::mutex on_load_keys_request_mtx{};

    std::optional<sequence::Cell> copy_buffer{std::nullopt};
    std::mutex copy_buffer_mtx{};

    gui::Theme theme{gui::find_theme("apollo")}; // Needed for editor startup.
    sl::Signal<void(gui::Theme const &)> on_theme_update{};
    std::mutex theme_mtx{};
};

/**
 * The state of the DAW.
 */
struct DAWState
{
    float bpm = 0.f;
    std::uint32_t sample_rate = 0;
};

// bpm = 120.f,
// sample rate = 44'100,

struct PluginState
{
    juce::Uuid const PROCESS_UUID = juce::Uuid{};
    std::string display_name = "XenSequencer";

    juce::File current_phrase_directory{get_phrases_directory()};

    // TODO current_tuning_directory

    sl::Signal<void(std::string const &)> on_focus_request{};
    sl::Signal<void(std::string const &)> on_show_request{};
    DAWState daw_state{};
    CommandHistory command_history{};
    XenTimeline timeline;
    inline static SharedState shared{};
    std::unique_ptr<juce::LookAndFeel> laf{nullptr};
};

[[nodiscard]] inline auto init_state() -> SequencerState
{
    return {
        .phrase = {{
            .cell = sequence::Rest{},
            .time_signature = {4, 4},
        }},
        .measure_names = {"Init Test"},
        .tuning =
            {
                .intervals = {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000,
                              1100},
                .octave = 1200,
            },
        .tuning_name = "12EDO",
        .base_frequency = 440.f,
    };
}

/**
 * Generates a demo state for testing.
 *
 * @return SequencerState
 */
[[nodiscard]] inline auto demo_state() -> SequencerState
{
    namespace seq = sequence;

    auto phrase = seq::Phrase{};

    for (auto i = 0; i < 2; ++i)
    {
        auto measure = seq::create_measure(seq::TimeSignature{4, 4});
        measure.cell = seq::generate::full(4, seq::Note{0, 1.0, 0.f, 0.8f});

        measure.cell =
            seq::modify::randomize_intervals(measure.cell, {0, {1}}, -40, 40);
        measure.cell =
            seq::modify::randomize_velocity(measure.cell, {0, {1}}, 0.f, 1.f);
        measure.cell = seq::modify::randomize_delay(measure.cell, {0, {1}}, 0.f, 0.1f);
        measure.cell = seq::modify::randomize_gate(measure.cell, {0, {1}}, 0.9f, 1.f);
        if (i % 2 != 1)
        {
            measure.cell =
                seq::modify::randomize_intervals(measure.cell, {0, {1}}, -20, 20);

            std::get<sequence::Sequence>(measure.cell).cells[3] = seq::Sequence{{
                seq::Note{5, 0.75f, 0.f, 0.3f},
                seq::Rest{},
                seq::Note{10, 0.5f, 0.25f, 1.f},
            }};
        }
        phrase.push_back(measure);
    }
    // {0, 25, 50, 75, 100, 125, 150},
    return SequencerState{
        .phrase = phrase,
        .measure_names = {"Demo"},
        .tuning =
            {
                .intervals = {0, 200, 400, 500, 700, 900, 1100},
                .octave = 1200,
            },
        .tuning_name = "Major",
        .base_frequency = 440.f,
    };
}

} // namespace xen