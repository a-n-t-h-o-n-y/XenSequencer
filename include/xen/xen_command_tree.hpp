#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <juce_core/juce_core.h>

#include <sequence/sequence.hpp>

#include <signals_light/signal.hpp>

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
 * Create a command tree, each GUI should have its own on_focus_change_request
 * signal, and therefore should have its own command tree.
 *
 * Each of the passed in parameter l-val references should outlive the command tree.
 */
[[nodiscard]] inline auto create_command_tree()
{
    using PS = PluginState;

    return cmd_group(
        "", ArgInfo<std::string>{"command_name"},

        cmd("undo", "Revert the last action.",
            [](PS &ps) {
                return ps.timeline.undo() ? minfo("Undone")
                                          : mwarning("Can't Undo: At Beginning of "
                                                     "Timeline");
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
                auto [buffer, state] = action::cut(ps.timeline);
                ps.timeline.add_state(std::move(state));
                {
                    auto const lock = std::lock_guard{ps.shared.copy_buffer_mtx};
                    ps.shared.copy_buffer = std::move(buffer);
                }
                return minfo("Cut Selection");
            }),

        cmd("paste",
            "Overwrite the current selection with what is stored in the copy buffer.",
            [](PS &ps) {
                {
                    auto const lock = std::lock_guard{ps.shared.copy_buffer_mtx};
                    ps.timeline.add_state(
                        action::paste(ps.timeline, ps.shared.copy_buffer));
                }
                return minfo("Pasted Over Selection");
            }),

        cmd("duplicate",
            "Duplicate the current selection by placing it in the right-adjacent Cell.",
            [](PS &ps) {
                auto [aux, state] = action::duplicate(ps.timeline);
                ps.timeline.set_aux_state(std::move(aux), false);
                ps.timeline.add_state(std::move(state));
                return minfo("Duplicated Selection");
            }),

        cmd(
            "mode",
            "Change the input mode."
            "\n\nThe mode determines the behavior of the up/down keys.",
            [](PS &ps, InputMode mode) {
                ps.timeline.set_aux_state(action::set_mode(ps.timeline, mode));
                return minfo("Input Mode Set to '" + to_string(mode) + '\'');
            },
            ArgInfo<InputMode>{"mode"}),

        cmd(
            "focus", "Move the keyboard focus to the specified component.",
            [](PS &ps, std::string const &component_id) {
                ps.on_focus_request(component_id);
                return mdebug("Focus Set to '" + component_id + '\'');
            },
            ArgInfo<std::string>{"component_id"}),

        cmd(
            "show", "Update the GUI to display the specified component.",
            [](PS &ps, std::string const &component_id) {
                ps.on_show_request(component_id);
                return mdebug("Showing '" + component_id + '\'');
            },
            ArgInfo<std::string>{"component_id"}),

        cmd_group(
            "load", ArgInfo<std::string>{"filetype"},

            cmd(
                "state",
                "Load a full plugin State from a file in the current directory. Do not "
                "include the .json extension in the filename you provide.",
                [](PS &ps, std::string const &filename) {
                    auto const cd =
                        ps.timeline.get_aux_state().current_phrase_directory;
                    if (!cd.isDirectory())
                    {
                        return merror("Invalid Current Phrase Directory");
                    }

                    auto const filepath = cd.getChildFile(filename + ".json");
                    if (!filepath.exists())
                    {
                        return merror("File Not Found: " +
                                      filepath.getFullPathName().toStdString());
                    }

                    // Call early in case of error
                    auto new_state =
                        action::load_state(filepath.getFullPathName().toStdString());

                    // Manually reset selection and phrase name on overwrite
                    auto aux = ps.timeline.get_aux_state();
                    aux.selected = {0, {}};
                    aux.current_phrase_name = juce::String{filename};
                    ps.timeline.set_aux_state(std::move(aux), false);

                    ps.timeline.add_state(std::move(new_state));

                    return minfo("State Loaded");
                },
                ArgInfo<std::string>{"filename"}),

            cmd("keys", "Load keys.yml and user_keys.yml.",
                [](PS &ps) {
                    try
                    {
                        {
                            auto const lock =
                                std::lock_guard{ps.shared.on_load_keys_request_mtx};
                            ps.shared.on_load_keys_request();
                        }
                        return minfo("Key Config Loaded");
                    }
                    catch (std::exception const &e)
                    {
                        return merror("Failed to Load Keys: " + std::string{e.what()});
                    }
                })),

        cmd_group("save", ArgInfo<std::string>{"filetype"},

                  cmd(
                      "state",
                      "Save the current plugin State to a file in the current phrase "
                      "directory. Do not include any extension in the filename you "
                      "provide. This will overwrite any existing file.",
                      [](PS &ps, std::string filename) {
                          auto const cd =
                              ps.timeline.get_aux_state().current_phrase_directory;
                          if (!cd.isDirectory())
                          {
                              return merror("Invalid Current Phrase Directory");
                          }

                          if (filename.empty())
                          {
                              filename = ps.timeline.get_aux_state()
                                             .current_phrase_name.toStdString();
                              if (filename.empty())
                              {
                                  return merror("No Phrase Name Found.");
                              }
                          }
                          else // store new phrase name
                          {
                              auto aux = ps.timeline.get_aux_state();
                              aux.current_phrase_name = juce::String{filename};
                              ps.timeline.set_aux_state(std::move(aux));
                          }

                          auto const filepath = cd.getChildFile(filename + ".json")
                                                    .getFullPathName()
                                                    .toStdString();

                          action::save_state(ps.timeline, filepath);
                          return minfo("State Saved to '" + filepath + '\'');
                      },
                      ArgInfo<std::string>{"filename", ""})),

        cmd("dataDirectory",
            "Display the path to the directory where user data is stored.",
            [](PS &) {
                return minfo(get_user_data_directory().getFullPathName().toStdString());
            }),

        cmd("UUID", "Display the UUID for this instance.",
            [](PS &ps) { return minfo(ps.PROCESS_UUID.toString().toStdString()); }),

        cmd_group(
            "move", ArgInfo<std::string>{"direction"},

            cmd(
                "left", "Move the selection left, or wrap around.",
                [](PS &ps, std::size_t amount) {
                    ps.timeline.set_aux_state(action::move_left(ps.timeline, amount));
                    return mdebug("Moved Left " + std::to_string(amount) + " Times");
                },
                ArgInfo<std::size_t>{"amount", 1}),

            cmd(
                "right", "Move the selection right, or wrap around.",
                [](PS &ps, std::size_t amount) {
                    ps.timeline.set_aux_state(action::move_right(ps.timeline, amount));
                    return mdebug("Moved Right " + std::to_string(amount) + " Times");
                },
                ArgInfo<std::size_t>{"amount", 1}),

            cmd(
                "up", "Move the selection up one level to a parent sequence.",
                [](PS &ps, std::size_t amount) {
                    ps.timeline.set_aux_state(action::move_up(ps.timeline, amount));
                    return mdebug("Moved Up " + std::to_string(amount) + " Times");
                },
                ArgInfo<std::size_t>{"amount", 1}),

            cmd(
                "down", "Move the selection down one level.",
                [](PS &ps, std::size_t amount) {
                    ps.timeline.set_aux_state(action::move_down(ps.timeline, amount));
                    return mdebug("Moved Down " + std::to_string(amount) + " Times");
                },
                ArgInfo<std::size_t>{"amount", 1})),

        cmd_group("append", ArgInfo<std::string>{"item", "measure"},

                  cmd(
                      "measure", "Append a measure to the current phrase.",
                      [](PS &ps, sequence::TimeSignature const &ts) {
                          auto [aux, state] = action::append_measure(ps.timeline, ts);
                          ps.timeline.set_aux_state(std::move(aux), false);
                          ps.timeline.add_state(std::move(state));
                          return minfo("Appended Measure");
                      },
                      ArgInfo<sequence::TimeSignature>{"duration", {{4, 4}}})),

        cmd_group(
            "insert", ArgInfo<std::string>{"item", "measure"},

            cmd(
                "measure",
                "Insert a measure at the current location inside the current phrase.",
                [](PS &ps, sequence::TimeSignature const &ts) {
                    auto [aux, state] = action::insert_measure(ps.timeline, ts);
                    ps.timeline.set_aux_state(std::move(aux), false);
                    ps.timeline.add_state(std::move(state));
                    return minfo("Inserted Measure");
                },
                ArgInfo<sequence::TimeSignature>{"duration", {{4, 4}}})),

        cmd(
            "note", "Create a new Note, overwritting the current selection.",
            [](PS &ps, int interval, float velocity, float delay, float gate) {
                increment_state(
                    ps.timeline,
                    [](sequence::Cell const &, auto... args) -> sequence::Cell {
                        return sequence::modify::note(args...);
                    },
                    interval, velocity, delay, gate);
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
                return minfo("Rest Created");
            }),

        pattern(cmd("flip",
                    "Flips Notes to Rests and Rests to Notes for the current "
                    "selection. Works over "
                    "sequences.",
                    [](PS &ps, sequence::Pattern const &pattern) {
                        increment_state(ps.timeline, &sequence::modify::flip, pattern,
                                        sequence::Note{});
                        return minfo("Flipped Selection");
                    })),

        cmd_group("delete", ArgInfo<std::string>{"item", "selection"},

                  cmd("selection", "Delete the current selection.",
                      [](PS &ps) {
                          auto [aux, state] =
                              action::delete_cell(ps.timeline.get_aux_state(),
                                                  ps.timeline.get_state().first);
                          ps.timeline.set_aux_state(aux, false);
                          ps.timeline.add_state(std::move(state));
                          return minfo("Deleted Selection");
                      })),

        cmd(
            "split",
            "Duplicates the current selection into `count` equal parts, replacing the "
            "current selection.",
            [](PS &ps, std::size_t count) {
                increment_state(ps.timeline, &sequence::modify::repeat, count);
                return minfo("Split Selection " + std::to_string(count) + " Times");
            },
            ArgInfo<std::size_t>{"count", 2}),

        cmd("lift",
            "Bring the current selection up one level, replacing its parent sequence "
            "with "
            "itself.",
            [](PS &ps) {
                auto [state, aux] = action::lift(ps.timeline);
                ps.timeline.set_aux_state(aux, false);
                ps.timeline.add_state(state);
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
                    return minfo("Filled Selection With Rests");
                }))),

        pattern(cmd_group(
            "set", ArgInfo<std::string>{"trait"},

            cmd(
                "note", "Set the note interval of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, int interval) {
                    increment_state(ps.timeline, &sequence::modify::set_interval,
                                    pattern, interval);
                    return minfo("Note Set");
                },
                ArgInfo<int>{"interval", 0}),

            cmd(
                "octave", "Set the octave of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, int octave) {
                    ps.timeline.add_state(
                        action::set_note_octave(ps.timeline, pattern, octave));
                    return minfo("Octave Set");
                },
                ArgInfo<int>{"octave", 0}),

            cmd(
                "velocity", "Set the velocity of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float velocity) {
                    increment_state(ps.timeline, &sequence::modify::set_velocity,
                                    pattern, velocity);
                    return minfo("Velocity Set");
                },
                ArgInfo<float>{"velocity", 0.8f}),

            cmd(
                "delay", "Set the delay of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float delay) {
                    increment_state(ps.timeline, &sequence::modify::set_delay, pattern,
                                    delay);
                    return minfo("Delay Set");
                },
                ArgInfo<float>{"delay", 0.f}),

            cmd(
                "gate", "Set the gate of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float gate) {
                    increment_state(ps.timeline, &sequence::modify::set_gate, pattern,
                                    gate);
                    return minfo("Gate Set");
                },
                ArgInfo<float>{"gate", 1.f}),

            cmd(
                "timeSignature",
                "Set the time signature of the current Measure. Ignores Pattern.",
                [](PS &ps, sequence::Pattern const &,
                   sequence::TimeSignature const &ts) {
                    ps.timeline.add_state(action::set_timesignature(ps.timeline, ts));
                    return minfo("TimeSignature Set");
                },
                ArgInfo<sequence::TimeSignature>{"timesignature", {{4, 4}}}),

            cmd(
                "baseFrequency",
                "Set the base note (interval zero) frequency to `freq`.",
                [](PS &ps, sequence::Pattern const &, float freq) {
                    ps.timeline.add_state(
                        action::set_base_frequency(ps.timeline, freq));
                    return minfo("Base Frequency Set");
                },
                ArgInfo<float>{"freq", 440.f}),

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
                ArgInfo<std::string>{"name"}),

            cmd(
                "phraseName", "Set the name of the current Phrase to `name`.",
                [](PS &ps, sequence::Pattern const &, std::string name) {
                    auto aux = ps.timeline.get_aux_state();
                    aux.current_phrase_name = std::move(name);
                    ps.timeline.set_aux_state(std::move(aux));
                    return minfo("Phrase Name Set");
                },
                ArgInfo<std::string>{"name"}))),

        pattern(cmd_group(
            "shift", ArgInfo<std::string>{"trait"},

            cmd(
                "note", "Increment/Decrement the note interval of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, int amount) {
                    increment_state(ps.timeline, &sequence::modify::shift_interval,
                                    pattern, amount);
                    return minfo("Note Shifted");
                },
                ArgInfo<int>{"amount", 1}),

            cmd(
                "octave", "Increment/Decrement the octave of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, int amount) {
                    ps.timeline.add_state(
                        action::shift_note_octave(ps.timeline, pattern, amount));
                    return minfo("Octave Shifted");
                },
                ArgInfo<int>{"amount", 1}),

            cmd(
                "velocity", "Increment/Decrement the velocity of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float amount) {
                    increment_state(ps.timeline, &sequence::modify::shift_velocity,
                                    pattern, amount);
                    return minfo("Velocity Shifted");
                },
                ArgInfo<float>{"amount", 0.1f}),

            cmd(
                "delay", "Increment/Decrement the delay of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float amount) {
                    increment_state(ps.timeline, &sequence::modify::shift_delay,
                                    pattern, amount);
                    return minfo("Delay Shifted");
                },
                ArgInfo<float>{"amount", 0.1f}),

            cmd(
                "gate", "Increment/Decrement the gate of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float amount) {
                    increment_state(ps.timeline, &sequence::modify::shift_gate, pattern,
                                    amount);
                    return minfo("Gate Shifted");
                },
                ArgInfo<float>{"amount", 0.1f}))),

        pattern(cmd_group(
            "humanize", ArgInfo<InputMode>{"mode"},

            cmd(
                InputMode::Velocity,
                "Apply a random shift to the velocity of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float amount) {
                    increment_state(ps.timeline, &sequence::modify::humanize_velocity,
                                    pattern, amount);
                    return minfo("Humanized Velocity");
                },
                ArgInfo<float>{"amount", 0.1f}),

            cmd(
                InputMode::Delay,
                "Apply a random shift to the delay of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float amount) {
                    increment_state(ps.timeline, &sequence::modify::humanize_delay,
                                    pattern, amount);
                    return minfo("Humanized Delay");
                },
                ArgInfo<float>{"amount", 0.1f}),

            cmd(
                InputMode::Gate,
                "Apply a random shift to the gate of any selected Notes.",
                [](PS &ps, sequence::Pattern const &pattern, float amount) {
                    increment_state(ps.timeline, &sequence::modify::humanize_gate,
                                    pattern, amount);
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
                    return minfo("Randomized Note");
                },
                ArgInfo<int>{"min", -12}, ArgInfo<int>{"max", 12}),

            cmd(
                InputMode::Velocity,
                "Set the velocity of any selected Notes to a random value.",
                [](PS &ps, sequence::Pattern const &pattern, float min, float max) {
                    increment_state(ps.timeline, &sequence::modify::randomize_velocity,
                                    pattern, min, max);
                    return minfo("Randomized Velocity");
                },
                ArgInfo<float>{"min", 0.01f}, ArgInfo<float>{"max", 1.f}),

            cmd(
                InputMode::Delay,
                "Set the delay of any selected Notes to a random value.",
                [](PS &ps, sequence::Pattern const &pattern, float min, float max) {
                    increment_state(ps.timeline, &sequence::modify::randomize_delay,
                                    pattern, min, max);
                    return minfo("Randomized Delay");
                },
                ArgInfo<float>{"min", 0.f}, ArgInfo<float>{"max", 0.95f}),

            cmd(
                InputMode::Gate,
                "Set the gate of any selected Notes to a random value.",
                [](PS &ps, sequence::Pattern const &pattern, float min, float max) {
                    increment_state(ps.timeline, &sequence::modify::randomize_gate,
                                    pattern, min, max);
                    return minfo("Randomized Gate");
                },
                ArgInfo<float>{"min", 0.f}, ArgInfo<float>{"max", 0.95f}))),

        cmd("shuffle", "Randomly shuffle Notes and Rests in current selection.",
            [](PS &ps) {
                increment_state(ps.timeline, &sequence::modify::shuffle);
                return minfo("Selection Shuffled");
            }),

        cmd(
            "rotate",
            "Shift the Notes and Rests in the current selection by `amount`."
            " Positive values shift right, negative values shift left.",
            [](PS &ps, int amount) {
                increment_state(ps.timeline, &sequence::modify::rotate, amount);
                return minfo("Selection Rotated");
            },
            ArgInfo<int>{"amount", 1}),

        cmd("reverse",
            "Reverse the order of all Notes and Rests in the current selection.",
            [](PS &ps) {
                increment_state(ps.timeline, &sequence::modify::reverse);
                return minfo("Selection Reversed");
            }),

        pattern(cmd(
            "mirror",
            "Mirror the note intervals of the current selection around `centerNote`.",
            [](PS &ps, sequence::Pattern const &pattern, int center_note) {
                increment_state(ps.timeline, &sequence::modify::mirror, pattern,
                                center_note);
                return minfo("Selection Mirrored");
            },
            ArgInfo<int>{"centerNote", 0})),

        pattern(cmd("quantize",
                    "Set the delay to zero and gate to one for all Notes in the "
                    "current selection.",
                    [](PS &ps, sequence::Pattern const &pattern) {
                        increment_state(ps.timeline, &sequence::modify::quantize,
                                        pattern);
                        return minfo("Selection Quantized");
                    })),

        cmd(
            "swing",
            "Set the delay of every other Note in the current selection to `amount`.",
            [](PS &ps, float amount) {
                increment_state(ps.timeline, &sequence::modify::swing, amount, false);
                return minfo("Selection Swung by " + std::to_string(amount));
            },
            ArgInfo<float>{"amount", 0.1f}),

        // Temporary --------------------------------------------------------------

        cmd("demo", "Reset the state to a demo Phrase.", [](PS &ps) {
            ps.timeline.set_aux_state({{0, {}}},
                                      false); // Manually reset selection on overwrite
            ps.timeline.add_state(demo_state());
            return minfo("Demo State Loaded");
        }));
}

using XenCommandTree = decltype(create_command_tree());

} // namespace xen
