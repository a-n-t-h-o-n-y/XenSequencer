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

namespace xen::catalog_detail
{
namespace
{

constexpr auto transform_policy = CommandPolicy{ProjectOperation::Edit,
                                                LibraryAccess::None,
                                                WorkspaceAccess::None,
                                                FileAccess::None,
                                                CopyBufferAccess::None,
                                                TargetRequirement::CellOrElement,
                                                RepeatPolicy::OnSuccessfulProjectChange,
                                                HistoryPolicy::Commit};
constexpr auto arp_policy = CommandPolicy{ProjectOperation::Edit,
                                          LibraryAccess::Read,
                                          WorkspaceAccess::None,
                                          FileAccess::None,
                                          CopyBufferAccess::None,
                                          TargetRequirement::CellOrElement,
                                          RepeatPolicy::OnSuccessfulProjectChange,
                                          HistoryPolicy::AmendCompatibleTransform};
constexpr auto chord_policy = CommandPolicy{ProjectOperation::Edit,
                                            LibraryAccess::Read,
                                            WorkspaceAccess::None,
                                            FileAccess::None,
                                            CopyBufferAccess::None,
                                            TargetRequirement::Cell,
                                            RepeatPolicy::OnSuccessfulProjectChange,
                                            HistoryPolicy::AmendCompatibleTransform};
constexpr auto drums_policy = CommandPolicy{ProjectOperation::Edit,
                                            LibraryAccess::Mutate,
                                            WorkspaceAccess::None,
                                            FileAccess::None,
                                            CopyBufferAccess::None,
                                            TargetRequirement::None,
                                            RepeatPolicy::OnSuccessfulProjectChange,
                                            HistoryPolicy::Commit};

} // namespace

void append_transform_specs(std::vector<CommandSpec> &specs)
{
    specs.push_back(command(
        {"stretch"}, true, "Stretch selected pattern.", transform_policy,
        std::make_tuple(repeat_count_arg("count", 2)),
        [](CommandHandlerContext &context, CommandInvocation const &invocation,
           std::size_t count) {
            auto state = context.project();
            state = increment_state(
                std::move(state), context.execution.cursor,
                require_selection(context.execution),
                [](auto target, sequence::Pattern const &pattern,
                   std::size_t repeat_count) {
                    return sequence::modify::stretch(target, pattern, repeat_count);
                },
                invocation.input.pattern, count);
            context.edit_project() = std::move(state);
            return unchanged_selection_result(
                minfo("Stretched Selection by " + std::to_string(count)),
                context.execution);
        }));

    specs.push_back(command(
        {"compress"}, true, "Compress selected pattern.", transform_policy,
        std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &invocation) {
            if (invocation.input.pattern == sequence::Pattern{0, {1}})
            {
                return make_result(
                    mwarning("Use pattern prefix to define compression."));
            }
            auto state = context.project();
            state = increment_state(
                std::move(state), context.execution.cursor,
                require_selection(context.execution),
                [](auto target, sequence::Pattern const &pattern) {
                    return sequence::modify::compress(target, pattern);
                },
                invocation.input.pattern);
            context.edit_project() = std::move(state);
            return unchanged_selection_result(minfo("Compressed Selection"),
                                              context.execution);
        }));

    specs.push_back(
        command({"shuffle"}, false, "Shuffle selected content.", transform_policy,
                std::make_tuple(),
                [](CommandHandlerContext &context, CommandInvocation const &) {
                    auto state = context.project();
                    state = increment_state(
                        std::move(state), context.execution.cursor,
                        require_selection(context.execution),
                        [](auto target) { return sequence::modify::shuffle(target); });
                    context.edit_project() = std::move(state);
                    return unchanged_selection_result(minfo("Selection Shuffled"),
                                                      context.execution);
                }));

    specs.push_back(command(
        {"rotate"}, false, "Rotate selected content.", transform_policy,
        std::make_tuple(optional_arg<int>("rotation_offset", "amount", 1)),
        [](CommandHandlerContext &context, CommandInvocation const &, int amount) {
            auto state = context.project();
            state = increment_state(
                std::move(state), context.execution.cursor,
                require_selection(context.execution),
                [](auto target, int rotation) {
                    return sequence::modify::rotate(target, rotation);
                },
                amount);
            context.edit_project() = std::move(state);
            return unchanged_selection_result(minfo("Selection Rotated"),
                                              context.execution);
        }));

    specs.push_back(
        command({"reverse"}, false, "Reverse selected content.", transform_policy,
                std::make_tuple(),
                [](CommandHandlerContext &context, CommandInvocation const &) {
                    auto state = context.project();
                    state = increment_state(
                        std::move(state), context.execution.cursor,
                        require_selection(context.execution),
                        [](auto target) { return sequence::modify::reverse(target); });
                    context.edit_project() = std::move(state);
                    return unchanged_selection_result(minfo("Selection Reversed"),
                                                      context.execution);
                }));

    specs.push_back(
        command({"mirror"}, true, "Mirror selected notes around center pitch.",
                transform_policy, std::make_tuple(note_pitch_arg("centerPitch", 0)),
                [](CommandHandlerContext &context, CommandInvocation const &invocation,
                   int center_pitch) {
                    auto state = context.project();
                    state = increment_state(
                        std::move(state), context.execution.cursor,
                        require_selection(context.execution),
                        [](auto target, sequence::Pattern const &pattern, int center) {
                            return sequence::modify::mirror(target, pattern, center);
                        },
                        invocation.input.pattern, center_pitch);
                    context.edit_project() = std::move(state);
                    return unchanged_selection_result(minfo("Selection Mirrored"),
                                                      context.execution);
                }));

    specs.push_back(command(
        {"step"}, true,
        "Apply incremental pitch/velocity offsets to selected sequence.",
        transform_policy,
        std::make_tuple(pitch_offset_arg("pitchDistance", 1),
                        velocity_offset_arg("velocityDistance", 0.f)),
        [](CommandHandlerContext &context, CommandInvocation const &invocation,
           int pitch_distance, float velocity_distance) {
            auto state = context.project();
            state = increment_state(
                std::move(state), context.execution.cursor,
                require_selection(context.execution),
                [](auto target, sequence::Pattern const &pattern, int pitch_offset,
                   float velocity_offset) {
                    return action::step(target, pattern, pitch_offset, velocity_offset);
                },
                invocation.input.pattern, pitch_distance, velocity_distance);
            context.edit_project() = std::move(state);
            return unchanged_selection_result(minfo("Stepped"), context.execution);
        }));

    specs.push_back(command(
        {"arp"}, true, "Apply chord arpeggiation to selection.", arp_policy,
        std::make_tuple(optional_arg<std::string>("chord_name", "chord", "cycle",
                                                  std::string{"\"cycle\""}),
                        optional_arg<int>("chord_inversion", "inversion", -1)),
        [](CommandHandlerContext &context, CommandInvocation const &invocation,
           std::string chord_name, int inversion) {
            auto inputs = context.prepare_transform(TransformKind::Arpeggio,
                                                    std::move(chord_name), inversion);
            auto state = std::move(inputs.baseline);
            auto const chord = find_chord(context.library().chords, inputs.chord_name);
            auto const &pitch = selected_column(state, context.execution.cursor).pitch;
            auto const intervals = invert_chord(
                chord, inputs.inversion, pitch.tuning.definition.intervals.size());
            state = increment_state(
                std::move(state), context.execution.cursor, inputs.selection,
                [](auto target, sequence::Pattern const &pattern,
                   std::vector<int> const &chord_intervals) {
                    return action::arp(target, pattern, chord_intervals);
                },
                invocation.input.pattern, intervals);
            context.edit_project() = std::move(state);
            return make_result(minfo("Arpeggiated with " + inputs.chord_name +
                                     " inversion: " + std::to_string(inputs.inversion)),
                               inputs.selection);
        }));

    specs.push_back(command(
        {"chord"}, false, "Apply chord offsets across elements in the selected cell.",
        chord_policy,
        std::make_tuple(optional_arg<std::string>("chord_name", "chord", "cycle",
                                                  std::string{"\"cycle\""}),
                        optional_arg<int>("chord_inversion", "inversion", -1)),
        [](CommandHandlerContext &context, CommandInvocation const &,
           std::string chord_name, int inversion) {
            auto inputs = context.prepare_transform(TransformKind::Chord,
                                                    std::move(chord_name), inversion);
            auto state = std::move(inputs.baseline);
            auto const chord = find_chord(context.library().chords, inputs.chord_name);
            auto const tuning_size = selected_column(state, context.execution.cursor)
                                         .pitch.tuning.definition.intervals.size();
            auto const intervals = invert_chord(chord, inputs.inversion, tuning_size);
            state = increment_state(
                std::move(state), context.execution.cursor, inputs.selection,
                [](sequence::Cell cell, std::vector<int> const &chord_intervals,
                   std::size_t size) {
                    return action::chord(std::move(cell), chord_intervals, size);
                },
                intervals, tuning_size);
            context.edit_project() = std::move(state);
            return make_result(minfo("Chorded with " + inputs.chord_name +
                                     " inversion: " + std::to_string(inputs.inversion)),
                               inputs.selection);
        }));

    specs.push_back(command(
        {"drums"}, false, "Switch to drum-oriented tuning.", drums_policy,
        std::make_tuple(
            minimum(optional_arg<std::size_t>("octave_size", "octaveSize", 16), 1.0,
                    "Must be at least 1."),
            pitch_offset_arg("offset", 1)),
        [](CommandHandlerContext &context, CommandInvocation const &,
           std::size_t requested_octave_size, int offset) {
            auto state = context.project();
            auto &pitch = selected_column(state, context.execution.cursor).pitch;
            auto const octave_size =
                std::clamp<std::size_t>(requested_octave_size, 1, 128);
            pitch.base_frequency = 440.f;
            pitch.scale = std::nullopt;
            auto const a3 = 57;
            pitch.transposition = 23 + offset - a3;
            pitch.tuning.definition = {
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
            pitch.tuning.name = "Drums (" + std::to_string(octave_size) + ")";
            context.edit_project() = std::move(state);
            return make_result(minfo("Drum Mode Active"));
        }));
}

} // namespace xen::catalog_detail
