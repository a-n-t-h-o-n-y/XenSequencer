#include <xen/command_action.hpp>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <sequence/modify.hpp>
#include <sequence/sequence.hpp>

#include <xen/actions.hpp>
#include <xen/chord.hpp>
#include <xen/constants.hpp>
#include <xen/copy_paste.hpp>
#include <xen/message_level.hpp>
#include <xen/selection.hpp>
#include <xen/string_manip.hpp>

namespace xen
{
namespace
{

template <class>
inline constexpr bool always_false_v = false;

} // namespace

auto is_again_action(CommandAction const &action) -> bool
{
    return std::holds_alternative<AgainAction>(action);
}

auto execute_command_action(PluginState &ps, ExecutionContext context,
                            CommandAction const &action)
    -> CommandActionResult
{
    auto staged_state = ps.timeline.get_state();
    staged_state.aux = context;
    ps.timeline.stage(std::move(staged_state));

    auto const engine_before = ps.timeline.get_state().sequencer;
    ps.commit_intent = CommitIntent::Auto;

    auto status = std::visit(
        [&](auto const &typed_action) -> std::pair<MessageLevel, std::string> {
            using ActionType = std::decay_t<decltype(typed_action)>;
            if constexpr (std::is_same_v<ActionType, MoveSelectionAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;

                switch (typed_action.direction)
                {
                case MoveDirection::Left:
                    state.aux = action::move_left(state.sequencer, std::move(state.aux),
                                                  typed_action.amount);
                    ps.timeline.stage(std::move(state));
                    return mdebug("Moved Left " +
                                  std::to_string(typed_action.amount) + " Times");
                case MoveDirection::Right:
                    state.aux = action::move_right(
                        state.sequencer, std::move(state.aux), typed_action.amount);
                    ps.timeline.stage(std::move(state));
                    return mdebug("Moved Right " +
                                  std::to_string(typed_action.amount) + " Times");
                case MoveDirection::Up:
                    state.aux =
                        action::move_up(std::move(state.aux), typed_action.amount);
                    ps.timeline.stage(std::move(state));
                    return mdebug("Moved Up " +
                                  std::to_string(typed_action.amount) + " Times");
                case MoveDirection::Down:
                    state.aux = action::move_down(
                        state.sequencer, std::move(state.aux), typed_action.amount);
                    ps.timeline.stage(std::move(state));
                    return mdebug("Moved Down " +
                                  std::to_string(typed_action.amount) + " Times");
                }

                throw std::runtime_error("Unhandled move direction");
            }
            else if constexpr (std::is_same_v<ActionType, WelcomeAction>)
            {
                return minfo(std::string{"Welcome to XenSequencer v"} + VERSION);
            }
            else if constexpr (std::is_same_v<ActionType, VersionAction>)
            {
                return minfo(std::string{"v"} + VERSION);
            }
            else if constexpr (std::is_same_v<ActionType, ResetAction>)
            {
                ps.timeline.stage({EngineState{}, EditorSessionState{}});
                ps.library.scale_shift_index = std::nullopt;
                return minfo("XenSequencer Reset");
            }
            else if constexpr (std::is_same_v<ActionType, AgainAction>)
            {
                return merror("Internal error: unexpanded 'again' action.");
            }
            else if constexpr (std::is_same_v<ActionType, SetKeyAction>)
            {
                if (typed_action.key > 127 || typed_action.key < -127)
                {
                    return merror("Invalid Key Value: " +
                                  std::to_string(typed_action.key) +
                                  ". Must be in range [-127, 127].");
                }

                auto state = ps.timeline.get_state();
                state.aux = context;
                state.sequencer.key = typed_action.key;
                ps.timeline.stage(std::move(state));
                return minfo("Key Set to " + std::to_string(typed_action.key) + ".");
            }
            else if constexpr (std::is_same_v<ActionType, SetSequenceNameAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;

                auto index = typed_action.index;
                index = (index == -1) ? (int)state.aux.selected.measure : index;
                if (index < 0 || index >= (int)state.sequencer.sequence_names.size())
                {
                    return merror("Invalid Sequence Index");
                }

                state.sequencer.sequence_names[(std::size_t)index] = typed_action.name;
                ps.timeline.stage(std::move(state));
                return minfo("Sequence Name Set");
            }
            else if constexpr (std::is_same_v<ActionType,
                                               SetSequenceTimeSignatureAction>)
            {
                if (typed_action.time_signature.denominator == 0 ||
                    typed_action.time_signature.numerator == 0)
                {
                    return merror("Invalid TimeSignature");
                }
                if ((float)typed_action.time_signature.numerator /
                        (float)typed_action.time_signature.denominator >
                    64.f)
                {
                    return merror(
                        "TimeSignature Too Large, Max length is 64 Whole Notes.");
                }

                auto state = ps.timeline.get_state();
                state.aux = context;

                auto index = typed_action.index;
                index = (index == -1) ? (int)state.aux.selected.measure : index;
                if (index < 0 || index >= (int)state.sequencer.sequence_bank.size())
                {
                    return merror("Invalid Sequence Index");
                }

                state.sequencer.sequence_bank[(std::size_t)index].time_signature =
                    typed_action.time_signature;
                ps.timeline.stage(std::move(state));
                return minfo("TimeSignature Set: " +
                             std::to_string(typed_action.time_signature.numerator) +
                             "/" +
                             std::to_string(typed_action.time_signature.denominator));
            }
            else if constexpr (std::is_same_v<ActionType, SelectSequenceAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;

                if (state.aux.selected.measure == (std::size_t)typed_action.index)
                {
                    return mdebug("Already Selected");
                }

                state.aux = action::set_selected_sequence(state.aux, typed_action.index);
                ps.timeline.stage(std::move(state));
                return mdebug("Sequence " + std::to_string(typed_action.index) +
                              " Selected");
            }
            else if constexpr (std::is_same_v<ActionType, SetInputModeAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state.aux = action::set_input_mode(std::move(state.aux),
                                                   typed_action.mode);
                ps.timeline.stage(std::move(state));
                return minfo("Input Mode Set to " +
                             single_quote(to_string(typed_action.mode)));
            }
            else if constexpr (std::is_same_v<ActionType, LoadSequenceBankAction>)
            {
                auto const cd = ps.config.current_sequence_directory;
                if (!cd.isDirectory())
                {
                    return merror("Invalid Current Sequence Directory");
                }

                auto const filepath =
                    cd.getChildFile(typed_action.filename + ".xss");
                if (!filepath.exists())
                {
                    return merror("File Not Found: " +
                                  filepath.getFullPathName().toStdString());
                }

                auto state = ps.timeline.get_state();
                state.aux = context;

                auto [sb, names] = action::load_sequence_bank(filepath);
                state.sequencer.sequence_bank = std::move(sb);
                state.sequencer.sequence_names = std::move(names);

                ps.timeline.stage(std::move(state));
                return minfo("Sequence Bank Loaded");
            }
            else if constexpr (std::is_same_v<ActionType, LoadTuningAction>)
            {
                auto const cd = ps.config.current_tuning_directory;
                if (!cd.isDirectory())
                {
                    return merror("Invalid Current Tuning Library Directory");
                }

                auto const filepath =
                    cd.getChildFile(typed_action.filename + ".scl");
                if (!filepath.exists())
                {
                    return merror("File Not Found: " +
                                  filepath.getFullPathName().toStdString());
                }

                auto state = ps.timeline.get_state();
                state.aux = context;
                state.sequencer.tuning_name =
                    filepath.getFileNameWithoutExtension().toStdString();
                state.sequencer.tuning =
                    sequence::from_scala(filepath.getFullPathName().toStdString());

                ps.timeline.stage(std::move(state));
                return minfo("Tuning Loaded");
            }
            else if constexpr (std::is_same_v<ActionType, LoadKeysAction>)
            {
                return mwarning(
                    "Command 'load keys' is deprecated and has no effect.");
            }
            else if constexpr (std::is_same_v<ActionType, LoadScalesAction>)
            {
                ps.library.scales = load_scales_from_files();
                return minfo("Scales Loaded: " +
                             std::to_string(ps.library.scales.size()));
            }
            else if constexpr (std::is_same_v<ActionType, LoadChordsAction>)
            {
                ps.library.chords = load_chords_from_files();
                return minfo("Chords Loaded: " +
                             std::to_string(ps.library.chords.size()));
            }
            else if constexpr (std::is_same_v<ActionType, CreateNoteAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto const &selected, int pitch, float velocity, float delay,
                       float gate) {
                        auto note = sequence::modify::note(pitch, velocity, delay, gate);
                        using Selected = std::decay_t<decltype(selected)>;
                        if constexpr (std::is_same_v<Selected, sequence::Cell>)
                        {
                            return sequence::Cell{
                                .elements = {std::move(note)},
                                .weight = selected.weight,
                            };
                        }
                        else
                        {
                            return note;
                        }
                    },
                    typed_action.pitch, typed_action.velocity, typed_action.delay,
                    typed_action.gate);
                ps.timeline.stage(std::move(state));
                return minfo("Note Created");
            }
            else if constexpr (std::is_same_v<ActionType, CopySelectionAction>)
            {
                auto const state = ps.timeline.get_state();
                action::copy(state.sequencer, context);
                return minfo("Copied Selection");
            }
            else if constexpr (std::is_same_v<ActionType, CutSelectionAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                action::copy(state.sequencer, state.aux);
                state = action::delete_cell(std::move(state));
                ps.timeline.stage(std::move(state));
                return minfo("Selection Cut");
            }
            else if constexpr (std::is_same_v<ActionType, PasteSelectionAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                auto const content = read_copy_buffer();
                state.sequencer =
                    action::paste(std::move(state.sequencer), state.aux);
                if (content.has_value() && has_selected_element(state.aux.selected) &&
                    std::holds_alternative<sequence::Cell>(*content))
                {
                    state.aux.selected.element_index.reset();
                }
                ps.timeline.stage(std::move(state));
                return minfo("Selection Pasted Over");
            }
            else if constexpr (std::is_same_v<ActionType, DuplicateSelectionAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = action::duplicate(std::move(state));
                ps.timeline.stage(std::move(state));
                return minfo("Selection Duplicated");
            }
            else if constexpr (std::is_same_v<ActionType, DeleteSelectionAction>)
            {
                ps.timeline.stage(action::delete_cell(ps.timeline.get_state()));
                return minfo("Deleted Selection");
            }
            else if constexpr (std::is_same_v<ActionType, SplitSelectionAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto target, std::size_t count) {
                        return sequence::modify::repeat(target, count);
                    },
                    typed_action.count);
                ps.timeline.stage(std::move(state));
                return minfo("Split Selection " +
                             std::to_string(typed_action.count) + " Times");
            }
            else if constexpr (std::is_same_v<ActionType, LiftSelectionAction>)
            {
                ps.timeline.stage(action::lift(ps.timeline.get_state()));
                return minfo("Selection Lifted One Layer");
            }
            else if constexpr (std::is_same_v<ActionType, SetBaseFrequencyAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state.sequencer =
                    action::set_base_frequency(std::move(state.sequencer),
                                               typed_action.freq);
                ps.timeline.stage(std::move(state));
                return minfo("Base Frequency Set");
            }
            else if constexpr (std::is_same_v<ActionType, SaveSequenceBankAction>)
            {
                auto const cd = ps.config.current_sequence_directory;
                if (!cd.isDirectory())
                {
                    return merror("Invalid Current Sequence Directory");
                }

                auto const filepath =
                    cd.getChildFile(typed_action.filename + ".xss");
                auto const state = ps.timeline.get_state();
                action::save_sequence_bank(state.sequencer.sequence_bank,
                                           state.sequencer.sequence_names,
                                           filepath);
                return minfo("Sequence Bank Saved to " +
                             single_quote(filepath.getFullPathName().toStdString()));
            }
            else if constexpr (std::is_same_v<ActionType, LibraryDirectoryAction>)
            {
                return minfo(
                    get_user_library_directory().getFullPathName().toStdString());
            }
            else if constexpr (std::is_same_v<ActionType, SetPitchAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;

                if (std::holds_alternative<int>(typed_action.pitch))
                {
                    state = increment_state(
                        std::move(state),
                        [](auto target, sequence::Pattern const &pattern, int pitch) {
                            return sequence::modify::set_pitch(target, pattern, pitch);
                        },
                        typed_action.pattern, std::get<int>(typed_action.pitch));
                }
                else
                {
                    state = increment_state(
                        std::move(state),
                        [](auto target, sequence::Pattern const &pattern,
                           Modulator const &mod) {
                            return action::set_pitches(target, pattern, mod);
                        },
                        typed_action.pattern, std::get<Modulator>(typed_action.pitch));
                    ps.commit_intent = CommitIntent::Defer;
                }

                ps.timeline.stage(std::move(state));
                return minfo("Note Set");
            }
            else if constexpr (std::is_same_v<ActionType, SetOctaveAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state.sequencer = action::set_note_octave(
                    std::move(state.sequencer), state.aux, typed_action.pattern,
                    typed_action.octave);
                ps.timeline.stage(std::move(state));
                return minfo("Octave Set");
            }
            else if constexpr (std::is_same_v<ActionType, SetVelocityAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;

                if (std::holds_alternative<float>(typed_action.velocity))
                {
                    state = increment_state(
                        std::move(state),
                        [](auto target, sequence::Pattern const &pattern,
                           float velocity) {
                            return sequence::modify::set_velocity(target, pattern,
                                                                 velocity);
                        },
                        typed_action.pattern,
                        std::get<float>(typed_action.velocity));
                }
                else
                {
                    state = increment_state(
                        std::move(state),
                        [](auto target, sequence::Pattern const &pattern,
                           Modulator const &mod) {
                            return action::set_velocities(target, pattern, mod);
                        },
                        typed_action.pattern,
                        std::get<Modulator>(typed_action.velocity));
                    ps.commit_intent = CommitIntent::Defer;
                }

                ps.timeline.stage(std::move(state));
                return minfo("Velocity Set");
            }
            else if constexpr (std::is_same_v<ActionType, SetDelayAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;

                if (std::holds_alternative<float>(typed_action.delay))
                {
                    state = increment_state(
                        std::move(state),
                        [](auto target, sequence::Pattern const &pattern, float delay) {
                            return sequence::modify::set_delay(target, pattern, delay);
                        },
                        typed_action.pattern, std::get<float>(typed_action.delay));
                }
                else
                {
                    state = increment_state(
                        std::move(state),
                        [](auto target, sequence::Pattern const &pattern,
                           Modulator const &mod) {
                            return action::set_delays(target, pattern, mod);
                        },
                        typed_action.pattern,
                                            std::get<Modulator>(typed_action.delay));
                    ps.commit_intent = CommitIntent::Defer;
                }

                ps.timeline.stage(std::move(state));
                return minfo("Delay Set");
            }
            else if constexpr (std::is_same_v<ActionType, SetGateAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;

                if (std::holds_alternative<float>(typed_action.gate))
                {
                    state = increment_state(
                        std::move(state),
                        [](auto target, sequence::Pattern const &pattern, float gate) {
                            return sequence::modify::set_gate(target, pattern, gate);
                        },
                        typed_action.pattern, std::get<float>(typed_action.gate));
                }
                else
                {
                    state = increment_state(
                        std::move(state),
                        [](auto target, sequence::Pattern const &pattern,
                           Modulator const &mod) {
                            return action::set_gates(target, pattern, mod);
                        },
                        typed_action.pattern,
                                            std::get<Modulator>(typed_action.gate));
                    ps.commit_intent = CommitIntent::Defer;
                }

                ps.timeline.stage(std::move(state));
                return minfo("Gate Set");
            }
            else if constexpr (std::is_same_v<ActionType, SetWeightAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(std::move(state), &action::set_weight,
                                        typed_action.value);
                ps.timeline.stage(std::move(state));
                return minfo("Weight Set");
            }
            else if constexpr (std::is_same_v<ActionType, SetWeightsAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;

                if (std::holds_alternative<float>(typed_action.weight))
                {
                    state = increment_state(
                        std::move(state),
                        [](auto target, sequence::Pattern const &pattern,
                           float weight) {
                            return action::set_weights(target, pattern, weight);
                        },
                        typed_action.pattern, std::get<float>(typed_action.weight));
                }
                else
                {
                    state = increment_state(
                        std::move(state),
                        [](auto target, sequence::Pattern const &pattern,
                           Modulator const &mod) {
                            return action::set_weights(target, pattern, mod);
                        },
                        typed_action.pattern,
                        std::get<Modulator>(typed_action.weight));
                }

                ps.timeline.stage(std::move(state));
                ps.commit_intent = CommitIntent::Defer;
                return minfo("Weights Set");
            }
            else if constexpr (std::is_same_v<ActionType, CommitAction>)
            {
                ps.commit_intent = CommitIntent::Force;
                return mdebug("commit made");
            }
            else if constexpr (std::is_same_v<ActionType, UndoAction>)
            {
                ps.timeline.reset_stage();
                auto current_aux = ps.timeline.get_state().aux;
                if (ps.timeline.undo())
                {
                    auto new_state = ps.timeline.get_state();
                    new_state.aux.selected = std::move(current_aux.selected);
                    new_state.aux.input_mode = current_aux.input_mode;
                    ps.timeline.stage(new_state);
                    return minfo("Undone");
                }

                return minfo("Nothing to undo.");
            }
            else if constexpr (std::is_same_v<ActionType, RedoAction>)
            {
                return ps.timeline.redo() ? minfo("Redone")
                                          : minfo("Nothing to redo.");
            }
            else if constexpr (std::is_same_v<ActionType, SetScaleAction>)
            {
                auto scale_name = to_lower(typed_action.name);
                auto state = ps.timeline.get_state();
                state.aux = context;

                if (scale_name == "chromatic")
                {
                    state.sequencer.scale = std::nullopt;
                    ps.timeline.stage(std::move(state));
                    return minfo("Scale Set to " + scale_name + ".");
                }

                auto const at = std::ranges::find(
                    ps.library.scales, scale_name,
                    [](Scale const &scale) { return scale.name; });
                if (at != std::end(ps.library.scales))
                {
                    state.sequencer.scale = *at;
                    ps.timeline.stage(std::move(state));
                    return minfo("Scale Set to " + scale_name + ".");
                }

                return merror("No Scale Found: " + scale_name + ".");
            }
            else if constexpr (std::is_same_v<ActionType, SetScaleModeAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;

                if (typed_action.mode_index == 0 ||
                    !state.sequencer.scale.has_value() ||
                    typed_action.mode_index >
                        state.sequencer.scale->intervals.size())
                {
                    return merror("Invalid Mode Index. Must be in range [1, "
                                  "scale size).");
                }

                state.sequencer.scale->mode =
                    (std::uint8_t)typed_action.mode_index;
                ps.timeline.stage(std::move(state));
                return minfo("Scale Mode Set");
            }
            else if constexpr (std::is_same_v<ActionType,
                                               SetTranslateDirectionAction>)
            {
                auto direction = to_lower(typed_action.direction);
                auto state = ps.timeline.get_state();
                state.aux = context;
                if (direction == "up")
                {
                    state.sequencer.scale_translate_direction =
                        TranslateDirection::Up;
                }
                else if (direction == "down")
                {
                    state.sequencer.scale_translate_direction =
                        TranslateDirection::Down;
                }
                else
                {
                    return merror("Invalid TranslateDirection: " + direction);
                }

                ps.timeline.stage(std::move(state));
                return minfo("Translate Direction Set");
            }
            else if constexpr (std::is_same_v<ActionType, ShiftPitchAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto target, sequence::Pattern const &pattern, int amount) {
                        return sequence::modify::shift_pitch(target, pattern, amount);
                    },
                    typed_action.pattern, typed_action.amount);
                ps.timeline.stage(std::move(state));
                return minfo("Pitch Shifted");
            }
            else if constexpr (std::is_same_v<ActionType, ShiftOctaveAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state.sequencer = action::shift_octave(std::move(state.sequencer),
                                                       state.aux,
                                                       typed_action.pattern,
                                                       typed_action.amount);
                ps.timeline.stage(std::move(state));
                return minfo("Octave Shifted");
            }
            else if constexpr (std::is_same_v<ActionType, ShiftVelocityAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto target, sequence::Pattern const &pattern, float amount) {
                        return sequence::modify::shift_velocity(target, pattern,
                                                                amount);
                    },
                    typed_action.pattern, typed_action.amount);
                ps.timeline.stage(std::move(state));
                return minfo("Velocity Shifted");
            }
            else if constexpr (std::is_same_v<ActionType, ShiftDelayAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto target, sequence::Pattern const &pattern, float amount) {
                        return sequence::modify::shift_delay(target, pattern, amount);
                    },
                    typed_action.pattern, typed_action.amount);
                ps.timeline.stage(std::move(state));
                return minfo("Delay Shifted");
            }
            else if constexpr (std::is_same_v<ActionType, ShiftGateAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto target, sequence::Pattern const &pattern, float amount) {
                        return sequence::modify::shift_gate(target, pattern, amount);
                    },
                    typed_action.pattern, typed_action.amount);
                ps.timeline.stage(std::move(state));
                return minfo("Gate Shifted");
            }
            else if constexpr (std::is_same_v<ActionType,
                                               ShiftSelectedSequenceAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                auto const size = (int)state.sequencer.sequence_bank.size();
                auto const index = (((int)state.aux.selected.measure +
                                     typed_action.amount) %
                                        size +
                                    size) %
                                   size;
                state.aux = action::set_selected_sequence(state.aux, index);
                ps.timeline.stage(std::move(state));
                return mdebug("Selected Sequence Shifted");
            }
            else if constexpr (std::is_same_v<ActionType, ShiftScaleAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                auto const index = action::shift_scale_index(
                    ps.library.scale_shift_index, typed_action.amount,
                    ps.library.scales.size());
                ps.library.scale_shift_index = index;
                if (index.has_value() && *index < ps.library.scales.size())
                {
                    state.sequencer.scale = ps.library.scales[*index];
                }
                else
                {
                    state.sequencer.scale = std::nullopt; // Chromatic
                }
                ps.timeline.stage(std::move(state));
                return minfo("Scale Shifted");
            }
            else if constexpr (std::is_same_v<ActionType, ShiftScaleModeAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                if (state.sequencer.scale.has_value())
                {
                    state.sequencer.scale =
                        action::shift_scale_mode(*state.sequencer.scale,
                                                 typed_action.amount);
                    ps.timeline.stage(std::move(state));
                }
                return minfo("Scale Mode Shifted");
            }
            else if constexpr (std::is_same_v<ActionType,
                                               ShiftTranslateDirectionAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                action::flip_translate_direction(
                    state.sequencer.scale_translate_direction);
                ps.timeline.stage(std::move(state));
                return minfo("Translate Direction Shifted");
            }
            else if constexpr (std::is_same_v<ActionType, ShiftEntireScaleAction>)
            {
                if (typed_action.direction != 1 && typed_action.direction != -1)
                {
                    return merror("Invalid direction, must be 1 or -1");
                }

                auto state = ps.timeline.get_state();
                state.aux = context;
                auto &td = state.sequencer.scale_translate_direction;
                if (state.sequencer.scale.has_value())
                {
                    action::flip_translate_direction(td);
                    if (td == TranslateDirection::Up)
                    {
                        state.sequencer.scale =
                            action::shift_scale_mode(*state.sequencer.scale,
                                                     typed_action.direction);
                        if ((state.sequencer.scale->mode == 1 &&
                             typed_action.direction == 1) ||
                            (state.sequencer.scale->mode ==
                                 state.sequencer.scale->intervals.size() &&
                             typed_action.direction == -1))
                        {
                            auto const index = action::shift_scale_index(
                                ps.library.scale_shift_index,
                                typed_action.direction,
                                ps.library.scales.size());
                            ps.library.scale_shift_index = index;
                            if (index.has_value() &&
                                *index < ps.library.scales.size())
                            {
                                state.sequencer.scale = ps.library.scales[*index];
                                if (typed_action.direction == -1)
                                {
                                    state.sequencer.scale->mode =
                                        state.sequencer.scale->intervals.size();
                                }
                            }
                            else
                            {
                                state.sequencer.scale = std::nullopt;
                            }
                        }
                    }
                }
                else if (!ps.library.scales.empty())
                {
                    auto const index = typed_action.direction == 1
                                           ? 0
                                           : ps.library.scales.size() - 1;
                    state.sequencer.scale = ps.library.scales[index];
                    td = TranslateDirection::Up;
                }

                ps.timeline.stage(std::move(state));
                return minfo("Entire Scale Shifted");
            }
            else if constexpr (std::is_same_v<ActionType,
                                               DoubleSequenceTimeSignatureAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;

                auto index = typed_action.index;
                index = (index == -1) ? (int)state.aux.selected.measure : index;
                if (index < 0 || index >= (int)state.sequencer.sequence_bank.size())
                {
                    return merror("Invalid Sequence Index");
                }

                auto &ts =
                    state.sequencer.sequence_bank[(std::size_t)index].time_signature;
                ts.numerator *= 2;

                if ((float)ts.numerator / (float)ts.denominator > 64.f)
                {
                    return merror(
                        "TimeSignature Too Large, Max length is 64 Whole Notes.");
                }

                if (ts.numerator == 0)
                {
                    return merror("Cannot Double the TimeSignature.");
                }

                ps.timeline.stage(std::move(state));
                return minfo("TimeSignature Doubled.");
            }
            else if constexpr (std::is_same_v<ActionType,
                                               HalveSequenceTimeSignatureAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;

                auto index = typed_action.index;
                index = (index == -1) ? (int)state.aux.selected.measure : index;
                if (index < 0 || index >= (int)state.sequencer.sequence_bank.size())
                {
                    return merror("Invalid Sequence Index");
                }

                auto &ts =
                    state.sequencer.sequence_bank[(std::size_t)index].time_signature;
                if (ts.numerator % 2 == 0)
                {
                    ts.numerator /= 2;
                }
                else
                {
                    ts.denominator *= 2;
                }

                if (ts.denominator == 0)
                {
                    return merror("Cannot Halve the TimeSignature.");
                }

                ps.timeline.stage(std::move(state));
                return minfo("TimeSignature Halved.");
            }
            else if constexpr (std::is_same_v<ActionType, RandomizePitchAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto target, sequence::Pattern const &pattern, int min,
                       int max) {
                        return sequence::modify::randomize_pitch(target, pattern, min,
                                                                 max);
                    },
                    typed_action.pattern, typed_action.min, typed_action.max);
                ps.timeline.stage(std::move(state));
                return minfo("Randomized Pitch");
            }
            else if constexpr (std::is_same_v<ActionType, RandomizeVelocityAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto target, sequence::Pattern const &pattern, float min,
                       float max) {
                        return sequence::modify::randomize_velocity(target, pattern,
                                                                    min, max);
                    },
                    typed_action.pattern, typed_action.min, typed_action.max);
                ps.timeline.stage(std::move(state));
                return minfo("Randomized Velocity");
            }
            else if constexpr (std::is_same_v<ActionType, RandomizeDelayAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto target, sequence::Pattern const &pattern, float min,
                       float max) {
                        return sequence::modify::randomize_delay(target, pattern,
                                                                 min, max);
                    },
                    typed_action.pattern, typed_action.min, typed_action.max);
                ps.timeline.stage(std::move(state));
                return minfo("Randomized Delay");
            }
            else if constexpr (std::is_same_v<ActionType, RandomizeGateAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto target, sequence::Pattern const &pattern, float min,
                       float max) {
                        return sequence::modify::randomize_gate(target, pattern,
                                                                min, max);
                    },
                    typed_action.pattern, typed_action.min, typed_action.max);
                ps.timeline.stage(std::move(state));
                return minfo("Randomized Gate");
            }
            else if constexpr (std::is_same_v<ActionType, StretchAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto target, sequence::Pattern const &pattern,
                       std::size_t count) {
                        return sequence::modify::stretch(target, pattern, count);
                    },
                    typed_action.pattern, typed_action.count);
                ps.timeline.stage(std::move(state));
                return minfo("Stretched Selection by " +
                             std::to_string(typed_action.count));
            }
            else if constexpr (std::is_same_v<ActionType, CompressAction>)
            {
                if (typed_action.pattern == sequence::Pattern{0, {1}})
                {
                    return mwarning("Use pattern prefix to define compression.");
                }

                auto state = ps.timeline.get_state();
                state.aux = context;
                state =
                    increment_state(
                        std::move(state),
                        [](auto target, sequence::Pattern const &pattern) {
                            return sequence::modify::compress(target, pattern);
                        },
                        typed_action.pattern);
                ps.timeline.stage(std::move(state));
                return minfo("Compressed Selection");
            }
            else if constexpr (std::is_same_v<ActionType, ShuffleAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto target) { return sequence::modify::shuffle(target); });
                ps.timeline.stage(std::move(state));
                return minfo("Selection Shuffled");
            }
            else if constexpr (std::is_same_v<ActionType, RotateAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto target, int amount) {
                        return sequence::modify::rotate(target, amount);
                    },
                    typed_action.amount);
                ps.timeline.stage(std::move(state));
                return minfo("Selection Rotated");
            }
            else if constexpr (std::is_same_v<ActionType, ReverseAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto target) { return sequence::modify::reverse(target); });
                ps.timeline.stage(std::move(state));
                return minfo("Selection Reversed");
            }
            else if constexpr (std::is_same_v<ActionType, MirrorAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto target, sequence::Pattern const &pattern, int center_pitch) {
                        return sequence::modify::mirror(target, pattern, center_pitch);
                    },
                    typed_action.pattern, typed_action.center_pitch);
                ps.timeline.stage(std::move(state));
                return minfo("Selection Mirrored");
            }
            else if constexpr (std::is_same_v<ActionType, StepAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;
                state = increment_state(
                    std::move(state),
                    [](auto target, sequence::Pattern const &pattern, int pitch_distance,
                       float velocity_distance) {
                        return action::step(target, pattern, pitch_distance,
                                            velocity_distance);
                    },
                    typed_action.pattern, typed_action.pitch_distance,
                    typed_action.velocity_distance);
                ps.timeline.stage(std::move(state));
                return minfo("Stepped");
            }
            else if constexpr (std::is_same_v<ActionType, DrumsAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;

                auto octave_size =
                    std::clamp<std::size_t>(typed_action.octave_size, 1, 128);

                state.sequencer.base_frequency = 440.f;
                state.sequencer.scale = std::nullopt;
                ps.library.scale_shift_index = std::nullopt;

                auto const a3 = 57;
                state.sequencer.key = 23 + typed_action.offset - a3;

                state.sequencer.tuning = {
                    .intervals =
                        [octave_size] {
                            auto intervals = std::vector<float>{};
                            for (auto i = std::size_t{0}; i < octave_size; ++i)
                            {
                                intervals.push_back(100.f * (float)i);
                            }
                            return intervals;
                        }(),
                    .octave = 100.f * (float)octave_size,
                    .description = "",
                };
                state.sequencer.tuning_name =
                    "Drums (" + std::to_string(octave_size) + ")";

                ps.timeline.stage(std::move(state));
                return minfo("Drum Mode Active");
            }
            else if constexpr (std::is_same_v<ActionType, ArpAction>)
            {
                auto state = ps.timeline.get_state();
                state.aux = context;

                auto chord_name = typed_action.chord;
                auto inversion = typed_action.inversion;

                bool const starting_new_chain =
                    state.aux.selected != state.aux.arp_state.selected ||
                    state.aux.arp_state.previous_commit_id !=
                        ps.timeline.get_current_commit_id();

                if (starting_new_chain)
                {
                    state.aux.arp_state.sequencer = state.sequencer;
                    state.aux.arp_state.selected = state.aux.selected;
                }

                if (chord_name == "cycle" && inversion != -1)
                {
                    chord_name = find_next_chord(
                                     ps.library.chords,
                                     state.aux.arp_state.previous_chord_name)
                                     .name;
                    auto const chord = find_chord(ps.library.chords, chord_name);
                    inversion =
                        std::min(inversion, (int)chord.intervals.size() - 1);
                }
                else if (chord_name != "cycle" && inversion == -1)
                {
                    auto const chord = find_chord(ps.library.chords, chord_name);
                    inversion = increment_inversion(
                        chord, state.aux.arp_state.previous_inversion);
                }
                else if (chord_name == "cycle" && inversion == -1)
                {
                    chord_name = state.aux.arp_state.previous_chord_name;
                    if (chord_name.empty())
                    {
                        inversion = 0;
                    }
                    else
                    {
                        auto const chord = find_chord(ps.library.chords, chord_name);
                        inversion = increment_inversion(
                            chord, state.aux.arp_state.previous_inversion);
                    }

                    if (inversion == 0)
                    {
                        chord_name =
                            find_next_chord(ps.library.chords, chord_name).name;
                    }
                }

                state.aux.arp_state.previous_chord_name = chord_name;
                state.aux.arp_state.previous_inversion = inversion;
                state.aux.arp_state.previous_commit_id =
                    ps.timeline.get_next_commit_id();

                state.sequencer = state.aux.arp_state.sequencer;
                state.aux.selected = state.aux.arp_state.selected;

                auto const chord = find_chord(ps.library.chords, chord_name);
                auto const intervals = invert_chord(
                    chord, inversion, state.sequencer.tuning.intervals.size());
                state = increment_state(
                    std::move(state),
                    [](auto target, sequence::Pattern const &pattern,
                       std::vector<int> const &intervals) {
                        return action::arp(target, pattern, intervals);
                    },
                    typed_action.pattern, intervals);

                ps.timeline.stage(std::move(state));
                return minfo("Arpeggiated with " + chord_name +
                             " inversion: " + std::to_string(inversion));
            }
            else
            {
                static_assert(always_false_v<ActionType>,
                              "Unhandled CommandAction variant alternative.");
            }
        },
        action);

    auto const state_after = ps.timeline.get_state();
    return CommandActionResult{
        .status = std::move(status),
        .context = state_after.aux,
        .engine_mutated = state_after.sequencer != engine_before,
        .commit_intent = ps.commit_intent,
    };
}

} // namespace xen
