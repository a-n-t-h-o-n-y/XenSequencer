#include "command_catalog_specs_internal.hpp"

#include <algorithm>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <sequence/modify.hpp>

#include <xen/actions.hpp>
#include <xen/chord.hpp>
#include <xen/command_dsl.hpp>
#include <xen/message_level.hpp>

#include "actions_internal.hpp"
#include "numeric.hpp"

namespace xen::catalog_detail
{
namespace
{

auto resolve_chord_cycle(std::vector<Chord> const &chords, ChordCycleState &cycle_state,
                         std::string chord_name, int inversion)
    -> std::pair<std::string, int>
{
    if (chord_name == "cycle" && inversion != -1)
    {
        chord_name = find_next_chord(chords, cycle_state.previous_chord_name).name;
        auto const chord = find_chord(chords, chord_name);
        if (chord.intervals.empty())
        {
            throw std::invalid_argument{"Chord intervals must not be empty."};
        }
        auto const last_inversion = numeric::checked_cast<int>(
            chord.intervals.size() - 1, "Chord interval count exceeds int.");
        inversion = std::min(inversion, last_inversion);
    }
    else if (chord_name != "cycle" && inversion == -1)
    {
        inversion = increment_inversion(find_chord(chords, chord_name),
                                        cycle_state.previous_inversion);
    }
    else if (chord_name == "cycle" && inversion == -1)
    {
        chord_name = cycle_state.previous_chord_name;
        inversion = chord_name.empty()
                        ? 0
                        : increment_inversion(find_chord(chords, chord_name),
                                              cycle_state.previous_inversion);
        if (inversion == 0)
        {
            chord_name = find_next_chord(chords, chord_name).name;
        }
    }
    return {std::move(chord_name), inversion};
}

} // namespace

void append_transform_specs(std::vector<CommandSpec> &specs)
{
    specs.push_back(command(
        {"stretch"}, true, "Stretch selected pattern.",
        std::make_tuple(optional_arg<std::size_t>("Unsigned", "count", 2)),
        [](PluginState &ps, CommandInvocation const &invocation, std::size_t count) {
            auto state = ps.timeline.get_state();
            state = increment_state(
                std::move(state), ps.editor,
                [](auto target, sequence::Pattern const &pattern,
                   std::size_t repeat_count) {
                    return sequence::modify::stretch(target, pattern, repeat_count);
                },
                invocation.input.pattern, count);
            ps.timeline.stage(std::move(state));
            return minfo("Stretched Selection by " + std::to_string(count));
        }));

    specs.push_back(
        command({"compress"}, true, "Compress selected pattern.", std::make_tuple(),
                [](PluginState &ps, CommandInvocation const &invocation) {
                    if (invocation.input.pattern == sequence::Pattern{0, {1}})
                    {
                        return mwarning("Use pattern prefix to define compression.");
                    }
                    auto state = ps.timeline.get_state();
                    state = increment_state(
                        std::move(state), ps.editor,
                        [](auto target, sequence::Pattern const &pattern) {
                            return sequence::modify::compress(target, pattern);
                        },
                        invocation.input.pattern);
                    ps.timeline.stage(std::move(state));
                    return minfo("Compressed Selection");
                }));

    specs.push_back(command(
        {"shuffle"}, false, "Shuffle selected content.", std::make_tuple(),
        [](PluginState &ps, CommandInvocation const &) {
            auto state = ps.timeline.get_state();
            state = increment_state(std::move(state), ps.editor, [](auto target) {
                return sequence::modify::shuffle(target);
            });
            ps.timeline.stage(std::move(state));
            return minfo("Selection Shuffled");
        }));

    specs.push_back(command({"rotate"}, false, "Rotate selected content.",
                            std::make_tuple(optional_arg<int>("Int", "amount", 1)),
                            [](PluginState &ps, CommandInvocation const &, int amount) {
                                auto state = ps.timeline.get_state();
                                state = increment_state(
                                    std::move(state), ps.editor,
                                    [](auto target, int rotation) {
                                        return sequence::modify::rotate(target,
                                                                        rotation);
                                    },
                                    amount);
                                ps.timeline.stage(std::move(state));
                                return minfo("Selection Rotated");
                            }));

    specs.push_back(command(
        {"reverse"}, false, "Reverse selected content.", std::make_tuple(),
        [](PluginState &ps, CommandInvocation const &) {
            auto state = ps.timeline.get_state();
            state = increment_state(std::move(state), ps.editor, [](auto target) {
                return sequence::modify::reverse(target);
            });
            ps.timeline.stage(std::move(state));
            return minfo("Selection Reversed");
        }));

    specs.push_back(command(
        {"mirror"}, true, "Mirror selected notes around center pitch.",
        std::make_tuple(optional_arg<int>("Int", "centerPitch", 0)),
        [](PluginState &ps, CommandInvocation const &invocation, int center_pitch) {
            auto state = ps.timeline.get_state();
            state = increment_state(
                std::move(state), ps.editor,
                [](auto target, sequence::Pattern const &pattern, int center) {
                    return sequence::modify::mirror(target, pattern, center);
                },
                invocation.input.pattern, center_pitch);
            ps.timeline.stage(std::move(state));
            return minfo("Selection Mirrored");
        }));

    specs.push_back(command(
        {"step"}, true,
        "Apply incremental pitch/velocity offsets to selected sequence.",
        std::make_tuple(optional_arg<int>("Int", "pitchDistance", 1),
                        optional_arg<float>("Float", "velocityDistance", 0.f)),
        [](PluginState &ps, CommandInvocation const &invocation, int pitch_distance,
           float velocity_distance) {
            auto state = ps.timeline.get_state();
            state = increment_state(
                std::move(state), ps.editor,
                [](auto target, sequence::Pattern const &pattern, int pitch_offset,
                   float velocity_offset) {
                    return action::step(target, pattern, pitch_offset, velocity_offset);
                },
                invocation.input.pattern, pitch_distance, velocity_distance);
            ps.timeline.stage(std::move(state));
            return minfo("Stepped");
        }));

    specs.push_back(command(
        {"arp"}, true, "Apply chord arpeggiation to selection.",
        std::make_tuple(optional_arg<std::string>("String", "chord", "cycle",
                                                  std::string{"\"cycle\""}),
                        optional_arg<int>("Int", "inversion", -1)),
        [](PluginState &ps, CommandInvocation const &invocation, std::string chord_name,
           int inversion) {
            auto state = ps.timeline.get_state();
            bool const starting_new_chain =
                ps.editor.selected != ps.editor.arp_state.selected ||
                ps.editor.arp_state.previous_project_revision !=
                    ps.timeline.get_project_revision();
            if (starting_new_chain)
            {
                ps.editor.arp_state.sequencer = state;
                ps.editor.arp_state.selected = ps.editor.selected;
            }
            std::tie(chord_name, inversion) =
                resolve_chord_cycle(ps.library.chords, ps.editor.arp_state,
                                    std::move(chord_name), inversion);
            ps.editor.arp_state.previous_chord_name = chord_name;
            ps.editor.arp_state.previous_inversion = inversion;
            ps.editor.arp_state.previous_project_revision =
                ps.timeline.get_project_revision();
            state = ps.editor.arp_state.sequencer;
            ps.editor.selected = ps.editor.arp_state.selected;
            auto const chord = find_chord(ps.library.chords, chord_name);
            auto const intervals =
                invert_chord(chord, inversion, state.tuning.intervals.size());
            state = increment_state(
                std::move(state), ps.editor,
                [](auto target, sequence::Pattern const &pattern,
                   std::vector<int> const &chord_intervals) {
                    return action::arp(target, pattern, chord_intervals);
                },
                invocation.input.pattern, intervals);
            ps.timeline.stage(std::move(state));
            return minfo("Arpeggiated with " + chord_name +
                         " inversion: " + std::to_string(inversion));
        }));

    specs.push_back(command(
        {"chord"}, false, "Apply chord offsets across elements in the selected cell.",
        std::make_tuple(optional_arg<std::string>("String", "chord", "cycle",
                                                  std::string{"\"cycle\""}),
                        optional_arg<int>("Int", "inversion", -1)),
        [](PluginState &ps, CommandInvocation const &, std::string chord_name,
           int inversion) {
            auto state = ps.timeline.get_state();
            bool const starting_new_chain =
                ps.editor.selected != ps.editor.chord_state.selected ||
                ps.editor.chord_state.previous_project_revision !=
                    ps.timeline.get_project_revision();
            if (starting_new_chain)
            {
                ps.editor.chord_state.sequencer = state;
                ps.editor.chord_state.selected = ps.editor.selected;
            }
            std::tie(chord_name, inversion) =
                resolve_chord_cycle(ps.library.chords, ps.editor.chord_state,
                                    std::move(chord_name), inversion);
            ps.editor.chord_state.previous_chord_name = chord_name;
            ps.editor.chord_state.previous_inversion = inversion;
            ps.editor.chord_state.previous_project_revision =
                ps.timeline.get_project_revision();
            state = ps.editor.chord_state.sequencer;
            ps.editor.selected = ps.editor.chord_state.selected;
            auto const chord = find_chord(ps.library.chords, chord_name);
            auto const tuning_size = state.tuning.intervals.size();
            auto const intervals = invert_chord(chord, inversion, tuning_size);
            state = increment_state(
                std::move(state), ps.editor,
                [](sequence::Cell cell, std::vector<int> const &chord_intervals,
                   std::size_t size) {
                    return action::chord(std::move(cell), chord_intervals, size);
                },
                intervals, tuning_size);
            ps.timeline.stage(std::move(state));
            return minfo("Chorded with " + chord_name +
                         " inversion: " + std::to_string(inversion));
        }));

    specs.push_back(
        command({"drums"}, false, "Switch to drum-oriented tuning.",
                std::make_tuple(optional_arg<std::size_t>("Unsigned", "octaveSize", 16),
                                optional_arg<int>("Int", "offset", 1)),
                [](PluginState &ps, CommandInvocation const &,
                   std::size_t requested_octave_size, int offset) {
                    auto state = ps.timeline.get_state();
                    auto const octave_size =
                        std::clamp<std::size_t>(requested_octave_size, 1, 128);
                    state.base_frequency = 440.f;
                    state.scale = std::nullopt;
                    ps.library.scale_shift_index = std::nullopt;
                    auto const a3 = 57;
                    state.key = 23 + offset - a3;
                    state.tuning = {
                        .intervals =
                            [octave_size] {
                                auto intervals = std::vector<float>{};
                                for (auto i = std::size_t{0}; i < octave_size; ++i)
                                {
                                    intervals.push_back(100.f * static_cast<float>(i));
                                }
                                return intervals;
                            }(),
                        .octave = 100.f * static_cast<float>(octave_size),
                        .description = "",
                    };
                    state.tuning_name = "Drums (" + std::to_string(octave_size) + ")";
                    ps.timeline.stage(std::move(state));
                    return minfo("Drum Mode Active");
                }));
}

} // namespace xen::catalog_detail
