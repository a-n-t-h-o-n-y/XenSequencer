#include <xen/xen_command_tree.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

#include <sequence/pattern.hpp>
#include <sequence/modify.hpp>
#include <sequence/sequence.hpp>
#include <sequence/utility.hpp> //temp

#include <xen/actions.hpp>
#include <xen/chord.hpp>
#include <xen/command.hpp>
#include <xen/constants.hpp>
#include <xen/input_mode.hpp>
#include <xen/message_level.hpp>
#include <xen/modulator.hpp>
#include <xen/scale.hpp>
#include <xen/state.hpp>
#include <xen/string_manip.hpp>
#include <xen/user_directory.hpp>

namespace xen
{

using sequence::Pattern;

namespace
{

template <typename Fn, typename... Args>
auto stage_selected_mutation(PluginState &ps, Fn &&fn, Args &&...args) -> void
{
    auto state = increment_state(ps.timeline.get_state(), std::forward<Fn>(fn),
                                 std::forward<Args>(args)...);
    ps.timeline.stage(std::move(state));
}

auto request_force_commit(PluginState &ps) -> void
{
    ps.commit_intent = CommitIntent::Force;
}

auto request_deferred_commit(PluginState &ps) -> void
{
    ps.commit_intent = CommitIntent::Defer;
}

} // namespace

auto create_command_tree() -> XenCommandTree
{
    using PS = PluginState;

    auto head = CommandGroup{""};

    // welcome
    head.add(cmd(signature("welcome"), "Display welcome message.", [](PS &) {
        return minfo(std::string{"Welcome to XenSequencer v"} + VERSION);
    }));

    // version
    head.add(cmd(signature("version"), "Print the current XenSequencer version.",
                 [](PS &) { return minfo(std::string{"v"} + VERSION); }));

    // commit
    head.add(cmd(signature("commit"),
                 "Commit changes to history, mostly for internal use", [](PS &ps) {
                     request_force_commit(ps);
                     return mdebug("commit made");
                 }));

    // reset
    head.add(
        cmd(signature("reset"), "Reset XenSequencer to its initial state.", [](PS &ps) {
            ps.timeline.stage({SequencerState{}, AuxState{}});
            ps.library.scale_shift_index = std::nullopt; // Chromatic
            return minfo("XenSequencer Reset");
        }));

    // undo
    head.add(
        cmd(signature("undo"), "Revert state to before the last action.", [](PS &ps) {
            ps.timeline.reset_stage();
            auto current_aux = ps.timeline.get_state().aux;
            if (ps.timeline.undo())
            {
                auto new_state = ps.timeline.get_state();

                // Important for continuity
                new_state.aux.selected = std::move(current_aux.selected);
                new_state.aux.input_mode = current_aux.input_mode;

                ps.timeline.stage(new_state);
                return minfo("Undone");
            }
            else
            {
                return minfo("Nothing to undo.");
            }
        }));

    // redo
    head.add(cmd(signature("redo"), "Reapply the last undone action.", [](PS &ps) {
        return ps.timeline.redo() ? minfo("Redone") : minfo("Nothing to redo.");
    }));

    // copy
    head.add(cmd(signature("copy"),
                 "Copy the current selection into the shared copy buffer.", [](PS &ps) {
                     auto const state = ps.timeline.get_state();
                     action::copy(state.sequencer, state.aux);
                     return minfo("Copied Selection");
                 }));

    // cut
    head.add(cmd(signature("cut"),
                 "Copy the current selection into the shared copy buffer and replace "
                 "the selection with a Rest.",
                 [](PS &ps) {
                     auto state = ps.timeline.get_state();
                     state.sequencer =
                         action::cut(std::move(state.sequencer), state.aux);
                     ps.timeline.stage(std::move(state));
                     return minfo("Selection Cut");
                 }));

    // paste
    head.add(cmd(
        signature("paste"),
        "Replace the current selection with the contents of the shared copy buffer.",
        [](PS &ps) {
            auto state = ps.timeline.get_state();
            state.sequencer =
                action::paste(std::move(state.sequencer), state.aux);
            ps.timeline.stage(std::move(state));
            return minfo("Selection Pasted Over");
        }));

    // duplicate
    head.add(cmd(signature("duplicate"),
                 "Duplicate the current selection to the next Cell.", [](PS &ps) {
                     ps.timeline.stage(action::duplicate(ps.timeline.get_state()));
                     return minfo("Selection Duplicated");
                 }));

    // inputMode
    head.add(
        cmd(signature("inputMode", arg<InputMode>("mode")),
            "Change the input mode. This determines the behavior of the up/down keys.",
            [](PS &ps, InputMode mode) {
                auto state = ps.timeline.get_state();
                state.aux = action::set_input_mode(std::move(state.aux), mode);
                ps.timeline.stage(std::move(state));
                return minfo("Input Mode Set to " + single_quote(to_string(mode)));
            }));

    // focus
    head.add(cmd(signature("focus", arg<std::string>("component_id")),
                 "Deprecated. UI focus is now handled by the UI adapter layer.",
                 [](PS &, std::string const &) {
                     return mwarning("Command 'focus' is deprecated and has no effect.");
                 }));

    // show
    head.add(cmd(signature("show", arg<std::string>("component_id")),
                 "Deprecated. UI routing is now handled by the UI adapter layer.",
                 [](PS &, std::string const &) {
                     return mwarning("Command 'show' is deprecated and has no effect.");
                 }));

    {
        auto load = cmd_group("load");

        // load sequenceBank
        load->add(cmd(
            signature("sequenceBank", arg<std::string>("filename")),
            "Load the entire sequence bank into the plugin from file. filename must be "
            "located in the library's currently set sequence directory. Do not include "
            "the .xss extension in the filename you provide.",
            [](PS &ps, std::string const &filename) {
                auto const cd = ps.config.current_sequence_directory;
                if (!cd.isDirectory())
                {
                    return merror("Invalid Current Sequence Directory");
                }

                auto const filepath = cd.getChildFile(filename + ".xss");
                if (!filepath.exists())
                {
                    return merror("File Not Found: " +
                                  filepath.getFullPathName().toStdString());
                }

                auto [state, aux] = ps.timeline.get_state();
                auto [sb, names] = action::load_sequence_bank(filepath);
                state.sequence_bank = std::move(sb);
                state.sequence_names = std::move(names);

                ps.timeline.stage({std::move(state), std::move(aux)});

                return minfo("Sequence Bank Loaded");
            }));

        // load tuning
        load->add(cmd(
            signature("tuning", arg<std::string>("filename")),
            "Load a tuning file (.scl) from the current `tunings` Library directory. "
            "Do not include the .scl extension in the filename you provide.",
            [](PS &ps, std::string const &filename) {
                auto const cd = ps.config.current_tuning_directory;
                if (!cd.isDirectory())
                {
                    return merror("Invalid Current Tuning Library Directory");
                }

                auto const filepath = cd.getChildFile(filename + ".scl");
                if (!filepath.exists())
                {
                    return merror("File Not Found: " +
                                  filepath.getFullPathName().toStdString());
                }

                auto [seq, aux] = ps.timeline.get_state();
                seq.tuning_name = filepath.getFileNameWithoutExtension().toStdString();
                seq.tuning =
                    sequence::from_scala(filepath.getFullPathName().toStdString());

                ps.timeline.stage({std::move(seq), std::move(aux)});

                return minfo("Tuning Loaded");
            }));

        // load keys
        load->add(
            cmd(signature("keys"), "Deprecated command.", [](PS &) {
                return mwarning(
                    "Command 'load keys' is deprecated and has no effect.");
            }));

        // load scales
        load->add(cmd(
            signature("scales"), "Load scales.yml and user_scales.yml.", [](PS &ps) {
                ps.library.scales = load_scales_from_files();
                return minfo("Scales Loaded: " + std::to_string(ps.library.scales.size()));
            }));

        // load chords
        load->add(cmd(
            signature("chords"), "Load chords.yml and user_chords.yml.", [](PS &ps) {
                ps.library.chords = load_chords_from_files();
                return minfo("Chords Loaded: " + std::to_string(ps.library.chords.size()));
            }));

        head.add(std::move(load));
    }

    {
        auto save = cmd_group("save");

        // save sequenceBank
        save->add(cmd(
            signature("sequenceBank", arg<std::string>("filename")),
            "Save the entire sequence bank to a file. The file will be located in "
            "the library's current sequence directory. Do not include the .xss "
            "extension in the filename you provide.",
            [](PS &ps, std::string const &filename) {
                auto const cd = ps.config.current_sequence_directory;
                if (!cd.isDirectory())
                {
                    return merror("Invalid Current Sequence Directory");
                }

                auto const filepath = cd.getChildFile(filename + ".xss");
                auto const [state, _] = ps.timeline.get_state();
                action::save_sequence_bank(state.sequence_bank, state.sequence_names,
                                           filepath);
                return minfo("Sequence Bank Saved to " +
                             single_quote(filepath.getFullPathName().toStdString()));
            }));

        head.add(std::move(save));
    }

    // libraryDirectory
    head.add(cmd(signature("libraryDirectory"),
                 "Display the path to the directory where the user library is stored.",
                 [](PS &) {
                     return minfo(
                         get_user_library_directory().getFullPathName().toStdString());
                 }));

    {
        auto move = cmd_group("move");

        // move left
        move->add(cmd(
            signature("left", arg<std::size_t>("amount", 1)),
            "Move the selection left, or wrap around.", [](PS &ps, std::size_t amount) {
                auto state = ps.timeline.get_state();
                state.aux = action::move_left(state.sequencer, std::move(state.aux),
                                              amount);
                ps.timeline.stage(std::move(state));
                return mdebug("Moved Left " + std::to_string(amount) + " Times");
            }));

        // move right
        move->add(cmd(signature("right", arg<std::size_t>("amount", 1)),
                      "Move the selection right, or wrap around.",
                      [](PS &ps, std::size_t amount) {
                          auto state = ps.timeline.get_state();
                          state.aux = action::move_right(
                              state.sequencer, std::move(state.aux), amount);
                          ps.timeline.stage(std::move(state));
                          return mdebug("Moved Right " + std::to_string(amount) +
                                        " Times");
                      }));

        // move up
        move->add(cmd(signature("up", arg<std::size_t>("amount", 1)),
                      "Move the selection up one level to a parent sequence.",
                      [](PS &ps, std::size_t amount) {
                          auto state = ps.timeline.get_state();
                          state.aux =
                              action::move_up(std::move(state.aux), amount);
                          ps.timeline.stage(std::move(state));
                          return mdebug("Moved Up " + std::to_string(amount) +
                                        " Times");
                      }));

        // move down
        move->add(
            cmd(signature("down", arg<std::size_t>("amount", 1)),
                "Move the selection down one level.", [](PS &ps, std::size_t amount) {
                    auto state = ps.timeline.get_state();
                    state.aux = action::move_down(state.sequencer,
                                                  std::move(state.aux), amount);
                    ps.timeline.stage(std::move(state));
                    return mdebug("Moved Down " + std::to_string(amount) + " Times");
                }));

        head.add(std::move(move));
    }

    // note
    head.add(cmd(signature("note", arg<int>("pitch", 0),
                           arg<float>("velocity", 100.f / 127.f),
                           arg<float>("delay", 0.f), arg<float>("gate", 1.f)),
                 "Create a new Note, overwritting the current selection.",
                 [](PS &ps, int pitch, float velocity, float delay, float gate) {
                     stage_selected_mutation(
                         ps,
                         [](sequence::Cell const &c, auto... args) -> sequence::Cell {
                             auto n = sequence::modify::note(args...);
                             n.weight = c.weight;
                             return n;
                         },
                         pitch, velocity, delay, gate);
                     return minfo("Note Created");
                 }));

    // rest
    head.add(cmd(signature("rest"),
                 "Create a new Rest, overwritting the current selection.", [](PS &ps) {
                     stage_selected_mutation(ps,
                                     [](sequence::Cell const &c) -> sequence::Cell {
                                         auto r = sequence::modify::rest();
                                         r.weight = c.weight;
                                         return r;
                                     });
                     return minfo("Rest Created");
                 }));

    // delete
    head.add(cmd(signature("delete"), "Delete the current selection.", [](PS &ps) {
        ps.timeline.stage(action::delete_cell(ps.timeline.get_state()));
        return minfo("Deleted Selection");
    }));

    // split
    head.add(cmd(signature("split", arg<std::size_t>("count", 2)),
                 "Duplicates the current selection into `count` equal parts, replacing "
                 "the current selection.",
                 [](PS &ps, std::size_t count) {
                     stage_selected_mutation(ps, &sequence::modify::repeat, count);
                     return minfo("Split Selection " + std::to_string(count) +
                                  " Times");
                 }));

    // lift
    head.add(cmd(signature("lift"),
                 "Bring the current selection up one level, replacing its parent "
                 "sequence with itself.",
                 [](PS &ps) {
                     ps.timeline.stage(action::lift(ps.timeline.get_state()));
                     return minfo("Selection Lifted One Layer");
                 }));

    // flip
    head.add(cmd(signature("flip", arg<Pattern>("")),
                 "Flips Notes to Rests and Rests to Notes for the current selection. "
                 "Works over sequences.",
                 [](PS &ps, Pattern const &pattern) {
                     stage_selected_mutation(ps, &sequence::modify::flip, pattern,
                                     sequence::Note{});
                     return minfo("Flipped Selection");
                 }));

    {
        auto fill = cmd_group("fill");

        // fill note
        fill->add(cmd(signature("note", arg<Pattern>(""), arg<int>("pitch", 0),
                                arg<float>("velocity", 100.f / 127.f),
                                arg<float>("delay", 0.f), arg<float>("gate", 1.f)),
                      "Fill the current selection with Notes, this works specifically "
                      "over sequences.",
                      [](PS &ps, Pattern const &pattern, int pitch, float velocity,
                         float delay, float gate) {
                          stage_selected_mutation(ps, &sequence::modify::notes_fill,
                                          pattern,
                                          sequence::Note{pitch, velocity, delay, gate});
                          return minfo("Filled Selection With Notes");
                      }));

        // fill rest
        fill->add(cmd(signature("rest", arg<Pattern>("")),
                      "Fill the current selection with Rests, this works specifically "
                      "over sequences.",
                      [](PS &ps, Pattern const &pattern) {
                          stage_selected_mutation(ps, &sequence::modify::rests_fill,
                                          pattern);
                          return minfo("Filled Selection With Rests");
                      }));

        head.add(std::move(fill));
    }

    {
        auto select = cmd_group("select");

        // select sequence
        select->add(cmd(
            signature("sequence", arg<int>("index")),
            "Change the current sequence from the SequenceBank to `index`. Zero-based.",
            [](PS &ps, int index) {
                auto [seq, aux] = ps.timeline.get_state();
                if (aux.selected.measure == (std::size_t)index)
                {
                    return mdebug("Already Selected");
                }
                aux = action::set_selected_sequence(aux, index);
                ps.timeline.stage({std::move(seq), std::move(aux)});
                return mdebug("Sequence " + std::to_string(index) + " Selected");
            }));

        head.add(std::move(select));
    }

    {
        auto set = cmd_group("set");

        // set pitch
        set->add(cmd(
            signature("pitch", arg<Pattern>(""),
                      arg<std::variant<int, Modulator>>("pitch", 0)),
            "Set the pitch of all selected Notes.",
            [](PS &ps, Pattern const &pattern, std::variant<int, Modulator> pitch) {
                std::visit(sequence::utility::overload{
                               [&](int p) {
                                   stage_selected_mutation(ps,
                                                   &sequence::modify::set_pitch,
                                                   pattern, p);
                               },
                               [&](Modulator const &mod) {
                                   stage_selected_mutation(ps, &action::set_pitches,
                                                   pattern, mod);
                                   request_deferred_commit(ps);
                               },
                           },
                           pitch);
                return minfo("Note Set");
            }));

        // set octave
        set->add(cmd(signature("octave", arg<Pattern>(""), arg<int>("octave", 0)),
                     "Set the octave of all selected Notes.",
                     [](PS &ps, Pattern const &pattern, int octave) {
                         auto state = ps.timeline.get_state();
                         state.sequencer = action::set_note_octave(
                             std::move(state.sequencer), state.aux, pattern, octave);
                         ps.timeline.stage(std::move(state));
                         return minfo("Octave Set");
                     }));

        // set velocity
        set->add(cmd(
            signature("velocity", arg<Pattern>(""),
                      arg<std::variant<float, Modulator>>("velocity", 100.f / 127.f)),
            "Set the velocity of all selected Notes.",
            [](PS &ps, Pattern const &pattern,
               std::variant<float, Modulator> velocity) {
                std::visit(sequence::utility::overload{
                               [&](float v) {
                                   stage_selected_mutation(ps,
                                                   &sequence::modify::set_velocity,
                                                   pattern, v);
                               },
                               [&](Modulator const &mod) {
                                   stage_selected_mutation(ps, &action::set_velocities,
                                                   pattern, mod);
                                   request_deferred_commit(ps);
                               },
                           },
                           velocity);
                return minfo("Velocity Set");
            }));

        // set delay
        set->add(cmd(
            signature("delay", arg<Pattern>(""),
                      arg<std::variant<float, Modulator>>("delay", 0.f)),
            "Set the delay of all selected Notes.",
            [](PS &ps, Pattern const &pattern, std::variant<float, Modulator> delay) {
                std::visit(sequence::utility::overload{
                               [&](float d) {
                                   stage_selected_mutation(ps,
                                                   &sequence::modify::set_delay,
                                                   pattern, d);
                               },
                               [&](Modulator const &mod) {
                                   stage_selected_mutation(ps, &action::set_delays,
                                                   pattern, mod);
                                   request_deferred_commit(ps);
                               },
                           },
                           delay);
                return minfo("Delay Set");
            }));

        // set gate
        set->add(cmd(
            signature("gate", arg<Pattern>(""),
                      arg<std::variant<float, Modulator>>("gate", 1.f)),
            "Set the gate of all selected Notes.",
            [](PS &ps, Pattern const &pattern, std::variant<float, Modulator> gate) {
                std::visit(sequence::utility::overload{
                               [&](float g) {
                                   stage_selected_mutation(ps,
                                                   &sequence::modify::set_gate, pattern,
                                                   g);
                               },
                               [&](Modulator const &mod) {
                                   stage_selected_mutation(ps, &action::set_gates,
                                                   pattern, mod);
                                   request_deferred_commit(ps);
                               },
                           },
                           gate);
                return minfo("Gate Set");
            }));

        {
            auto seq = cmd_group("sequence");

            // set sequence name
            seq->add(
                cmd(signature("name", arg<std::string>("name"), arg<int>("index", -1)),
                    "Set the name of a Sequence. If no index is given, set the name of "
                    "the current Sequence.",
                    [](PS &ps, std::string name, int index) {
                        auto [state, aux] = ps.timeline.get_state();
                        index = (index == -1) ? (int)aux.selected.measure : index;
                        if (index < 0 || index >= (int)state.sequence_names.size())
                        {
                            return merror("Invalid Sequence Index");
                        }
                        state.sequence_names[(std::size_t)index] = std::move(name);
                        ps.timeline.stage({std::move(state), std::move(aux)});
                        return minfo("Sequence Name Set");
                    }));

            // set sequence timeSignature
            seq->add(cmd(
                signature("timeSignature",
                          arg<sequence::TimeSignature>("timesignature", {4, 4}),
                          arg<int>("index", -1)),
                "Set the time signature of a Sequence. If no index is given, set "
                "the time signature of the current Sequence.",
                [](PS &ps, sequence::TimeSignature const &ts, int index) {
                    if (ts.denominator == 0 || ts.numerator == 0)
                    {
                        return merror("Invalid TimeSignature");
                    }
                    if ((float)ts.numerator / (float)ts.denominator > 64.f)
                    {
                        return merror(
                            "TimeSignature Too Large, Max length is 64 Whole Notes.");
                    }
                    auto [state, aux] = ps.timeline.get_state();
                    index = (index == -1) ? (int)aux.selected.measure : index;
                    if (index < 0 || index >= (int)state.sequence_bank.size())
                    {
                        return merror("Invalid Sequence Index");
                    }
                    state.sequence_bank[(std::size_t)index].time_signature = ts;
                    ps.timeline.stage({std::move(state), std::move(aux)});
                    return minfo("TimeSignature Set: " + std::to_string(ts.numerator) +
                                 "/" + std::to_string(ts.denominator));
                }));

            set->add(std::move(seq));
        }

        // set baseFrequency
        set->add(cmd(signature("baseFrequency", arg<float>("freq", 440.f)),
                     "Set the base note (pitch zero) frequency to `freq` Hz.",
                     [](PS &ps, float freq) {
                         auto state = ps.timeline.get_state();
                         state.sequencer =
                             action::set_base_frequency(std::move(state.sequencer), freq);
                         ps.timeline.stage(std::move(state));
                         return minfo("Base Frequency Set");
                     }));

        // set theme
        set->add(cmd(
            signature("theme", arg<std::string>("name")),
            "Deprecated command.", [](PS &, std::string const &) {
                return mwarning(
                    "Command 'set theme' is deprecated and has no effect.");
            }));

        // set scale
        set->add(cmd(signature("scale", arg<std::string>("name")),
                     "Set the current scale by name.", [](PS &ps, std::string name) {
                         name = to_lower(name);
                         if (name == "chromatic")
                         {
                             auto state = ps.timeline.get_state();
                             state.sequencer.scale = std::nullopt;
                             ps.timeline.stage(std::move(state));
                             return minfo("Scale Set to " + name + ".");
                         }
                         // Scale names are stored as all lower case.
                         auto const at = std::ranges::find(
                             ps.library.scales, name, [](Scale const &s) { return s.name; });
                         if (at != std::end(ps.library.scales))
                         {
                             auto state = ps.timeline.get_state();
                             state.sequencer.scale = *at;
                             ps.timeline.stage(std::move(state));
                             return minfo("Scale Set to " + name + ".");
                         }
                         else
                         {
                             return merror("No Scale Found: " + name + ".");
                         }
                     }));

        // set mode
        set->add(cmd(signature("mode", arg<std::size_t>("mode_index")),
                     "Set the mode of the current scale. [1, scale size].",
                     [](PS &ps, std::size_t mode_index) {
                         auto state = ps.timeline.get_state();
                         if (mode_index == 0 || !state.sequencer.scale.has_value() ||
                             mode_index > state.sequencer.scale->intervals.size())
                         {
                             return merror("Invalid Mode Index. Must be in range [1, "
                                           "scale size).");
                         }
                         state.sequencer.scale->mode = (std::uint8_t)mode_index;
                         ps.timeline.stage(std::move(state));
                         return minfo("Scale Mode Set");
                     }));

        // set translateDirection
        set->add(cmd(signature("translateDirection", arg<std::string>("direction")),
                     "Set the Scale's translate direction to either Up or Down.",
                     [](PS &ps, std::string direction) {
                         direction = to_lower(direction);
                         auto state = ps.timeline.get_state();
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
                     }));

        // set key
        set->add(cmd(signature("key", arg<int>("key", 0)),
                     "Set the key to tranpose to, any integer value is valid.",
                     [](PS &ps, int key) {
                         if (key > 127 || key < -127)
                         {
                             return merror("Invalid Key Value: " + std::to_string(key) +
                                           ". Must be in range [-127, 127].");
                         }
                         auto state = ps.timeline.get_state();
                         state.sequencer.key = key;
                         ps.timeline.stage(std::move(state));
                         return minfo("Key Set to " + std::to_string(key) + ".");
                     }));

        // set weight
        set->add(cmd(signature("weight", arg<float>("value")),
                     "Set the weight of the selected cell", [](PS &ps, float weight) {
                         stage_selected_mutation(ps, &action::set_weight, weight);
                         return minfo("Weight Set");
                     }));

        // set weights
        set->add(cmd(
            signature("weights", arg<sequence::Pattern>(""),
                      arg<std::variant<float, Modulator>>("weight")),
            "Set the weights of the children of the selected cell, does not make a "
            "commit",
            [](PS &ps, sequence::Pattern const &pattern,
               std::variant<float, Modulator> const &weight) {
                std::visit(
                    sequence::utility::overload{
                        [&](float w) {
                            stage_selected_mutation(
                                ps,
                                static_cast<sequence::Cell (*)(
                                    sequence::Cell, sequence::Pattern const &, float)>(
                                    &action::set_weights),
                                pattern, w);
                        },
                        [&](Modulator const &mod) {
                            stage_selected_mutation(
                                ps,
                                static_cast<sequence::Cell (*)(
                                    sequence::Cell, sequence::Pattern const &,
                                    Modulator const &)>(&action::set_weights),
                                pattern, mod);
                        }},
                    weight);
                request_deferred_commit(ps);
                return minfo("Weights Set");
            }));

        head.add(std::move(set));
    }

    {
        auto dbl = cmd_group("double");
        {
            auto seq = cmd_group("sequence");

            // double sequence timeSignature
            seq->add(cmd(
                signature("timeSignature", arg<int>("index", -1)),
                "Double the given Sequence's TimeSignature, or the currently "
                "selected Sequence's TimeSignature if index is -1.",
                [](PS &ps, int index) {
                    auto [state, aux] = ps.timeline.get_state();
                    index = (index == -1) ? (int)aux.selected.measure : index;
                    if (index < 0 || index >= (int)state.sequence_bank.size())
                    {
                        return merror("Invalid Sequence Index");
                    }

                    auto &ts = state.sequence_bank[(std::size_t)index].time_signature;

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

                    ps.timeline.stage({std::move(state), std::move(aux)});
                    return minfo("TimeSignature Doubled.");
                }));

            dbl->add(std::move(seq));
        }

        head.add(std::move(dbl));
    }

    {
        auto halve = cmd_group("halve");
        {
            auto seq = cmd_group("sequence");

            // halve sequence timeSignature
            seq->add(cmd(signature("timeSignature", arg<int>("index", -1)),
                         "Halve the given Sequence's TimeSignature, or the currently "
                         "selected Sequence's TimeSignature if index is -1.",
                         [](PS &ps, int index) {
                             auto [state, aux] = ps.timeline.get_state();
                             index = (index == -1) ? (int)aux.selected.measure : index;
                             if (index < 0 || index >= (int)state.sequence_bank.size())
                             {
                                 return merror("Invalid Sequence Index");
                             }

                             auto &ts =
                                 state.sequence_bank[(std::size_t)index].time_signature;

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

                             ps.timeline.stage({std::move(state), std::move(aux)});
                             return minfo("TimeSignature Halved.");
                         }));

            halve->add(std::move(seq));
        }

        head.add(std::move(halve));
    }

    {
        auto shift = cmd_group("shift");

        // shift pitch
        shift->add(cmd(signature("pitch", arg<Pattern>(""), arg<int>("amount", 1)),
                       "Increment/Decrement the pitch of all selected Notes.",
                       [](PS &ps, Pattern const &pattern, int amount) {
                           stage_selected_mutation(ps, &sequence::modify::shift_pitch,
                                           pattern, amount);
                           return minfo("Pitch Shifted");
                       }));

        // shift octave
        shift->add(cmd(signature("octave", arg<Pattern>(""), arg<int>("amount", 1)),
                       "Increment/Decrement the octave of all selected Notes.",
                       [](PS &ps, Pattern const &pattern, int amount) {
                           auto state = ps.timeline.get_state();
                           state.sequencer = action::shift_octave(
                               std::move(state.sequencer), state.aux, pattern, amount);
                           ps.timeline.stage(std::move(state));
                           return minfo("Octave Shifted");
                       }));

        // shift velocity
        shift->add(
            cmd(signature("velocity", arg<Pattern>(""), arg<float>("amount", 0.1f)),
                "Increment/Decrement the velocity of all selected Notes.",
                [](PS &ps, Pattern const &pattern, float amount) {
                    stage_selected_mutation(ps, &sequence::modify::shift_velocity,
                                    pattern, amount);
                    return minfo("Velocity Shifted");
                }));

        // shift delay
        shift->add(cmd(signature("delay", arg<Pattern>(""), arg<float>("amount", 0.1f)),
                       "Increment/Decrement the delay of all selected Notes.",
                       [](PS &ps, Pattern const &pattern, float amount) {
                           stage_selected_mutation(ps, &sequence::modify::shift_delay,
                                           pattern, amount);
                           return minfo("Delay Shifted");
                       }));

        // shift gate
        shift->add(cmd(signature("gate", arg<Pattern>(""), arg<float>("amount", 0.1f)),
                       "Increment/Decrement the gate of all selected Notes.",
                       [](PS &ps, Pattern const &pattern, float amount) {
                           stage_selected_mutation(ps, &sequence::modify::shift_gate,
                                           pattern, amount);
                           return minfo("Gate Shifted");
                       }));

        // shift selectedSequence
        shift->add(
            cmd(signature("selectedSequence", arg<int>("amount")),
                "Change the selected/displayed sequence by `amount`. This wraps around "
                "edges of the SequenceBank. `amount` can be positive or negative.",
                [](PS &ps, int amount) {
                    auto [seq, aux] = ps.timeline.get_state();
                    auto const size = (int)seq.sequence_bank.size();
                    auto const index =
                        (((int)aux.selected.measure + amount) % size + size) % size;
                    aux = action::set_selected_sequence(aux, index);
                    ps.timeline.stage({std::move(seq), std::move(aux)});
                    return mdebug("Selected Sequence Shifted");
                }));

        // shift scale
        shift->add(cmd(signature("scale", arg<int>("amount", 1)),
                       "Move Forward/Backward through the loaded Scales.",
                       [](PS &ps, int amount) {
                           auto [seq, aux] = ps.timeline.get_state();
                           auto const index = action::shift_scale_index(
                               ps.library.scale_shift_index, amount, ps.library.scales.size());
                           ps.library.scale_shift_index = index;
                           if (index.has_value() && *index < ps.library.scales.size())
                           {
                               seq.scale = ps.library.scales[*index];
                           }
                           else
                           {
                               seq.scale = std::nullopt; // Chromatic
                           }
                           ps.timeline.stage({std::move(seq), std::move(aux)});
                           return minfo("Scale Shifted");
                       }));

        // shift scaleMode
        shift->add(cmd(signature("scaleMode", arg<int>("amount", 1)),
                       "Increment/Decrement the mode of the current scale.",
                       [](PS &ps, int amount) {
                           auto [seq, aux] = ps.timeline.get_state();
                           if (seq.scale.has_value())
                           {
                               seq.scale = action::shift_scale_mode(*seq.scale, amount);
                               ps.timeline.stage({std::move(seq), std::move(aux)});
                           }
                           return minfo("Scale Mode Shifted");
                       }));

        // shift translateDirection
        shift->add(cmd(signature("translateDirection"),
                       "Flip the scale translate direction to the opposite of its "
                       "current state.",
                       [](PS &ps) {
                           auto state = ps.timeline.get_state();
                           action::flip_translate_direction(
                               state.sequencer.scale_translate_direction);
                           ps.timeline.stage(std::move(state));
                           return minfo("Translate Direction Shifted");
                       }));

        // shift entireScale
        // shift translateDirection -> mode -> scale
        shift->add(cmd(
            signature("entireScale", arg<int>("direction", 1)),
            "Shift the entire scale structure (translateDirection, mode, "
            "scale) in that order, moving onto the next as each reaches the "
            "end. direction parameter must be -1 or 1.",
            [](PS &ps, int direction) {
                if (direction != 1 && direction != -1)
                {
                    return merror("Invalid direction, must be 1 or -1");
                }
                auto [seq, aux] = ps.timeline.get_state();
                auto &td = seq.scale_translate_direction;
                if (seq.scale.has_value())
                {
                    action::flip_translate_direction(td);
                    if (td == TranslateDirection::Up)
                    {
                        seq.scale = action::shift_scale_mode(*(seq.scale), direction);
                        if ((seq.scale->mode == 1 && direction == 1) ||
                            (seq.scale->mode == seq.scale->intervals.size() &&
                             direction == -1))
                        {
                            auto const index = action::shift_scale_index(
                                ps.library.scale_shift_index, direction, ps.library.scales.size());
                            ps.library.scale_shift_index = index;
                            if (index.has_value() && *index < ps.library.scales.size())
                            {
                                seq.scale = ps.library.scales[*index];
                                if (direction == -1)
                                {
                                    seq.scale->mode = seq.scale->intervals.size();
                                }
                            }
                            else
                            {
                                seq.scale = std::nullopt; // Chromatic
                            }
                        }
                    }
                }
                else if (!ps.library.scales.empty()) // Current is chromatic
                {
                    auto const index = direction == 1 ? 0 : ps.library.scales.size() - 1;
                    seq.scale = ps.library.scales[index];
                    td = TranslateDirection::Up;
                }

                ps.timeline.stage({std::move(seq), std::move(aux)});
                return minfo("Entire Scale Shifted");
            }));

        head.add(std::move(shift));
    }

    {
        auto randomize = cmd_group("randomize");

        // randomize pitch
        randomize->add(cmd(signature("pitch", arg<Pattern>(""), arg<int>("min", -12),
                                     arg<int>("max", 12)),
                           "Set the pitch of any selected Notes to a random value.",
                           [](PS &ps, Pattern const &pattern, int min, int max) {
                               stage_selected_mutation(ps,
                                               &sequence::modify::randomize_pitch,
                                               pattern, min, max);
                               return minfo("Randomized Pitch");
                           }));

        // randomize velocity
        randomize->add(cmd(signature("velocity", arg<Pattern>(""),
                                     arg<float>("min", 0.01f), arg<float>("max", 1.f)),
                           "Set the velocity of any selected Notes to a random value.",
                           [](PS &ps, Pattern const &pattern, float min, float max) {
                               stage_selected_mutation(ps,
                                               &sequence::modify::randomize_velocity,
                                               pattern, min, max);
                               return minfo("Randomized Velocity");
                           }));

        // randomize delay
        randomize->add(cmd(signature("delay", arg<Pattern>(""), arg<float>("min", 0.f),
                                     arg<float>("max", 0.95f)),
                           "Set the delay of any selected Notes to a random value.",
                           [](PS &ps, Pattern const &pattern, float min, float max) {
                               stage_selected_mutation(ps,
                                               &sequence::modify::randomize_delay,
                                               pattern, min, max);
                               return minfo("Randomized Delay");
                           }));

        // randomize gate
        randomize->add(cmd(signature("gate", arg<Pattern>(""), arg<float>("min", 0.f),
                                     arg<float>("max", 0.95f)),
                           "Set the gate of any selected Notes to a random value.",
                           [](PS &ps, Pattern const &pattern, float min, float max) {
                               stage_selected_mutation(ps,
                                               &sequence::modify::randomize_gate,
                                               pattern, min, max);
                               return minfo("Randomized Gate");
                           }));

        head.add(std::move(randomize));
    }

    // stretch
    head.add(cmd(
        signature("stretch", arg<Pattern>(""), arg<std::size_t>("count", 2)),
        "Duplicates items in the current selection `count` times, replacing the "
        "current selection.\n\nThis is similar to `split`, the difference is this does "
        "not split sequences, it will traverse until it finds a Note or Rest and will "
        "then duplicate it. This can also take a Pattern, whereas split cannot.",
        [](PS &ps, Pattern const &pattern, std::size_t count) {
            stage_selected_mutation(ps, &sequence::modify::stretch, pattern, count);
            return minfo("Stretched Selection by " + std::to_string(count));
        }));

    // compress
    head.add(cmd(signature("compress", arg<Pattern>("")),
                 "Keep items from the current selection that match the given Pattern, "
                 "replacing the current selection.",
                 [](PS &ps, Pattern const &pattern) {
                     if (pattern == Pattern{0, {1}})
                     {
                         return mwarning("Use pattern prefix to define compression.");
                     }
                     else
                     {
                         stage_selected_mutation(ps, &sequence::modify::compress,
                                         pattern);
                         return minfo("Compressed Selection");
                     }
                 }));

    // shuffle
    head.add(cmd(signature("shuffle"),
                 "Randomly shuffle Notes and Rests in current selection.", [](PS &ps) {
                     stage_selected_mutation(ps, &sequence::modify::shuffle);
                     return minfo("Selection Shuffled");
                 }));

    // rotate
    head.add(cmd(signature("rotate", arg<int>("amount", 1)),
                 "Shift Cells in the current selection by `amount`.\n\nPositive values "
                 "shift right, negative values shift left.",
                 [](PS &ps, int amount) {
                     stage_selected_mutation(ps, &sequence::modify::rotate, amount);
                     return minfo("Selection Rotated");
                 }));

    // reverse
    head.add(cmd(signature("reverse"),
                 "Reverse the order of all Notes and Rests in the current selection.",
                 [](PS &ps) {
                     stage_selected_mutation(ps, &sequence::modify::reverse);
                     return minfo("Selection Reversed");
                 }));

    // mirror
    head.add(
        cmd(signature("mirror", arg<Pattern>(""), arg<int>("centerPitch", 0)),
            "Mirror the note pitches of the current selection around `centerPitch`.",
            [](PS &ps, Pattern const &pattern, int center_pitch) {
                stage_selected_mutation(ps, &sequence::modify::mirror, pattern,
                                center_pitch);
                return minfo("Selection Mirrored");
            }));

    // quantize
    head.add(cmd(
        signature("quantize", arg<Pattern>("")),
        "Set the delay to zero and gate to one for all Notes in the current selection.",
        [](PS &ps, Pattern const &pattern) {
            stage_selected_mutation(ps, &sequence::modify::quantize, pattern);
            return minfo("Selection Quantized");
        }));

    // swing
    head.add(
        cmd(signature("swing", arg<float>("amount", 0.1f)),
            "Set the delay of every other Note in the current selection to `amount`.",
            [](PS &ps, float amount) {
                stage_selected_mutation(ps, &sequence::modify::swing, amount, false);
                return minfo("Selection Swung by " + std::to_string(amount));
            }));

    // step
    head.add(cmd(
        signature("step", arg<Pattern>(""), arg<int>("pitchDistance", 1),
                  arg<float>("velocityDistance", 0.f)),
        "Increments pitch and velocity of each child cell in the selection. "
        "If pattern is given, only adds to increments on cells that match the pattern.",
        [](PS &ps, Pattern const &pattern, int pitch_distance,
           float velocity_distance) {
            auto [state, aux] = ps.timeline.get_state();
            auto &selected = get_selected_cell(state.sequence_bank, aux.selected);
            selected =
                action::step(selected, pattern, pitch_distance, velocity_distance);
            ps.timeline.stage({std::move(state), std::move(aux)});
            return minfo("Stepped");
        }));

    // arp
    head.add(cmd(
        signature("arp", arg<Pattern>(""), arg<std::string>("chord", "cycle"),
                  arg<int>("inversion", -1)),
        "Plays a given chord across the current selection, each interval in the chord "
        "is applied in order to child cells in the selection.",
        [](PS &ps, Pattern const &pattern, std::string chord_name, int inversion) {
            auto [state, aux] = ps.timeline.get_state();

            bool const starting_new_chain =
                aux.selected != aux.arp_state.selected ||
                aux.arp_state.previous_commit_id != ps.timeline.get_current_commit_id();

            if (starting_new_chain)
            {
                aux.arp_state.sequencer = state;
                aux.arp_state.selected = aux.selected;
            }

            // Find chord name and inversion if either is a 'cycle' value.
            if (chord_name == "cycle" && inversion != -1)
            {
                chord_name =
                    find_next_chord(ps.library.chords, aux.arp_state.previous_chord_name).name;
                auto const chord = find_chord(ps.library.chords, chord_name);
                inversion = std::min(inversion, (int)chord.intervals.size() - 1);
            }
            else if (chord_name != "cycle" && inversion == -1)
            {
                auto const chord = find_chord(ps.library.chords, chord_name);
                inversion =
                    increment_inversion(chord, aux.arp_state.previous_inversion);
            }
            else if (chord_name == "cycle" && inversion == -1)
            {
                chord_name = aux.arp_state.previous_chord_name;
                if (chord_name.empty())
                {
                    inversion = 0;
                }
                else
                {
                    auto const chord = find_chord(ps.library.chords, chord_name);
                    inversion =
                        increment_inversion(chord, aux.arp_state.previous_inversion);
                }
                if (inversion == 0)
                {
                    chord_name = find_next_chord(ps.library.chords, chord_name).name;
                }
            }

            // chord_name and inversion are now valid.
            aux.arp_state.previous_chord_name = chord_name;
            aux.arp_state.previous_inversion = inversion;
            aux.arp_state.previous_commit_id = ps.timeline.get_next_commit_id();

            state = aux.arp_state.sequencer;
            aux.selected = aux.arp_state.selected;

            auto &selected = get_selected_cell(state.sequence_bank, aux.selected);
            auto const chord = find_chord(ps.library.chords, chord_name);
            auto const intervals =
                invert_chord(chord, inversion, state.tuning.intervals.size());
            selected = action::arp(selected, pattern, intervals);

            ps.timeline.stage({std::move(state), std::move(aux)});

            return minfo("Arpeggiated with " + chord_name +
                         " inversion: " + std::to_string(inversion));
        }));

    // drums
    head.add(cmd(
        signature("drums", arg<std::size_t>("octaveSize", 16), arg<int>("offset", 1)),
        "Enter 'Drum Mode' where the zero note becomes `offset` plus the lowest of the "
        "general midi drum notes and the number of notes displayed is increased to "
        "`octaveSize`.",
        [](PS &ps, std::size_t octave_size, int offset) {
            octave_size = std::clamp<std::size_t>(octave_size, 1, 128);
            auto [state, aux] = ps.timeline.get_state();

            state.base_frequency = 440.f;

            state.scale = std::nullopt;
            ps.library.scale_shift_index = std::nullopt; // Chromatic

            // A3 is the zero pitch
            auto const A3 = 57;
            state.key = 23 + offset - A3;

            state.tuning = {
                .intervals =
                    [octave_size] {
                        auto intervals = std::vector<float>{};
                        for (std::size_t i = 0; i < octave_size; ++i)
                        {
                            intervals.push_back(100.f * (float)i);
                        }
                        return intervals;
                    }(),
                .octave = 100.f * (float)octave_size,
                .description = "",
            };
            state.tuning_name = "Drums (" + std::to_string(octave_size) + ")";

            ps.timeline.stage({std::move(state), std::move(aux)});
            return minfo("Drum Mode Active");
        }));

    return head;
}

} // namespace xen
