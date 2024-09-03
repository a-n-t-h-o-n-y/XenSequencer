#pragma once

#include <cassert>
#include <cstddef>
#include <exception>
#include <mutex>
#include <string>
#include <utility>
#include <variant>

#include <sequence/sequence.hpp>

#include <xen/actions.hpp>
#include <xen/command.hpp>
#include <xen/gui/themes.hpp>
#include <xen/input_mode.hpp>
#include <xen/message_level.hpp>
#include <xen/state.hpp>
#include <xen/string_manip.hpp>
#include <xen/user_directory.hpp>

namespace xen
{

/**
 * Create a command tree.
 */
[[nodiscard]] inline auto create_command_tree()
{
    using PS = PluginState;

    return cmd_group(
        "", ArgInfo<std::string>{"command_name"},

        cmd("undo", "Revert the last action.",
            [](PS &ps) {
                ps.timeline.reset_stage();
                auto current_aux = ps.timeline.get_state().aux;
                if (ps.timeline.undo())
                {
                    auto new_state = ps.timeline.get_state();
                    new_state.aux = std::move(current_aux); // Important for continuity
                    ps.timeline.stage(new_state);
                    return minfo("Undone");
                }
                else
                {
                    return mwarning("Can't Undo: At Beginning of Timeline");
                }
            }),

        cmd("redo", "Reapply the last undone action.",
            [](PS &ps) {
                return ps.timeline.redo() ? minfo("Redone")
                                          : mwarning("Can't Redo: At End of Timeline");
            }),

        cmd("copy", "Put the current selection in the copy buffer.",
            [](PS &ps) {
                {
                    auto const lock = std::lock_guard{ps.shared.copy_buffer_mtx};
                    ps.shared.copy_buffer = action::copy(ps.timeline);
                }
                return minfo("Copied Selection");
            }),

        cmd("cut",
            "Put the current selection in the copy buffer and replace it with a Rest.",
            [](PS &ps) {
                auto [_, aux] = ps.timeline.get_state();
                auto [buffer, state] = action::cut(ps.timeline);
                ps.timeline.stage({std::move(state), std::move(aux)});
                ps.timeline.set_commit_flag();
                {
                    auto const lock = std::lock_guard{ps.shared.copy_buffer_mtx};
                    ps.shared.copy_buffer = std::move(buffer);
                }
                return minfo("Cut Selection");
            }),

        cmd("paste",
            "Overwrite the current selection with what is stored in the copy buffer.",
            [](PS &ps) {
                auto [_, aux] = ps.timeline.get_state();
                {
                    auto const lock = std::lock_guard{ps.shared.copy_buffer_mtx};
                    ps.timeline.stage({
                        action::paste(ps.timeline, ps.shared.copy_buffer),
                        std::move(aux),
                    });
                }
                ps.timeline.set_commit_flag();
                return minfo("Pasted Over Selection");
            }),

        cmd("duplicate",
            "Duplicate the current selection by placing it in the right-adjacent Cell.",
            [](PS &ps) {
                ps.timeline.stage(action::duplicate(ps.timeline));
                ps.timeline.set_commit_flag();
                return minfo("Duplicated Selection");
            }),

        cmd(
            "mode",
            "Change the input mode."
            "\n\nThe mode determines the behavior of the up/down keys.",
            [](PS &ps, InputMode mode) {
                auto [state, _] = ps.timeline.get_state();
                ps.timeline.stage({
                    std::move(state),
                    action::set_mode(ps.timeline, mode),
                });
                return minfo("Input Mode Set to " + single_quote(to_string(mode)));
            },
            ArgInfo<InputMode>{"mode"}),

        cmd(
            "focus", "Move the keyboard focus to the specified component.",
            [](PS &ps, std::string const &component_id) {
                ps.on_focus_request(component_id);
                return mdebug("Focus Set to " + single_quote(component_id));
            },
            ArgInfo<std::string>{"component_id"}),

        cmd(
            "show", "Update the GUI to display the specified component.",
            [](PS &ps, std::string const &component_id) {
                ps.on_show_request(component_id);
                return mdebug("Showing " + single_quote(component_id));
            },
            ArgInfo<std::string>{"component_id"}),

        cmd_group(
            "load", ArgInfo<std::string>{"filetype"},

            cmd(
                "measure",
                "Load a Measure from a file in the current sequence directory. Do not "
                "include the .xenseq extension in the filename you provide.",
                [](PS &ps, std::string const &filename, int index) {
                    auto [state, aux] = ps.timeline.get_state();
                    index = (index == -1) ? (int)aux.selected.measure : index;
                    if (index < 0 || index >= (int)state.sequence_bank.size())
                    {
                        return merror("Invalid Measure Index");
                    }
                    auto const cd = ps.current_phrase_directory;
                    if (!cd.isDirectory())
                    {
                        return merror("Invalid Current Sequence Library Directory");
                    }

                    auto const filepath = cd.getChildFile(filename + ".xenseq");
                    if (!filepath.exists())
                    {
                        return merror("File Not Found: " +
                                      filepath.getFullPathName().toStdString());
                    }

                    // Call early in case of error
                    auto loaded_measure =
                        action::load_measure(filepath.getFullPathName().toStdString());

                    state.sequence_bank[(std::size_t)index] = std::move(loaded_measure);
                    // Manually reset selection if overwriting current display measure.
                    if ((std::size_t)index == aux.selected.measure)
                    {
                        aux.selected = {aux.selected.measure, {}};
                    }

                    ps.timeline.stage({std::move(state), std::move(aux)});
                    ps.timeline.set_commit_flag();

                    return minfo("State Loaded");
                },
                ArgInfo<std::string>{"filename"}, ArgInfo<int>{"index", -1}),

            cmd(
                "tuning",
                "Load a tuning file (.scl) from the current `tunings` "
                "Library directory. Do not include the .scl extension in the "
                "filename you provide.",
                [](PS &ps, std::string const &filename) {
                    auto const cd = ps.current_tuning_directory;
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
                    // TODO update tuning name from file contents?
                    // TODO grab description from file contents, from_scala should do it
                    seq.tuning_name =
                        filepath.getFileNameWithoutExtension().toStdString();
                    seq.tuning =
                        sequence::from_scala(filepath.getFullPathName().toStdString());

                    ps.timeline.stage({std::move(seq), std::move(aux)});
                    ps.timeline.set_commit_flag();

                    return minfo("Tuning Loaded");
                },
                ArgInfo<std::string>{"filename"}),

            cmd("keys", "Load keys.yml and user_keys.yml.",
                [](PS &ps) {
                    try
                    {
                        {
                            auto const lock = std::lock_guard{
                                ps.shared.on_load_keys_request_mtx,
                            };
                            ps.shared.on_load_keys_request();
                        }
                        return minfo("Key Config Loaded");
                    }
                    catch (std::exception const &e)
                    {
                        return merror("Failed to Load Keys: " + std::string{e.what()});
                    }
                })),

        cmd_group(
            "save", ArgInfo<std::string>{"filetype"},

            cmd(
                "measure",
                "Save the current measure to a file in the current sequence directory. "
                "Do not include any extension in the filename you provide. This will "
                "overwrite any existing file.",
                [](PS &ps, std::string filename) {
                    auto const cd = ps.current_phrase_directory;
                    if (!cd.isDirectory())
                    {
                        return merror("Invalid Current Phrase Directory");
                    }

                    if (filename.empty())
                    {
                        auto const state = ps.timeline.get_state();
                        filename =
                            state.sequencer.measure_names[state.aux.selected.measure];

                        if (filename.empty())
                        {
                            return merror("No Measure Name Found.");
                        }
                    }
                    else // store new measure name
                    {
                        auto state = ps.timeline.get_state();
                        state.sequencer.measure_names[state.aux.selected.measure] =
                            filename;
                        ps.timeline.stage(
                            {std::move(state.sequencer), std::move(state.aux)});
                        ps.timeline.set_commit_flag();
                    }

                    auto const filepath = cd.getChildFile(filename + ".xenseq")
                                              .getFullPathName()
                                              .toStdString();

                    // TODO add index to this command with default for current selection
                    auto const state = ps.timeline.get_state();
                    auto const &measure =
                        state.sequencer.sequence_bank[state.aux.selected.measure];
                    action::save_measure(measure, filepath);
                    return minfo("State Saved to " + single_quote(filepath));
                },
                ArgInfo<std::string>{"filename", ""})),

        cmd("libraryDirectory",
            "Display the path to the directory where the user library is stored.",
            [](PS &) {
                return minfo(
                    get_user_library_directory().getFullPathName().toStdString());
            }),

        cmd("UUID", "Display the UUID for this instance.",
            [](PS &ps) { return minfo(ps.PROCESS_UUID.toString().toStdString()); }),

        cmd_group(
            "move", ArgInfo<std::string>{"direction"},

            cmd(
                "left", "Move the selection left, or wrap around.",
                [](PS &ps, std::size_t amount) {
                    auto [state, _] = ps.timeline.get_state();
                    ps.timeline.stage({
                        std::move(state),
                        action::move_left(ps.timeline, amount),
                    });
                    return mdebug("Moved Left " + std::to_string(amount) + " Times");
                },
                ArgInfo<std::size_t>{"amount", 1}),

            cmd(
                "right", "Move the selection right, or wrap around.",
                [](PS &ps, std::size_t amount) {
                    auto [state, _] = ps.timeline.get_state();
                    ps.timeline.stage({
                        std::move(state),
                        action::move_right(ps.timeline, amount),
                    });
                    return mdebug("Moved Right " + std::to_string(amount) + " Times");
                },
                ArgInfo<std::size_t>{"amount", 1}),

            cmd(
                "up", "Move the selection up one level to a parent sequence.",
                [](PS &ps, std::size_t amount) {
                    auto [state, _] = ps.timeline.get_state();
                    ps.timeline.stage({
                        std::move(state),
                        action::move_up(ps.timeline, amount),
                    });
                    return mdebug("Moved Up " + std::to_string(amount) + " Times");
                },
                ArgInfo<std::size_t>{"amount", 1}),

            cmd(
                "down", "Move the selection down one level.",
                [](PS &ps, std::size_t amount) {
                    auto [state, _] = ps.timeline.get_state();
                    ps.timeline.stage({
                        std::move(state),
                        action::move_down(ps.timeline, amount),
                    });
                    return mdebug("Moved Down " + std::to_string(amount) + " Times");
                },
                ArgInfo<std::size_t>{"amount", 1})),

        cmd(
            "note", "Create a new Note, overwritting the current selection.",
            [](PS &ps, int interval, float velocity, float delay, float gate) {
                increment_state(
                    ps.timeline,
                    [](sequence::Cell const &, auto... args) -> sequence::Cell {
                        return sequence::modify::note(args...);
                    },
                    interval, velocity, delay, gate);
                ps.timeline.set_commit_flag();
                return minfo("Note Created");
            },
            ArgInfo<int>{"interval", 0}, ArgInfo<float>{"velocity", 0.8f},
            ArgInfo<float>{"delay", 0.f}, ArgInfo<float>{"gate", 1.f}),

        cmd("rest", "Create a new Rest, overwritting the current selection.",
            [](PS &ps) {
                increment_state(ps.timeline,
                                [](sequence::Cell const &) -> sequence::Cell {
                                    return sequence::modify::rest();
                                });
                ps.timeline.set_commit_flag();
                return minfo("Rest Created");
            }),

        pattern(cmd("flip",
                    "Flips Notes to Rests and Rests to Notes for the current "
                    "selection. Works over "
                    "sequences.",
                    [](PS &ps, sequence::Pattern const &pattern) {
                        increment_state(ps.timeline, &sequence::modify::flip, pattern,
                                        sequence::Note{});
                        ps.timeline.set_commit_flag();
                        return minfo("Flipped Selection");
                    })),

        cmd_group("delete", ArgInfo<std::string>{"item", "selection"},

                  cmd("selection", "Delete the current selection.",
                      [](PS &ps) {
                          ps.timeline.stage(
                              action::delete_cell(ps.timeline.get_state()));
                          ps.timeline.set_commit_flag();
                          return minfo("Deleted Selection");
                      })),

        cmd(
            "split",
            "Duplicates the current selection into `count` equal parts, replacing the "
            "current selection.",
            [](PS &ps, std::size_t count) {
                increment_state(ps.timeline, &sequence::modify::repeat, count);
                ps.timeline.set_commit_flag();
                return minfo("Split Selection " + std::to_string(count) + " Times");
            },
            ArgInfo<std::size_t>{"count", 2}),

        cmd("lift",
            "Bring the current selection up one level, replacing its parent sequence "
            "with "
            "itself.",
            [](PS &ps) {
                ps.timeline.stage(action::lift(ps.timeline));
                ps.timeline.set_commit_flag();
                return minfo("Selection Lifted One Layer");
            }),

        pattern(cmd(
            "stretch",
            "Duplicates items in the current selection `count` times, replacing the "
            "current selection."
            "\n\nThis is similar to `split`, the difference is this does not split "
            "sequences, it will traverse until"
            " it finds a Note or Rest and will then duplicate it. This can also take a "
            "Pattern, whereas split cannot.",
            [](PS &ps, sequence::Pattern const &pattern, std::size_t count) {
                increment_state(ps.timeline, &sequence::modify::stretch, pattern,
                                count);
                ps.timeline.set_commit_flag();
                return minfo("Stretched Selection by " + std::to_string(count));
            },
            ArgInfo<std::size_t>{"count", 2})),

        pattern(cmd(
            "compress",
            "Keep items from the current selection that match the given Pattern, "
            "replacing the current selection.",
            [](PS &ps, sequence::Pattern const &pattern) {
                if (pattern == sequence::Pattern{0, {1}})
                {
                    return mwarning("Use pattern prefix to define compression.");
                }
                else
                {
                    increment_state(ps.timeline, &sequence::modify::compress, pattern);
                    ps.timeline.set_commit_flag();
                    return minfo("Compressed Selection");
                }
            })),

        pattern(cmd_group(
            "fill", ArgInfo<std::string>{"type"},

            cmd(
                "note",
                "Fill the current selection with Notes, this works specifically over "
                "sequences.",
                [](PS &ps, sequence::Pattern const &pattern, int interval,
                   float velocity, float delay, float gate) {
                    increment_state(ps.timeline, &sequence::modify::notes_fill, pattern,
                                    sequence::Note{interval, velocity, delay, gate});
                    ps.timeline.set_commit_flag();
                    return minfo("Filled Selection With Notes");
                },
                ArgInfo<int>{"interval", 0}, ArgInfo<float>{"velocity", 0.8f},
                ArgInfo<float>{"delay", 0.f}, ArgInfo<float>{"gate", 1.f}),

            cmd("rest",
                "Fill the current selection with Rests, this works specifically over "
                "sequences.",
                [](PS &ps, sequence::Pattern const &pattern) {
                    increment_state(ps.timeline, &sequence::modify::rests_fill,
                                    pattern);
                    ps.timeline.set_commit_flag();
                    return minfo("Filled Selection With Rests");
                }))),

        cmd_group("select", ArgInfo<std::string>{"type"},

                  cmd(
                      "sequence",
                      "Change the current sequence from the SequenceBank to `index`. "
                      "Zero-based.",
                      [](PS &ps, int index) {
                          auto [seq, aux] = ps.timeline.get_state();
                          if (aux.selected.measure == (std::size_t)index)
                          {
                              return mwarning("Already Selected");
                          }
                          aux = action::set_selected_sequence(aux, index);
                          ps.timeline.stage({std::move(seq), std::move(aux)});
                          return mdebug("Sequence " + std::to_string(index) +
                                        " Selected");
                      },
                      ArgInfo<int>{"index"})),

        pattern(cmd_group(
            "set", ArgInfo<std::string>{"trait"},

            cmd(
                "note", "Set the note interval of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, int interval) {
                    increment_state(ps.timeline, &sequence::modify::set_interval,
                                    pattern, interval);
                    ps.timeline.set_commit_flag();
                    return minfo("Note Set");
                },
                ArgInfo<int>{"interval", 0}),

            cmd(
                "octave", "Set the octave of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, int octave) {
                    auto [_, aux] = ps.timeline.get_state();
                    ps.timeline.stage({
                        action::set_note_octave(ps.timeline, pattern, octave),
                        std::move(aux),
                    });
                    ps.timeline.set_commit_flag();
                    return minfo("Octave Set");
                },
                ArgInfo<int>{"octave", 0}),

            cmd(
                "velocity", "Set the velocity of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float velocity) {
                    increment_state(ps.timeline, &sequence::modify::set_velocity,
                                    pattern, velocity);
                    ps.timeline.set_commit_flag();
                    return minfo("Velocity Set");
                },
                ArgInfo<float>{"velocity", 0.8f}),

            cmd(
                "delay", "Set the delay of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float delay) {
                    increment_state(ps.timeline, &sequence::modify::set_delay, pattern,
                                    delay);
                    ps.timeline.set_commit_flag();
                    return minfo("Delay Set");
                },
                ArgInfo<float>{"delay", 0.f}),

            cmd(
                "gate", "Set the gate of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float gate) {
                    increment_state(ps.timeline, &sequence::modify::set_gate, pattern,
                                    gate);
                    ps.timeline.set_commit_flag();
                    return minfo("Gate Set");
                },
                ArgInfo<float>{"gate", 1.f}),

            cmd_group(
                "measure", ArgInfo<std::string>{"trait"},

                cmd(
                    "name",
                    "Set the name of a Measure. If no index is given, set the "
                    "name of the current Measure. Ignores Pattern.",
                    [](PS &ps, sequence::Pattern const &, std::string name, int index) {
                        auto [state, aux] = ps.timeline.get_state();
                        index = (index == -1) ? (int)aux.selected.measure : index;
                        if (index < 0 || index >= (int)state.measure_names.size())
                        {
                            return merror("Invalid Measure Index");
                        }
                        state.measure_names[(std::size_t)index] = std::move(name);
                        ps.timeline.stage({std::move(state), std::move(aux)});
                        ps.timeline.set_commit_flag();
                        return minfo("Measure Name Set");
                    },
                    ArgInfo<std::string>{"name"}, ArgInfo<int>{"index", -1}),

                cmd(
                    "timeSignature",
                    "Set the time signature of a Measure. If no index is given, set "
                    "the time signature of the current Measure. Ignores Pattern.",
                    [](PS &ps, sequence::Pattern const &,
                       sequence::TimeSignature const &ts, int index) {
                        auto [state, aux] = ps.timeline.get_state();
                        index = (index == -1) ? (int)aux.selected.measure : index;
                        if (index < 0 || index >= (int)state.sequence_bank.size())
                        {
                            return merror("Invalid Measure Index");
                        }

                        state.sequence_bank[(std::size_t)index].time_signature = ts;
                        ps.timeline.stage({std::move(state), std::move(aux)});
                        ps.timeline.set_commit_flag();
                        return minfo("TimeSignature Set");
                    },
                    ArgInfo<sequence::TimeSignature>{"timesignature", {{4, 4}}},
                    ArgInfo<int>{"index", -1})),

            cmd_group("tuning", ArgInfo<std::string>{"trait"},

                      cmd(
                          "name",
                          "Set the name of the current tuning. Ignores Pattern.",
                          [](PS &ps, sequence::Pattern const &, std::string name) {
                              auto [state, aux] = ps.timeline.get_state();
                              state.tuning_name = std::move(name);
                              ps.timeline.stage({std::move(state), std::move(aux)});
                              ps.timeline.set_commit_flag();
                              return minfo("Tuning Name Set");
                          },
                          ArgInfo<std::string>{"name"}),
                      cmd(
                          "baseFrequency",
                          "Set the base note (interval zero) frequency to `freq` Hz.",
                          [](PS &ps, sequence::Pattern const &, float freq) {
                              auto [_, aux] = ps.timeline.get_state();
                              ps.timeline.stage({
                                  action::set_base_frequency(ps.timeline, freq),
                                  std::move(aux),
                              });
                              ps.timeline.set_commit_flag();
                              return minfo("Base Frequency Set");
                          },
                          ArgInfo<float>{"freq", 440.f})),

            cmd(
                "theme", "Set the color theme of the app by name.",
                [](PS &ps, sequence::Pattern const &, std::string name) {
                    name = to_lower(strip(name));
                    if (name == "dark")
                    {
                        name = "apollo";
                    }
                    else if (name == "light")
                    {
                        name = "coal";
                    }
                    try
                    {
                        auto const theme = gui::find_theme(name);
                        {
                            auto const lock = std::lock_guard{ps.shared.theme_mtx};
                            ps.shared.theme = theme;
                            ps.shared.on_theme_update(ps.shared.theme);
                        }
                        return minfo("Theme Set");
                    }
                    catch (std::exception const &e)
                    {
                        return merror("Failed to Load Theme: " + std::string{e.what()});
                    }
                },
                ArgInfo<std::string>{"name"}))),

        pattern(cmd_group(
            "shift", ArgInfo<std::string>{"trait"},

            cmd(
                "note", "Increment/Decrement the note interval of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, int amount) {
                    increment_state(ps.timeline, &sequence::modify::shift_interval,
                                    pattern, amount);
                    ps.timeline.set_commit_flag();
                    return minfo("Note Shifted");
                },
                ArgInfo<int>{"amount", 1}),

            cmd(
                "octave", "Increment/Decrement the octave of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, int amount) {
                    auto [_, aux] = ps.timeline.get_state();
                    ps.timeline.stage({
                        action::shift_note_octave(ps.timeline, pattern, amount),
                        std::move(aux),
                    });
                    ps.timeline.set_commit_flag();
                    return minfo("Octave Shifted");
                },
                ArgInfo<int>{"amount", 1}),

            cmd(
                "velocity", "Increment/Decrement the velocity of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float amount) {
                    increment_state(ps.timeline, &sequence::modify::shift_velocity,
                                    pattern, amount);
                    ps.timeline.set_commit_flag();
                    return minfo("Velocity Shifted");
                },
                ArgInfo<float>{"amount", 0.1f}),

            cmd(
                "delay", "Increment/Decrement the delay of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float amount) {
                    increment_state(ps.timeline, &sequence::modify::shift_delay,
                                    pattern, amount);
                    ps.timeline.set_commit_flag();
                    return minfo("Delay Shifted");
                },
                ArgInfo<float>{"amount", 0.1f}),

            cmd(
                "gate", "Increment/Decrement the gate of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float amount) {
                    increment_state(ps.timeline, &sequence::modify::shift_gate, pattern,
                                    amount);
                    ps.timeline.set_commit_flag();
                    return minfo("Gate Shifted");
                },
                ArgInfo<float>{"amount", 0.1f}),

            cmd(
                "selectedSequence",
                "Change the selected/displayed sequence by `amount`. This wraps around "
                "edges of the SequenceBank. `amount` can be positive or negative. "
                "Pattern is ignored.",
                [](PS &ps, sequence::Pattern const &, int amount) {
                    auto [seq, aux] = ps.timeline.get_state();
                    auto const index = ((int)aux.selected.measure + amount) %
                                       (int)seq.sequence_bank.size();
                    aux = action::set_selected_sequence(aux, index);
                    ps.timeline.stage({std::move(seq), std::move(aux)});
                    return mdebug("Selected Sequence Shifted");
                },
                ArgInfo<int>{"amount"}))),

        pattern(cmd_group(
            "humanize", ArgInfo<InputMode>{"mode"},

            cmd(
                InputMode::Velocity,
                "Apply a random shift to the velocity of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float amount) {
                    increment_state(ps.timeline, &sequence::modify::humanize_velocity,
                                    pattern, amount);
                    ps.timeline.set_commit_flag();
                    return minfo("Humanized Velocity");
                },
                ArgInfo<float>{"amount", 0.1f}),

            cmd(
                InputMode::Delay,
                "Apply a random shift to the delay of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float amount) {
                    increment_state(ps.timeline, &sequence::modify::humanize_delay,
                                    pattern, amount);
                    ps.timeline.set_commit_flag();
                    return minfo("Humanized Delay");
                },
                ArgInfo<float>{"amount", 0.1f}),

            cmd(
                InputMode::Gate,
                "Apply a random shift to the gate of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float amount) {
                    increment_state(ps.timeline, &sequence::modify::humanize_gate,
                                    pattern, amount);
                    ps.timeline.set_commit_flag();
                    return minfo("Humanized Gate");
                },
                ArgInfo<float>{"amount", 0.1f}))),

        pattern(cmd_group(
            "randomize", ArgInfo<InputMode>{"mode"},

            cmd(
                InputMode::Note,
                "Set the note interval of any selected Notes to a random value.",
                [](PS &ps, sequence::Pattern const &pattern, int min, int max) {
                    increment_state(ps.timeline, &sequence::modify::randomize_intervals,
                                    pattern, min, max);
                    ps.timeline.set_commit_flag();
                    return minfo("Randomized Note");
                },
                ArgInfo<int>{"min", -12}, ArgInfo<int>{"max", 12}),

            cmd(
                InputMode::Velocity,
                "Set the velocity of any selected Notes to a random value.",
                [](PS &ps, sequence::Pattern const &pattern, float min, float max) {
                    increment_state(ps.timeline, &sequence::modify::randomize_velocity,
                                    pattern, min, max);
                    ps.timeline.set_commit_flag();
                    return minfo("Randomized Velocity");
                },
                ArgInfo<float>{"min", 0.01f}, ArgInfo<float>{"max", 1.f}),

            cmd(
                InputMode::Delay,
                "Set the delay of any selected Notes to a random value.",
                [](PS &ps, sequence::Pattern const &pattern, float min, float max) {
                    increment_state(ps.timeline, &sequence::modify::randomize_delay,
                                    pattern, min, max);
                    ps.timeline.set_commit_flag();
                    return minfo("Randomized Delay");
                },
                ArgInfo<float>{"min", 0.f}, ArgInfo<float>{"max", 0.95f}),

            cmd(
                InputMode::Gate,
                "Set the gate of any selected Notes to a random value.",
                [](PS &ps, sequence::Pattern const &pattern, float min, float max) {
                    increment_state(ps.timeline, &sequence::modify::randomize_gate,
                                    pattern, min, max);
                    ps.timeline.set_commit_flag();
                    return minfo("Randomized Gate");
                },
                ArgInfo<float>{"min", 0.f}, ArgInfo<float>{"max", 0.95f}))),

        cmd("shuffle", "Randomly shuffle Notes and Rests in current selection.",
            [](PS &ps) {
                increment_state(ps.timeline, &sequence::modify::shuffle);
                ps.timeline.set_commit_flag();
                return minfo("Selection Shuffled");
            }),

        cmd(
            "rotate",
            "Shift the Notes and Rests in the current selection by `amount`."
            " Positive values shift right, negative values shift left.",
            [](PS &ps, int amount) {
                increment_state(ps.timeline, &sequence::modify::rotate, amount);
                ps.timeline.set_commit_flag();
                return minfo("Selection Rotated");
            },
            ArgInfo<int>{"amount", 1}),

        cmd("reverse",
            "Reverse the order of all Notes and Rests in the current selection.",
            [](PS &ps) {
                increment_state(ps.timeline, &sequence::modify::reverse);
                ps.timeline.set_commit_flag();
                return minfo("Selection Reversed");
            }),

        pattern(cmd(
            "mirror",
            "Mirror the note intervals of the current selection around `centerNote`.",
            [](PS &ps, sequence::Pattern const &pattern, int center_note) {
                increment_state(ps.timeline, &sequence::modify::mirror, pattern,
                                center_note);
                ps.timeline.set_commit_flag();
                return minfo("Selection Mirrored");
            },
            ArgInfo<int>{"centerNote", 0})),

        pattern(cmd("quantize",
                    "Set the delay to zero and gate to one for all Notes in the "
                    "current selection.",
                    [](PS &ps, sequence::Pattern const &pattern) {
                        increment_state(ps.timeline, &sequence::modify::quantize,
                                        pattern);
                        ps.timeline.set_commit_flag();
                        return minfo("Selection Quantized");
                    })),

        cmd(
            "swing",
            "Set the delay of every other Note in the current selection to `amount`.",
            [](PS &ps, float amount) {
                increment_state(ps.timeline, &sequence::modify::swing, amount, false);
                ps.timeline.set_commit_flag();
                return minfo("Selection Swung by " + std::to_string(amount));
            },
            ArgInfo<float>{"amount", 0.1f}),

        cmd(
            "step",
            "Repeat the selected Cell with incrementing interval and velocity applied.",
            [](PS &ps, std::size_t count, int interval_distance,
               float velocity_distance) {
                if (velocity_distance > 1.f || velocity_distance < -1.f)
                {
                    return merror("velocity distance has to be in the range: [-1, 1]");
                }

                auto [state, aux] = ps.timeline.get_state();
                auto &selected = get_selected_cell(state.sequence_bank, aux.selected);

                // Split
                selected = sequence::modify::repeat(selected, count);

                assert(std::holds_alternative<sequence::Sequence>(selected));

                // Increment Interval
                auto index = 0;
                for (auto &cell : std::get<sequence::Sequence>(selected).cells)
                {
                    cell = sequence::modify::shift_interval(cell, {0, {1}},
                                                            index * interval_distance);
                    ++index;
                }

                // Increment Velocity
                index = 0;
                for (auto &cell : std::get<sequence::Sequence>(selected).cells)
                {
                    cell = sequence::modify::shift_velocity(
                        cell, {0, {1}}, (float)index * velocity_distance);
                    ++index;
                }

                ps.timeline.stage({std::move(state), std::move(aux)});
                ps.timeline.set_commit_flag();
                return minfo("Stepped");
            },
            ArgInfo<std::size_t>{"count"}, ArgInfo<int>{"interval_distance"},
            ArgInfo<float>{"velocity_distance", 0.f}));
}

using XenCommandTree = decltype(create_command_tree());

} // namespace xen
