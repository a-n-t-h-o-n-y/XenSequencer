#include "command_catalog_specs_internal.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <variant>

#include <sequence/modify.hpp>

#include <xen/actions.hpp>
#include <xen/command_dsl.hpp>
#include <xen/message_level.hpp>
#include <xen/string_manip.hpp>

#include "actions_internal.hpp"

namespace xen::catalog_detail
{
namespace
{

constexpr auto project_edit_policy =
    CommandPolicy{ProjectOperation::Edit,  LibraryAccess::None,
                  WorkspaceAccess::None,   FileAccess::None,
                  TargetRequirement::None, RepeatPolicy::OnSuccessfulProjectChange,
                  HistoryPolicy::Commit};
constexpr auto targeted_edit_policy =
    CommandPolicy{ProjectOperation::Edit,
                  LibraryAccess::None,
                  WorkspaceAccess::None,
                  FileAccess::None,
                  TargetRequirement::CellOrElement,
                  RepeatPolicy::OnSuccessfulProjectChange,
                  HistoryPolicy::Commit};
constexpr auto cell_edit_policy =
    CommandPolicy{ProjectOperation::Edit,  LibraryAccess::None,
                  WorkspaceAccess::None,   FileAccess::None,
                  TargetRequirement::Cell, RepeatPolicy::OnSuccessfulProjectChange,
                  HistoryPolicy::Commit};
constexpr auto library_read_edit_policy =
    CommandPolicy{ProjectOperation::Edit,  LibraryAccess::Read,
                  WorkspaceAccess::None,   FileAccess::None,
                  TargetRequirement::None, RepeatPolicy::OnSuccessfulProjectChange,
                  HistoryPolicy::Commit};
constexpr auto library_mutating_edit_policy =
    CommandPolicy{ProjectOperation::Edit,  LibraryAccess::Mutate,
                  WorkspaceAccess::None,   FileAccess::None,
                  TargetRequirement::None, RepeatPolicy::OnSuccessfulProjectChange,
                  HistoryPolicy::Commit};

[[nodiscard]] auto exceeds_max_measure_length(
    sequence::TimeSignature const &time_signature) -> bool
{
    if (time_signature.denominator == 0)
    {
        return true;
    }
    auto const quotient = time_signature.numerator / time_signature.denominator;
    auto const remainder = time_signature.numerator % time_signature.denominator;
    return quotient > 64 || (quotient == 64 && remainder != 0);
}

} // namespace

void append_set_and_shift_specs(std::vector<CommandSpec> &specs)
{
    specs.push_back(command(
        {"set", "pitch"}, true, "Set selected note pitches.", targeted_edit_policy,
        std::make_tuple(optional_arg<std::variant<int, Modulator>>(
            "Int|Modulator", "pitch", std::variant<int, Modulator>{0})),
        [](CommandHandlerContext &context, CommandInvocation const &invocation,
           std::variant<int, Modulator> const &pitch) {
            auto state = context.project();
            if (std::holds_alternative<int>(pitch))
            {
                state = increment_state(
                    std::move(state), require_selection(context.execution),
                    [](auto target, sequence::Pattern const &pattern, int value) {
                        return sequence::modify::set_pitch(target, pattern, value);
                    },
                    invocation.input.pattern, std::get<int>(pitch));
            }
            else
            {
                state = increment_state(
                    std::move(state), require_selection(context.execution),
                    [](auto target, sequence::Pattern const &pattern,
                       Modulator const &modulator) {
                        return action::set_pitches(target, pattern, modulator);
                    },
                    invocation.input.pattern, std::get<Modulator>(pitch));
            }
            context.edit_project() = std::move(state);
            return unchanged_selection_result(minfo("Note Set"), context.execution);
        }));

    specs.push_back(command(
        {"set", "octave"}, true, "Set selected note octaves.", targeted_edit_policy,
        std::make_tuple(optional_arg<int>("Int", "octave", 0)),
        [](CommandHandlerContext &context, CommandInvocation const &invocation,
           int octave) {
            auto state = context.project();
            state = action::set_note_octave(std::move(state),
                                            require_selection(context.execution),
                                            invocation.input.pattern, octave);
            context.edit_project() = std::move(state);
            return unchanged_selection_result(minfo("Octave Set"), context.execution);
        }));

    specs.push_back(command(
        {"set", "velocity"}, true, "Set selected note velocities.",
        {"volume", "gain", "level", "loudness"}, targeted_edit_policy,
        std::make_tuple(optional_arg<std::variant<float, Modulator>>(
            "Float|Modulator", "velocity",
            std::variant<float, Modulator>{100.f / 127.f})),
        [](CommandHandlerContext &context, CommandInvocation const &invocation,
           std::variant<float, Modulator> const &velocity) {
            auto state = context.project();
            if (std::holds_alternative<float>(velocity))
            {
                state = increment_state(
                    std::move(state), require_selection(context.execution),
                    [](auto target, sequence::Pattern const &pattern, float value) {
                        return sequence::modify::set_velocity(target, pattern, value);
                    },
                    invocation.input.pattern, std::get<float>(velocity));
            }
            else
            {
                state = increment_state(
                    std::move(state), require_selection(context.execution),
                    [](auto target, sequence::Pattern const &pattern,
                       Modulator const &modulator) {
                        return action::set_velocities(target, pattern, modulator);
                    },
                    invocation.input.pattern, std::get<Modulator>(velocity));
            }
            context.edit_project() = std::move(state);
            return unchanged_selection_result(minfo("Velocity Set"), context.execution);
        }));

    auto const set_fractional = [](auto scalar_fn, auto modulator_fn,
                                   std::string message) {
        return [scalar_fn, modulator_fn, message = std::move(message)](
                   CommandHandlerContext &context, CommandInvocation const &invocation,
                   std::variant<float, Modulator> const &value) {
            auto state = context.project();
            if (std::holds_alternative<float>(value))
            {
                state = increment_state(
                    std::move(state), require_selection(context.execution), scalar_fn,
                    invocation.input.pattern, std::get<float>(value));
            }
            else
            {
                state = increment_state(
                    std::move(state), require_selection(context.execution),
                    modulator_fn, invocation.input.pattern, std::get<Modulator>(value));
            }
            context.edit_project() = std::move(state);
            return unchanged_selection_result(minfo(message), context.execution);
        };
    };

    specs.push_back(
        command({"set", "delay"}, true, "Set selected note delays.",
                {"offset", "timing", "shift"}, targeted_edit_policy,
                std::make_tuple(optional_arg<std::variant<float, Modulator>>(
                    "Float|Modulator", "delay", std::variant<float, Modulator>{0.f})),
                set_fractional(
                    [](auto target, sequence::Pattern const &pattern, float value) {
                        return sequence::modify::set_delay(target, pattern, value);
                    },
                    [](auto target, sequence::Pattern const &pattern,
                       Modulator const &modulator) {
                        return action::set_delays(target, pattern, modulator);
                    },
                    "Delay Set")));
    specs.push_back(
        command({"set", "gate"}, true, "Set selected note gates.",
                {"duration", "length"}, targeted_edit_policy,
                std::make_tuple(optional_arg<std::variant<float, Modulator>>(
                    "Float|Modulator", "gate", std::variant<float, Modulator>{1.f})),
                set_fractional(
                    [](auto target, sequence::Pattern const &pattern, float value) {
                        return sequence::modify::set_gate(target, pattern, value);
                    },
                    [](auto target, sequence::Pattern const &pattern,
                       Modulator const &modulator) {
                        return action::set_gates(target, pattern, modulator);
                    },
                    "Gate Set")));

    specs.push_back(command(
        {"set", "measure", "timeSignature"}, false, "Set measure time signature.",
        project_edit_policy,
        std::make_tuple(optional_arg<sequence::TimeSignature>(
            "TimeSignature", "timesignature", sequence::TimeSignature{4, 4})),
        [](CommandHandlerContext &context, CommandInvocation const &,
           sequence::TimeSignature time_signature) {
            if (time_signature.denominator == 0 || time_signature.numerator == 0)
            {
                return make_result(merror("Invalid TimeSignature"));
            }
            if (exceeds_max_measure_length(time_signature))
            {
                return make_result(
                    merror("TimeSignature Too Large, Max length is 64 Whole Notes."));
            }
            auto state = context.project();
            state.measure.time_signature = time_signature;
            context.edit_project() = std::move(state);
            return make_result(minfo("Measure TimeSignature Set: " +
                                     std::to_string(time_signature.numerator) + "/" +
                                     std::to_string(time_signature.denominator)));
        }));

    specs.push_back(command(
        {"set", "baseFrequency"}, false, "Set base frequency in Hz.",
        project_edit_policy,
        std::make_tuple(optional_arg<float>("Float", "freq", 440.f)),
        [](CommandHandlerContext &context, CommandInvocation const &, float frequency) {
            auto state = context.project();
            state = action::set_base_frequency(std::move(state), frequency);
            context.edit_project() = std::move(state);
            return make_result(minfo("Base Frequency Set"));
        }));

    specs.push_back(command(
        {"set", "scale"}, false, "Set the active scale by source ID.",
        library_read_edit_policy,
        std::make_tuple(required_arg<std::string>("String", "source_id")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           std::string const &source_id) {
            auto state = context.project();
            if (source_id == "chromatic")
            {
                state.pitch.scale = std::nullopt;
                context.edit_project() = std::move(state);
                return make_result(minfo("Scale Set to chromatic."));
            }
            auto const at = std::ranges::find(context.library().scales, source_id,
                                              &LibraryScale::id);
            if (at == std::end(context.library().scales))
            {
                return make_result(merror("No Scale Found: " + source_id + "."));
            }
            validate_scale(at->definition);
            state.pitch.scale =
                ActiveScale{.source_id = at->id, .definition = at->definition};
            context.edit_project() = std::move(state);
            return make_result(minfo("Scale Set to " + source_id + "."));
        }));

    specs.push_back(command(
        {"set", "mode"}, false, "Set the active scale mode index.", project_edit_policy,
        std::make_tuple(required_arg<std::size_t>("Unsigned", "mode_index")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           std::size_t mode_index) {
            auto state = context.project();
            if (mode_index == 0 || !state.pitch.scale.has_value() ||
                mode_index > state.pitch.scale->definition.intervals.size())
            {
                return make_result(
                    merror("Invalid Mode Index. Must be in range [1, scale size)."));
            }
            state.pitch.scale->definition.mode = static_cast<std::uint8_t>(mode_index);
            validate_scale(state.pitch.scale->definition);
            context.edit_project() = std::move(state);
            return make_result(minfo("Scale Mode Set"));
        }));

    specs.push_back(command(
        {"set", "translateDirection"}, false, "Set scale translate direction.",
        project_edit_policy,
        std::make_tuple(required_arg<std::string>("String", "direction")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           std::string const &value) {
            auto const direction = to_lower(value);
            auto state = context.project();
            if (direction == "up")
            {
                state.pitch.translation_direction = TranslateDirection::Up;
            }
            else if (direction == "down")
            {
                state.pitch.translation_direction = TranslateDirection::Down;
            }
            else
            {
                return make_result(merror("Invalid TranslateDirection: " + direction));
            }
            context.edit_project() = std::move(state);
            return make_result(minfo("Translate Direction Set"));
        }));

    specs.push_back(command(
        {"set", "key"}, false, "Set transposition key.", project_edit_policy,
        std::make_tuple(optional_arg<int>("Int", "key", 0)),
        [](CommandHandlerContext &context, CommandInvocation const &, int key) {
            if (key > 127 || key < -127)
            {
                return make_result(merror("Invalid Key Value: " + std::to_string(key) +
                                          ". Must be in range [-127, 127]."));
            }
            auto state = context.project();
            state.pitch.transposition = key;
            context.edit_project() = std::move(state);
            return make_result(minfo("Key Set to " + std::to_string(key) + "."));
        }));

    specs.push_back(command(
        {"set", "weight"}, false, "Set selected cell weight.", cell_edit_policy,
        std::make_tuple(required_arg<float>("Float", "value")),
        [](CommandHandlerContext &context, CommandInvocation const &, float value) {
            auto state = context.project();
            state =
                increment_state(std::move(state), require_selection(context.execution),
                                &action::set_weight, value);
            context.edit_project() = std::move(state);
            return unchanged_selection_result(minfo("Weight Set"), context.execution);
        }));

    specs.push_back(command(
        {"set", "weights"}, true, "Set child weights in selected cell.",
        cell_edit_policy,
        std::make_tuple(
            required_arg<std::variant<float, Modulator>>("Float|Modulator", "weight")),
        [](CommandHandlerContext &context, CommandInvocation const &invocation,
           std::variant<float, Modulator> const &weight) {
            auto state = context.project();
            if (std::holds_alternative<float>(weight))
            {
                state = increment_state(
                    std::move(state), require_selection(context.execution),
                    [](auto target, sequence::Pattern const &pattern, float value) {
                        return action::set_weights(target, pattern, value);
                    },
                    invocation.input.pattern, std::get<float>(weight));
            }
            else
            {
                state = increment_state(
                    std::move(state), require_selection(context.execution),
                    [](auto target, sequence::Pattern const &pattern,
                       Modulator const &modulator) {
                        return action::set_weights(target, pattern, modulator);
                    },
                    invocation.input.pattern, std::get<Modulator>(weight));
            }
            context.edit_project() = std::move(state);
            return unchanged_selection_result(minfo("Weights Set"), context.execution);
        }));

    specs.push_back(command(
        {"double", "measure", "timeSignature"}, false, "Double measure time signature.",
        project_edit_policy, std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto state = context.project();
            auto &time_signature = state.measure.time_signature;
            if (time_signature.numerator >
                std::numeric_limits<decltype(time_signature.numerator)>::max() / 2)
            {
                return make_result(merror("Cannot Double the TimeSignature."));
            }
            auto const doubled = time_signature.numerator * 2;
            auto const candidate =
                sequence::TimeSignature{doubled, time_signature.denominator};
            if (exceeds_max_measure_length(candidate))
            {
                return make_result(
                    merror("TimeSignature Too Large, Max length is 64 Whole Notes."));
            }
            time_signature.numerator = doubled;
            context.edit_project() = std::move(state);
            return make_result(minfo("Measure TimeSignature Doubled."));
        }));

    specs.push_back(command(
        {"halve", "measure", "timeSignature"}, false, "Halve measure time signature.",
        project_edit_policy, std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto state = context.project();
            auto &time_signature = state.measure.time_signature;
            if (time_signature.numerator % 2 == 0)
            {
                time_signature.numerator /= 2;
            }
            else
            {
                if (time_signature.denominator >
                    std::numeric_limits<decltype(time_signature.denominator)>::max() /
                        2)
                {
                    return make_result(merror("Cannot Halve the TimeSignature."));
                }
                time_signature.denominator *= 2;
            }
            context.edit_project() = std::move(state);
            return make_result(minfo("Measure TimeSignature Halved."));
        }));

    auto const shift_pattern = [](auto shift_fn, std::string message) {
        return [shift_fn, message = std::move(message)](
                   CommandHandlerContext &context, CommandInvocation const &invocation,
                   auto amount) {
            auto state = context.project();
            state =
                increment_state(std::move(state), require_selection(context.execution),
                                shift_fn, invocation.input.pattern, amount);
            context.edit_project() = std::move(state);
            return unchanged_selection_result(minfo(message), context.execution);
        };
    };

    specs.push_back(command(
        {"shift", "pitch"}, true, "Shift selected note pitches.", targeted_edit_policy,
        std::make_tuple(optional_arg<int>("Int", "amount", 1)),
        shift_pattern(
            [](auto target, sequence::Pattern const &pattern, int amount) {
                return action::shift_pitch(std::move(target), pattern, amount);
            },
            "Pitch Shifted")));
    specs.push_back(command(
        {"shift", "octave"}, true, "Shift selected note octaves.", targeted_edit_policy,
        std::make_tuple(optional_arg<int>("Int", "amount", 1)),
        [](CommandHandlerContext &context, CommandInvocation const &invocation,
           int amount) {
            auto state = context.project();
            state = action::shift_octave(std::move(state),
                                         require_selection(context.execution),
                                         invocation.input.pattern, amount);
            context.edit_project() = std::move(state);
            return unchanged_selection_result(minfo("Octave Shifted"),
                                              context.execution);
        }));
    specs.push_back(command(
        {"shift", "velocity"}, true, "Shift selected note velocities.",
        targeted_edit_policy,
        std::make_tuple(optional_arg<float>("Float", "amount", 0.1f)),
        shift_pattern(
            [](auto target, sequence::Pattern const &pattern, float amount) {
                return sequence::modify::shift_velocity(target, pattern, amount);
            },
            "Velocity Shifted")));
    specs.push_back(command(
        {"shift", "delay"}, true, "Shift selected note delays.", targeted_edit_policy,
        std::make_tuple(optional_arg<float>("Float", "amount", 0.1f)),
        shift_pattern(
            [](auto target, sequence::Pattern const &pattern, float amount) {
                return sequence::modify::shift_delay(target, pattern, amount);
            },
            "Delay Shifted")));
    specs.push_back(command(
        {"shift", "gate"}, true, "Shift selected note gates.", targeted_edit_policy,
        std::make_tuple(optional_arg<float>("Float", "amount", 0.1f)),
        shift_pattern(
            [](auto target, sequence::Pattern const &pattern, float amount) {
                return sequence::modify::shift_gate(target, pattern, amount);
            },
            "Gate Shifted")));

    specs.push_back(command(
        {"shift", "scale"}, false, "Shift loaded scale index.",
        library_read_edit_policy,
        std::make_tuple(optional_arg<int>("Int", "amount", 1)),
        [](CommandHandlerContext &context, CommandInvocation const &, int amount) {
            auto state = context.project();
            auto const &library = context.library();
            auto current = std::optional<std::size_t>{};
            if (state.pitch.scale.has_value())
            {
                if (!state.pitch.scale->source_id.has_value())
                {
                    return make_result(
                        merror("Active scale has no library source ID."));
                }
                auto const at = std::ranges::find(
                    library.scales, *state.pitch.scale->source_id, &LibraryScale::id);
                if (at == library.scales.end())
                {
                    return make_result(
                        merror("Active scale source is missing from the library."));
                }
                current =
                    static_cast<std::size_t>(std::distance(library.scales.begin(), at));
            }
            auto const index =
                action::shift_scale_index(current, amount, library.scales.size());
            state.pitch.scale =
                index.has_value()
                    ? std::optional<ActiveScale>{ActiveScale{
                          .source_id = library.scales[*index].id,
                          .definition = library.scales[*index].definition}}
                    : std::nullopt;
            context.edit_project() = std::move(state);
            return make_result(minfo("Scale Shifted"));
        }));

    specs.push_back(command(
        {"shift", "scaleMode"}, false, "Shift scale mode.", project_edit_policy,
        std::make_tuple(optional_arg<int>("Int", "amount", 1)),
        [](CommandHandlerContext &context, CommandInvocation const &, int amount) {
            auto state = context.project();
            if (state.pitch.scale.has_value())
            {
                state.pitch.scale->definition =
                    action::shift_scale_mode(state.pitch.scale->definition, amount);
                context.edit_project() = std::move(state);
            }
            return make_result(minfo("Scale Mode Shifted"));
        }));

    specs.push_back(
        command({"shift", "translateDirection"}, false, "Flip translate direction.",
                project_edit_policy, std::make_tuple(),
                [](CommandHandlerContext &context, CommandInvocation const &) {
                    auto state = context.project();
                    action::flip_translate_direction(state.pitch.translation_direction);
                    context.edit_project() = std::move(state);
                    return make_result(minfo("Translate Direction Shifted"));
                }));

    specs.push_back(command(
        {"shift", "entireScale"}, false, "Shift direction, mode, and scale together.",
        library_read_edit_policy,
        std::make_tuple(optional_arg<int>("Int", "direction", 1)),
        [](CommandHandlerContext &context, CommandInvocation const &, int direction) {
            if (direction != 1 && direction != -1)
            {
                return make_result(merror("Invalid direction, must be 1 or -1"));
            }
            auto state = context.project();
            auto const &library = context.library();
            auto &translate_direction = state.pitch.translation_direction;
            if (state.pitch.scale.has_value())
            {
                if (!state.pitch.scale->source_id.has_value())
                {
                    return make_result(
                        merror("Active scale has no library source ID."));
                }
                auto at = std::ranges::find(
                    library.scales, *state.pitch.scale->source_id, &LibraryScale::id);
                if (at == library.scales.end())
                {
                    return make_result(
                        merror("Active scale source is missing from the library."));
                }
                action::flip_translate_direction(translate_direction);
                if (translate_direction == TranslateDirection::Up)
                {
                    state.pitch.scale->definition = action::shift_scale_mode(
                        state.pitch.scale->definition, direction);
                    if ((state.pitch.scale->definition.mode == 1 && direction == 1) ||
                        (state.pitch.scale->definition.mode ==
                             state.pitch.scale->definition.intervals.size() &&
                         direction == -1))
                    {
                        auto const current = static_cast<std::size_t>(
                            std::distance(library.scales.begin(), at));
                        auto const index = action::shift_scale_index(
                            current, direction, library.scales.size());
                        if (index.has_value() && *index < library.scales.size())
                        {
                            state.pitch.scale = ActiveScale{
                                .source_id = library.scales[*index].id,
                                .definition = library.scales[*index].definition,
                            };
                            if (direction == -1)
                            {
                                state.pitch.scale->definition.mode =
                                    state.pitch.scale->definition.intervals.size();
                            }
                        }
                        else
                        {
                            state.pitch.scale = std::nullopt;
                        }
                    }
                }
            }
            else if (!library.scales.empty())
            {
                auto const index = direction == 1 ? 0 : library.scales.size() - 1;
                state.pitch.scale = ActiveScale{
                    .source_id = library.scales[index].id,
                    .definition = library.scales[index].definition,
                };
                translate_direction = TranslateDirection::Up;
            }
            context.edit_project() = std::move(state);
            return make_result(minfo("Entire Scale Shifted"));
        }));

    auto const randomize = [](auto randomize_fn, std::string message) {
        return [randomize_fn, message = std::move(message)](
                   CommandHandlerContext &context, CommandInvocation const &invocation,
                   auto min, auto max) {
            auto state = context.project();
            state =
                increment_state(std::move(state), require_selection(context.execution),
                                randomize_fn, invocation.input.pattern, min, max);
            context.edit_project() = std::move(state);
            return unchanged_selection_result(minfo(message), context.execution);
        };
    };
    specs.push_back(command(
        {"randomize", "pitch"}, true, "Randomize note pitches.", targeted_edit_policy,
        std::make_tuple(optional_arg<int>("Int", "min", -12),
                        optional_arg<int>("Int", "max", 12)),
        randomize(
            [](auto target, sequence::Pattern const &pattern, int min, int max) {
                return sequence::modify::randomize_pitch(target, pattern, min, max);
            },
            "Randomized Pitch")));
    specs.push_back(command(
        {"randomize", "velocity"}, true, "Randomize note velocities.",
        targeted_edit_policy,
        std::make_tuple(optional_arg<float>("Float", "min", 0.01f),
                        optional_arg<float>("Float", "max", 1.f)),
        randomize(
            [](auto target, sequence::Pattern const &pattern, float min, float max) {
                return sequence::modify::randomize_velocity(target, pattern, min, max);
            },
            "Randomized Velocity")));
    specs.push_back(command(
        {"randomize", "delay"}, true, "Randomize note delays.", targeted_edit_policy,
        std::make_tuple(optional_arg<float>("Float", "min", 0.f),
                        optional_arg<float>("Float", "max", 0.95f)),
        randomize(
            [](auto target, sequence::Pattern const &pattern, float min, float max) {
                return sequence::modify::randomize_delay(target, pattern, min, max);
            },
            "Randomized Delay")));
    specs.push_back(command(
        {"randomize", "gate"}, true, "Randomize note gates.", targeted_edit_policy,
        std::make_tuple(optional_arg<float>("Float", "min", 0.f),
                        optional_arg<float>("Float", "max", 0.95f)),
        randomize(
            [](auto target, sequence::Pattern const &pattern, float min, float max) {
                return sequence::modify::randomize_gate(target, pattern, min, max);
            },
            "Randomized Gate")));
}

} // namespace xen::catalog_detail
