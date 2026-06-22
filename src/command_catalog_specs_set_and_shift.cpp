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
        [](PluginState &ps, CommandExecutionContext &context,
           CommandInvocation const &invocation,
           std::variant<int, Modulator> const &pitch) {
            auto state = ps.timeline.get_state();
            if (std::holds_alternative<int>(pitch))
            {
                state = increment_state(
                    std::move(state), require_selection(context),
                    [](auto target, sequence::Pattern const &pattern, int value) {
                        return sequence::modify::set_pitch(target, pattern, value);
                    },
                    invocation.input.pattern, std::get<int>(pitch));
            }
            else
            {
                state = increment_state(
                    std::move(state), require_selection(context),
                    [](auto target, sequence::Pattern const &pattern,
                       Modulator const &modulator) {
                        return action::set_pitches(target, pattern, modulator);
                    },
                    invocation.input.pattern, std::get<Modulator>(pitch));
            }
            ps.timeline.stage(std::move(state));
            return unchanged_selection_result(minfo("Note Set"), context);
        }));

    specs.push_back(command(
        {"set", "octave"}, true, "Set selected note octaves.", targeted_edit_policy,
        std::make_tuple(optional_arg<int>("Int", "octave", 0)),
        [](PluginState &ps, CommandExecutionContext &context,
           CommandInvocation const &invocation, int octave) {
            auto state = ps.timeline.get_state();
            state = action::set_note_octave(std::move(state), require_selection(context),
                                            invocation.input.pattern, octave);
            ps.timeline.stage(std::move(state));
            return unchanged_selection_result(minfo("Octave Set"), context);
        }));

    specs.push_back(command(
        {"set", "velocity"}, true, "Set selected note velocities.",
        targeted_edit_policy,
        std::make_tuple(optional_arg<std::variant<float, Modulator>>(
            "Float|Modulator", "velocity",
            std::variant<float, Modulator>{100.f / 127.f})),
        [](PluginState &ps, CommandExecutionContext &context,
           CommandInvocation const &invocation,
           std::variant<float, Modulator> const &velocity) {
            auto state = ps.timeline.get_state();
            if (std::holds_alternative<float>(velocity))
            {
                state = increment_state(
                    std::move(state), require_selection(context),
                    [](auto target, sequence::Pattern const &pattern, float value) {
                        return sequence::modify::set_velocity(target, pattern, value);
                    },
                    invocation.input.pattern, std::get<float>(velocity));
            }
            else
            {
                state = increment_state(
                    std::move(state), require_selection(context),
                    [](auto target, sequence::Pattern const &pattern,
                       Modulator const &modulator) {
                        return action::set_velocities(target, pattern, modulator);
                    },
                    invocation.input.pattern, std::get<Modulator>(velocity));
            }
            ps.timeline.stage(std::move(state));
            return unchanged_selection_result(minfo("Velocity Set"), context);
        }));

    auto const set_fractional = [](auto scalar_fn, auto modulator_fn,
                                   std::string message) {
        return [scalar_fn, modulator_fn, message = std::move(message)](
                   PluginState &ps, CommandExecutionContext &context,
                   CommandInvocation const &invocation,
                   std::variant<float, Modulator> const &value) {
            auto state = ps.timeline.get_state();
            if (std::holds_alternative<float>(value))
            {
                state =
                    increment_state(std::move(state), require_selection(context),
                                    scalar_fn,
                                    invocation.input.pattern, std::get<float>(value));
            }
            else
            {
                state = increment_state(std::move(state), require_selection(context),
                                        modulator_fn, invocation.input.pattern,
                                        std::get<Modulator>(value));
            }
            ps.timeline.stage(std::move(state));
            return unchanged_selection_result(minfo(message), context);
        };
    };

    specs.push_back(command(
        {"set", "delay"}, true, "Set selected note delays.", targeted_edit_policy,
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
        command({"set", "gate"}, true, "Set selected note gates.", targeted_edit_policy,
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
        [](PluginState &ps, CommandExecutionContext &, CommandInvocation const &,
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
            auto state = ps.timeline.get_state();
            state.measure.time_signature = time_signature;
            ps.timeline.stage(std::move(state));
            return make_result(minfo("Measure TimeSignature Set: " +
                                     std::to_string(time_signature.numerator) + "/" +
                                     std::to_string(time_signature.denominator)));
        }));

    specs.push_back(
        command({"set", "baseFrequency"}, false, "Set base frequency in Hz.",
                project_edit_policy,
                std::make_tuple(optional_arg<float>("Float", "freq", 440.f)),
                [](PluginState &ps, CommandExecutionContext &,
                   CommandInvocation const &, float frequency) {
                    auto state = ps.timeline.get_state();
                    state = action::set_base_frequency(std::move(state), frequency);
                    ps.timeline.stage(std::move(state));
                    return make_result(minfo("Base Frequency Set"));
                }));

    specs.push_back(command(
        {"set", "scale"}, false, "Set the active scale by name.",
        library_read_edit_policy,
        std::make_tuple(required_arg<std::string>("String", "name")),
        [](PluginState &ps, CommandExecutionContext &, CommandInvocation const &,
           std::string const &name) {
            auto const scale_name = to_lower(name);
            auto state = ps.timeline.get_state();
            if (scale_name == "chromatic")
            {
                state.scale = std::nullopt;
                ps.timeline.stage(std::move(state));
                return make_result(minfo("Scale Set to " + scale_name + "."));
            }
            auto const at =
                std::ranges::find(ps.library.scales, scale_name,
                                  [](Scale const &scale) { return scale.name; });
            if (at == std::end(ps.library.scales))
            {
                return make_result(merror("No Scale Found: " + scale_name + "."));
            }
            validate_scale(*at);
            state.scale = *at;
            ps.timeline.stage(std::move(state));
            return make_result(minfo("Scale Set to " + scale_name + "."));
        }));

    specs.push_back(command(
        {"set", "mode"}, false, "Set the active scale mode index.", project_edit_policy,
        std::make_tuple(required_arg<std::size_t>("Unsigned", "mode_index")),
        [](PluginState &ps, CommandExecutionContext &, CommandInvocation const &,
           std::size_t mode_index) {
            auto state = ps.timeline.get_state();
            if (mode_index == 0 || !state.scale.has_value() ||
                mode_index > state.scale->intervals.size())
            {
                return make_result(
                    merror("Invalid Mode Index. Must be in range [1, scale size)."));
            }
            state.scale->mode = static_cast<std::uint8_t>(mode_index);
            validate_scale(*state.scale);
            ps.timeline.stage(std::move(state));
            return make_result(minfo("Scale Mode Set"));
        }));

    specs.push_back(command(
        {"set", "translateDirection"}, false, "Set scale translate direction.",
        project_edit_policy,
        std::make_tuple(required_arg<std::string>("String", "direction")),
        [](PluginState &ps, CommandExecutionContext &, CommandInvocation const &,
           std::string const &value) {
            auto const direction = to_lower(value);
            auto state = ps.timeline.get_state();
            if (direction == "up")
            {
                state.scale_translate_direction = TranslateDirection::Up;
            }
            else if (direction == "down")
            {
                state.scale_translate_direction = TranslateDirection::Down;
            }
            else
            {
                return make_result(
                    merror("Invalid TranslateDirection: " + direction));
            }
            ps.timeline.stage(std::move(state));
            return make_result(minfo("Translate Direction Set"));
        }));

    specs.push_back(
        command({"set", "key"}, false, "Set transposition key.", project_edit_policy,
                std::make_tuple(optional_arg<int>("Int", "key", 0)),
                [](PluginState &ps, CommandExecutionContext &, CommandInvocation const &,
                   int key) {
                    if (key > 127 || key < -127)
                    {
                        return make_result(
                            merror("Invalid Key Value: " + std::to_string(key) +
                                   ". Must be in range [-127, 127]."));
                    }
                    auto state = ps.timeline.get_state();
                    state.key = key;
                    ps.timeline.stage(std::move(state));
                    return make_result(
                        minfo("Key Set to " + std::to_string(key) + "."));
                }));

    specs.push_back(
        command({"set", "weight"}, false, "Set selected cell weight.", cell_edit_policy,
                std::make_tuple(required_arg<float>("Float", "value")),
                [](PluginState &ps, CommandExecutionContext &context,
                   CommandInvocation const &, float value) {
                    auto state = ps.timeline.get_state();
                    state = increment_state(std::move(state), require_selection(context),
                                            &action::set_weight, value);
                    ps.timeline.stage(std::move(state));
                    return unchanged_selection_result(minfo("Weight Set"), context);
                }));

    specs.push_back(command(
        {"set", "weights"}, true, "Set child weights in selected cell.",
        cell_edit_policy,
        std::make_tuple(
            required_arg<std::variant<float, Modulator>>("Float|Modulator", "weight")),
        [](PluginState &ps, CommandExecutionContext &context,
           CommandInvocation const &invocation,
           std::variant<float, Modulator> const &weight) {
            auto state = ps.timeline.get_state();
            if (std::holds_alternative<float>(weight))
            {
                state = increment_state(
                    std::move(state), require_selection(context),
                    [](auto target, sequence::Pattern const &pattern, float value) {
                        return action::set_weights(target, pattern, value);
                    },
                    invocation.input.pattern, std::get<float>(weight));
            }
            else
            {
                state = increment_state(
                    std::move(state), require_selection(context),
                    [](auto target, sequence::Pattern const &pattern,
                       Modulator const &modulator) {
                        return action::set_weights(target, pattern, modulator);
                    },
                    invocation.input.pattern, std::get<Modulator>(weight));
            }
            ps.timeline.stage(std::move(state));
            return unchanged_selection_result(minfo("Weights Set"), context);
        }));

    specs.push_back(command(
        {"double", "measure", "timeSignature"}, false, "Double measure time signature.",
        project_edit_policy, std::make_tuple(),
        [](PluginState &ps, CommandExecutionContext &, CommandInvocation const &) {
            auto state = ps.timeline.get_state();
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
            ps.timeline.stage(std::move(state));
            return make_result(minfo("Measure TimeSignature Doubled."));
        }));

    specs.push_back(command(
        {"halve", "measure", "timeSignature"}, false, "Halve measure time signature.",
        project_edit_policy, std::make_tuple(),
        [](PluginState &ps, CommandExecutionContext &, CommandInvocation const &) {
            auto state = ps.timeline.get_state();
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
            ps.timeline.stage(std::move(state));
            return make_result(minfo("Measure TimeSignature Halved."));
        }));

    auto const shift_pattern = [](auto shift_fn, std::string message) {
        return [shift_fn, message = std::move(message)](
                   PluginState &ps, CommandExecutionContext &context,
                   CommandInvocation const &invocation, auto amount) {
            auto state = ps.timeline.get_state();
            state = increment_state(std::move(state), require_selection(context),
                                    shift_fn, invocation.input.pattern, amount);
            ps.timeline.stage(std::move(state));
            return unchanged_selection_result(minfo(message), context);
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
        [](PluginState &ps, CommandExecutionContext &context,
           CommandInvocation const &invocation, int amount) {
            auto state = ps.timeline.get_state();
            state = action::shift_octave(std::move(state), require_selection(context),
                                         invocation.input.pattern, amount);
            ps.timeline.stage(std::move(state));
            return unchanged_selection_result(minfo("Octave Shifted"), context);
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

    specs.push_back(
        command({"shift", "scale"}, false, "Shift loaded scale index.",
                library_mutating_edit_policy,
                std::make_tuple(optional_arg<int>("Int", "amount", 1)),
                [](PluginState &ps, CommandExecutionContext &,
                   CommandInvocation const &, int amount) {
                    auto state = ps.timeline.get_state();
                    auto const index = action::shift_scale_index(
                        ps.library.scale_shift_index, amount, ps.library.scales.size());
                    ps.library.scale_shift_index = index;
                    state.scale = index.has_value() && *index < ps.library.scales.size()
                                      ? std::optional<Scale>{ps.library.scales[*index]}
                                      : std::nullopt;
                    ps.timeline.stage(std::move(state));
                    return make_result(minfo("Scale Shifted"));
                }));

    specs.push_back(
        command({"shift", "scaleMode"}, false, "Shift scale mode.", project_edit_policy,
                std::make_tuple(optional_arg<int>("Int", "amount", 1)),
                [](PluginState &ps, CommandExecutionContext &,
                   CommandInvocation const &, int amount) {
                    auto state = ps.timeline.get_state();
                    if (state.scale.has_value())
                    {
                        state.scale = action::shift_scale_mode(*state.scale, amount);
                        ps.timeline.stage(std::move(state));
                    }
                    return make_result(minfo("Scale Mode Shifted"));
                }));

    specs.push_back(
        command({"shift", "translateDirection"}, false, "Flip translate direction.",
                project_edit_policy, std::make_tuple(),
                [](PluginState &ps, CommandExecutionContext &,
                   CommandInvocation const &) {
                    auto state = ps.timeline.get_state();
                    action::flip_translate_direction(state.scale_translate_direction);
                    ps.timeline.stage(std::move(state));
                    return make_result(minfo("Translate Direction Shifted"));
                }));

    specs.push_back(command(
        {"shift", "entireScale"}, false, "Shift direction, mode, and scale together.",
        library_mutating_edit_policy,
        std::make_tuple(optional_arg<int>("Int", "direction", 1)),
        [](PluginState &ps, CommandExecutionContext &, CommandInvocation const &,
           int direction) {
            if (direction != 1 && direction != -1)
            {
                return make_result(merror("Invalid direction, must be 1 or -1"));
            }
            auto state = ps.timeline.get_state();
            auto &translate_direction = state.scale_translate_direction;
            if (state.scale.has_value())
            {
                action::flip_translate_direction(translate_direction);
                if (translate_direction == TranslateDirection::Up)
                {
                    state.scale = action::shift_scale_mode(*state.scale, direction);
                    if ((state.scale->mode == 1 && direction == 1) ||
                        (state.scale->mode == state.scale->intervals.size() &&
                         direction == -1))
                    {
                        auto const index = action::shift_scale_index(
                            ps.library.scale_shift_index, direction,
                            ps.library.scales.size());
                        ps.library.scale_shift_index = index;
                        if (index.has_value() && *index < ps.library.scales.size())
                        {
                            state.scale = ps.library.scales[*index];
                            if (direction == -1)
                            {
                                state.scale->mode = state.scale->intervals.size();
                            }
                        }
                        else
                        {
                            state.scale = std::nullopt;
                        }
                    }
                }
            }
            else if (!ps.library.scales.empty())
            {
                auto const index = direction == 1 ? 0 : ps.library.scales.size() - 1;
                state.scale = ps.library.scales[index];
                translate_direction = TranslateDirection::Up;
            }
            ps.timeline.stage(std::move(state));
            return make_result(minfo("Entire Scale Shifted"));
        }));

    auto const randomize = [](auto randomize_fn, std::string message) {
        return [randomize_fn, message = std::move(message)](
                   PluginState &ps, CommandExecutionContext &context,
                   CommandInvocation const &invocation, auto min, auto max) {
            auto state = ps.timeline.get_state();
            state = increment_state(std::move(state), require_selection(context),
                                    randomize_fn, invocation.input.pattern, min, max);
            ps.timeline.stage(std::move(state));
            return unchanged_selection_result(minfo(message), context);
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
