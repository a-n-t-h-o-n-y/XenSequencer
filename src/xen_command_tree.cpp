#include <xen/xen_command_tree.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include <sequence/pattern.hpp>
#include <sequence/sequence.hpp>

#include <xen/actions.hpp>
#include <xen/chord.hpp>
#include <xen/command.hpp>
#include <xen/constants.hpp>
#include <xen/gui/themes.hpp>
#include <xen/input_mode.hpp>
#include <xen/message_level.hpp>
#include <xen/scale.hpp>
#include <xen/state.hpp>
#include <xen/string_manip.hpp>
#include <xen/user_directory.hpp>

namespace xen
{

auto create_command_tree() -> XenCommandTree
{
    using PS = PluginState;

    auto head = CommandGroup{""};

    // welcome
    head.add(cmd(
        Signature{.id = "welcome"},
        [](PS &) { return minfo(std::string{"Welcome to XenSequencer v"} + VERSION); },
        "Display welcome message."));

    // version
    head.add(cmd(
        Signature{.id = "version"},
        [](PS &) { return minfo(std::string{"v"} + VERSION); },
        "Print the current version string."));

    // reset
    head.add(cmd(
        Signature{.id = "reset"},
        [](PS &ps) {
            ps.timeline.stage({SequencerState{}, AuxState{}});
            ps.timeline.set_commit_flag();
            ps.scale_shift_index = std::nullopt; // Chromatic
            return minfo("Plugin State Reset");
        },
        "Reset the timeline to a blank state."));

    // undo
    head.add(cmd(
        Signature{.id = "undo"},
        [](PS &ps) {
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
        },
        "Revert the last action."));

    // redo
    head.add(cmd(
        Signature{.id = "redo"},
        [](PS &ps) {
            return ps.timeline.redo() ? minfo("Redone") : minfo("Nothing to redo.");
        },
        "Reapply the last undone action."));

    // copy
    head.add(cmd(
        Signature{.id = "copy"},
        [](PS &ps) {
            action::copy(ps.timeline);
            return minfo("Copied Selection");
        },
        "Put the current selection in the copy buffer."));

    // cut
    head.add(cmd(
        Signature{.id = "cut"},
        [](PS &ps) {
            auto [_, aux] = ps.timeline.get_state();
            auto state = action::cut(ps.timeline);
            ps.timeline.stage({std::move(state), std::move(aux)});
            ps.timeline.set_commit_flag();
            return minfo("Cut Selection");
        },
        "Put the current selection in the copy buffer and replace it with a Rest."));

    // paste
    head.add(cmd(
        Signature{.id = "paste"},
        [](PS &ps) {
            auto [_, aux] = ps.timeline.get_state();
            ps.timeline.stage({action::paste(ps.timeline), std::move(aux)});
            ps.timeline.set_commit_flag();
            return minfo("Pasted Over Selection");
        },
        "Overwrite the current selection with what is stored in the copy buffer."));

    // duplicate
    head.add(cmd(
        Signature{.id = "duplicate"},
        [](PS &ps) {
            ps.timeline.stage(action::duplicate(ps.timeline));
            ps.timeline.set_commit_flag();
            return minfo("Duplicated Selection");
        },
        "Duplicate the current selection by placing it in the right-adjacent Cell."));

    // inputMode
    head.add(cmd(
        Signature{
            .id = "inputMode",
            .args = std::tuple{ArgInfo<InputMode>{"mode"}},
        },
        [](PS &ps, InputMode mode) {
            auto [state, _] = ps.timeline.get_state();
            ps.timeline.stage({
                std::move(state),
                action::set_input_mode(ps.timeline, mode),
            });
            return minfo("Input Mode Set to " + single_quote(to_string(mode)));
        },
        "Change the input mode. This determines the behavior of the up/down keys."));

    // focus
    head.add(cmd(
        Signature{
            .id = "focus",
            .args = std::tuple{ArgInfo<std::string>{"component_id"}},
        },
        [](PS &ps, std::string const &component_id) {
            ps.on_focus_request(component_id);
            return mdebug("Focused on " + component_id);
        },
        "Focus on a specific component."));

    // show
    head.add(cmd(
        Signature{
            .id = "show",
            .args = std::tuple{ArgInfo<std::string>{"component_id"}},
        },
        [](PS &ps, std::string const &component_id) {
            ps.on_show_request(component_id);
            return mdebug("Showing " + single_quote(component_id));
        },
        "Update the GUI to display the specified component."));

    {
        auto load = cmd_group("load");

        // load sequenceBank
        load->add(cmd(
            Signature{
                .id = "sequenceBank",
                .args = std::tuple{ArgInfo<std::string>{"filename"}},
            },
            [](PS &ps, std::string const &filename) {
                auto const cd = ps.current_phrase_directory;
                if (!cd.isDirectory())
                {
                    return merror("Invalid Current Phrase Directory");
                }

                auto const filepath = cd.getChildFile(filename + ".xss");
                if (!filepath.exists())
                {
                    return merror("File Not Found: " +
                                  filepath.getFullPathName().toStdString());
                }

                auto [state, aux] = ps.timeline.get_state();
                state.sequence_bank = action::load_sequence_bank(
                    filepath.getFullPathName().toStdString());

                ps.timeline.stage({std::move(state), std::move(aux)});
                ps.timeline.set_commit_flag();

                return minfo("Sequence Bank Loaded");
            },
            "Load the entire sequence bank into the plugin from file. filename must be "
            "located in the library's currently set sequence directory. Do not include "
            "the .xss extension in the filename you provide."));

        // load tuning
        load->add(cmd(
            Signature{
                .id = "tuning",
                .args = std::tuple{ArgInfo<std::string>{"filename"}},
            },
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
                seq.tuning_name = filepath.getFileNameWithoutExtension().toStdString();
                seq.tuning =
                    sequence::from_scala(filepath.getFullPathName().toStdString());

                ps.timeline.stage({std::move(seq), std::move(aux)});
                ps.timeline.set_commit_flag();

                return minfo("Tuning Loaded");
            },
            "Load a tuning file (.scl) from the current `tunings` Library directory. "
            "Do not include the .scl extension in the filename you provide."));

        // load keys
        load->add(cmd(
            Signature{.id = "keys"},
            [](PS &ps) {
                try
                {
                    auto const lock = std::lock_guard{
                        ps.shared.on_load_keys_request_mtx,
                    };
                    ps.shared.on_load_keys_request();
                    return minfo("Key Config Loaded");
                }
                catch (std::exception const &e)
                {
                    return merror("Failed to Load Keys: " + std::string{e.what()});
                }
            },
            "Load keys.yml and user_keys.yml."));

        // load scales
        load->add(cmd(
            Signature{.id = "scales"},
            [](PS &ps) {
                ps.scales = load_scales_from_files();
                return minfo("Scales Loaded: " + std::to_string(ps.scales.size()));
            },
            "Load scales.yml and user_scales.yml."));

        // load chords
        load->add(cmd(
            Signature{.id = "chords"},
            [](PS &ps) {
                ps.chords = load_chords_from_files();
                return minfo("Chords Loaded: " + std::to_string(ps.chords.size()));
            },
            "Load chords.yml and user_chords.yml."));

        head.add(std::move(load));
    }

    {
        auto save = cmd_group("save");

        // save sequenceBank
        save->add(cmd(
            Signature{
                .id = "sequenceBank",
                .args = std::tuple{ArgInfo<std::string>{"filename"}},
            },
            [](PS &ps, std::string const &filename) {
                auto const cd = ps.current_phrase_directory;
                if (!cd.isDirectory())
                {
                    return merror("Invalid Current Phrase Directory");
                }

                auto const filepath =
                    cd.getChildFile(filename + ".xss").getFullPathName().toStdString();

                auto const [state, _] = ps.timeline.get_state();
                action::save_sequence_bank(state.sequence_bank, filepath);
                return minfo("Sequence Bank Saved to " + single_quote(filepath));
            },
            "Save the entire sequence bank to a file. The file will be located in "
            "the library's current sequence directory. Do not include the .xss "
            "extension in the filename you provide."));

        head.add(std::move(save));
    }

    // libraryDirectory
    head.add(cmd(
        Signature{.id = "libraryDirectory"},
        [](PS &) {
            return minfo(get_user_library_directory().getFullPathName().toStdString());
        },
        "Display the path to the directory where the user library is stored."));

    {
        auto move = cmd_group("move");

        // move left
        move->add(cmd(
            Signature{.id = "left",
                      .args = std::tuple{ArgInfo<std::size_t>{"amount", 1}}},
            [](PS &ps, std::size_t amount) {
                auto [state, _] = ps.timeline.get_state();
                ps.timeline.stage({
                    std::move(state),
                    action::move_left(ps.timeline, amount),
                });
                return mdebug("Moved Left " + std::to_string(amount) + " Times");
            },
            "Move the selection left, or wrap around."));

        // move right
        move->add(cmd(
            Signature{.id = "right",
                      .args = std::tuple{ArgInfo<std::size_t>{"amount", 1}}},
            [](PS &ps, std::size_t amount) {
                auto [state, _] = ps.timeline.get_state();
                ps.timeline.stage({
                    std::move(state),
                    action::move_right(ps.timeline, amount),
                });
                return mdebug("Moved Right " + std::to_string(amount) + " Times");
            },
            "Move the selection right, or wrap around."));

        // move up
        move->add(cmd(
            Signature{.id = "up",
                      .args = std::tuple{ArgInfo<std::size_t>{"amount", 1}}},
            [](PS &ps, std::size_t amount) {
                auto [state, _] = ps.timeline.get_state();
                ps.timeline.stage({
                    std::move(state),
                    action::move_up(ps.timeline, amount),
                });
                return mdebug("Moved Up " + std::to_string(amount) + " Times");
            },
            "Move the selection up one level to a parent sequence."));

        // move down
        move->add(cmd(
            Signature{.id = "down",
                      .args = std::tuple{ArgInfo<std::size_t>{"amount", 1}}},
            [](PS &ps, std::size_t amount) {
                auto [state, _] = ps.timeline.get_state();
                ps.timeline.stage({
                    std::move(state),
                    action::move_down(ps.timeline, amount),
                });
                return mdebug("Moved Down " + std::to_string(amount) + " Times");
            },
            "Move the selection down one level."));

        head.add(std::move(move));
    }

    // note
    head.add(cmd(
        Signature{
            .id = "note",
            .args =
                std::tuple{
                    ArgInfo<int>{"pitch", 0},
                    ArgInfo<float>{"velocity", 0.8f},
                    ArgInfo<float>{"delay", 0.f},
                    ArgInfo<float>{"gate", 1.f},
                },
        },
        [](PS &ps, int pitch, float velocity, float delay, float gate) {
            increment_state(
                ps.timeline,
                [](sequence::Cell const &, auto... args) -> sequence::Cell {
                    return sequence::modify::note(args...);
                },
                pitch, velocity, delay, gate);
            ps.timeline.set_commit_flag();
            return minfo("Note Created");
        },
        "Create a new Note, overwritting the current selection."));

    // rest
    head.add(cmd(
        Signature{.id = "rest"},
        [](PS &ps) {
            increment_state(ps.timeline, [](sequence::Cell const &) -> sequence::Cell {
                return sequence::modify::rest();
            });
            ps.timeline.set_commit_flag();
            return minfo("Rest Created");
        },
        "Create a new Rest, overwritting the current selection."));

    // delete
    head.add(cmd(
        Signature{.id = "delete"},
        [](PS &ps) {
            ps.timeline.stage(action::delete_cell(ps.timeline.get_state()));
            ps.timeline.set_commit_flag();
            return minfo("Deleted Selection");
        },
        "Delete the current selection."));

    // split
    head.add(cmd(
        Signature{.id = "split", .args = std::tuple{ArgInfo<std::size_t>{"count", 2}}},
        [](PS &ps, std::size_t count) {
            increment_state(ps.timeline, &sequence::modify::repeat, count);
            ps.timeline.set_commit_flag();
            return minfo("Split Selection " + std::to_string(count) + " Times");
        },
        "Duplicates the current selection into `count` equal parts, replacing the "
        "current selection."));

    // lift
    head.add(cmd(
        Signature{.id = "lift"},
        [](PS &ps) {
            ps.timeline.stage(action::lift(ps.timeline));
            ps.timeline.set_commit_flag();
            return minfo("Selection Lifted One Layer");
        },
        "Bring the current selection up one level, replacing its parent sequence "
        "with itself."));

    // flip
    head.add(cmd(
        PatternedSignature{.id = "flip"},
        [](PS &ps, sequence::Pattern const &pattern) {
            increment_state(ps.timeline, &sequence::modify::flip, pattern,
                            sequence::Note{});
            ps.timeline.set_commit_flag();
            return minfo("Flipped Selection");
        },
        "Flips Notes to Rests and Rests to Notes for the current selection. Works over "
        "sequences."));

    {
        auto fill = cmd_group("fill");

        // fill note
        fill->add(cmd(
            PatternedSignature{
                .id = "note",
                .args =
                    std::tuple{
                        ArgInfo<int>{"pitch", 0},
                        ArgInfo<float>{"velocity", 0.8f},
                        ArgInfo<float>{"delay", 0.f},
                        ArgInfo<float>{"gate", 1.f},
                    },
            },
            [](PS &ps, sequence::Pattern const &pattern, int pitch, float velocity,
               float delay, float gate) {
                increment_state(ps.timeline, &sequence::modify::notes_fill, pattern,
                                sequence::Note{pitch, velocity, delay, gate});
                ps.timeline.set_commit_flag();
                return minfo("Filled Selection With Notes");
            },
            "Fill the current selection with Notes, this works specifically over "
            "sequences."));

        // fill rest
        fill->add(cmd(
            PatternedSignature{.id = "rest"},
            [](PS &ps, sequence::Pattern const &pattern) {
                increment_state(ps.timeline, &sequence::modify::rests_fill, pattern);
                ps.timeline.set_commit_flag();
                return minfo("Filled Selection With Rests");
            },
            "Fill the current selection with Rests, this works specifically over "
            "sequences."));

        head.add(std::move(fill));
    }

    {
        auto select = cmd_group("select");

        // select sequence
        select->add(cmd(
            Signature{.id = "sequence", .args = std::tuple{ArgInfo<int>{"index"}}},
            [](PS &ps, int index) {
                auto [seq, aux] = ps.timeline.get_state();
                if (aux.selected.measure == (std::size_t)index)
                {
                    return mwarning("Already Selected");
                }
                aux = action::set_selected_sequence(aux, index);
                ps.timeline.stage({std::move(seq), std::move(aux)});
                return mdebug("Sequence " + std::to_string(index) + " Selected");
            },
            "Change the current sequence from the SequenceBank to `index`. "
            "Zero-based."));

        head.add(std::move(select));
    }

    {
        auto set = cmd_group("set");

        // set pitch
        set->add(cmd(
            PatternedSignature{
                .id = "pitch",
                .args = std::tuple{ArgInfo<int>{"pitch", 0}},
            },
            [](PS &ps, sequence::Pattern const &pattern, int pitch) {
                increment_state(ps.timeline, &sequence::modify::set_pitch, pattern,
                                pitch);
                ps.timeline.set_commit_flag();
                return minfo("Note Set");
            },
            "Set the pitch of all selected Notes."));

        // set octave
        set->add(cmd(
            PatternedSignature{
                .id = "octave",
                .args = std::tuple{ArgInfo<int>{"octave", 0}},
            },
            [](PS &ps, sequence::Pattern const &pattern, int octave) {
                auto [_, aux] = ps.timeline.get_state();
                ps.timeline.stage({
                    action::set_note_octave(ps.timeline, pattern, octave),
                    std::move(aux),
                });
                ps.timeline.set_commit_flag();
                return minfo("Octave Set");
            },
            "Set the octave of all selected Notes."));

        // set velocity
        set->add(cmd(
            PatternedSignature{
                .id = "velocity",
                .args = std::tuple{ArgInfo<float>{"velocity", 0.8f}},
            },
            [](PS &ps, sequence::Pattern const &pattern, float velocity) {
                increment_state(ps.timeline, &sequence::modify::set_velocity, pattern,
                                velocity);
                ps.timeline.set_commit_flag();
                return minfo("Velocity Set");
            },
            "Set the velocity of all selected Notes."));

        // set delay
        set->add(cmd(
            PatternedSignature{
                .id = "delay",
                .args = std::tuple{ArgInfo<float>{"delay", 0.f}},
            },
            [](PS &ps, sequence::Pattern const &pattern, float delay) {
                increment_state(ps.timeline, &sequence::modify::set_delay, pattern,
                                delay);
                ps.timeline.set_commit_flag();
                return minfo("Delay Set");
            },
            "Set the delay of all selected Notes."));

        // set gate
        set->add(cmd(
            PatternedSignature{
                .id = "gate",
                .args = std::tuple{ArgInfo<float>{"gate", 1.f}},
            },
            [](PS &ps, sequence::Pattern const &pattern, float gate) {
                increment_state(ps.timeline, &sequence::modify::set_gate, pattern,
                                gate);
                ps.timeline.set_commit_flag();
                return minfo("Gate Set");
            },
            "Set the gate of all selected Notes."));

        {
            auto measure = cmd_group("measure");

            // set measure name
            measure->add(cmd(
                Signature{
                    .id = "name",
                    .args =
                        std::tuple{
                            ArgInfo<std::string>{"name"},
                            ArgInfo<int>{"index", -1},
                        },
                },
                [](PS &ps, std::string name, int index) {
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
                "Set the name of a Measure. If no index is given, set the name of the "
                "current Measure."));

            // set measure timeSignature
            measure->add(cmd(
                Signature{
                    .id = "timeSignature",
                    .args =
                        std::tuple{
                            ArgInfo<sequence::TimeSignature>{"timesignature", {{4, 4}}},
                            ArgInfo<int>{"index", -1},
                        },
                },
                [](PS &ps, sequence::TimeSignature const &ts, int index) {
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
                "Set the time signature of a Measure. If no index is given, set the "
                "time signature of the current Measure."));

            set->add(std::move(measure));
        }

        // set baseFrequency
        set->add(cmd(
            Signature{
                .id = "baseFrequency",
                .args = std::tuple{ArgInfo<float>{"freq", 440.f}},
            },
            [](PS &ps, float freq) {
                auto [_, aux] = ps.timeline.get_state();
                ps.timeline.stage({
                    action::set_base_frequency(ps.timeline, freq),
                    std::move(aux),
                });
                ps.timeline.set_commit_flag();
                return minfo("Base Frequency Set");
            },
            "Set the base note (pitch zero) frequency to `freq` Hz."));

        // set theme
        set->add(cmd(
            Signature{
                .id = "theme",
                .args = std::tuple{ArgInfo<std::string>{"name"}},
            },
            [](PS &ps, std::string name) {
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
            "Set the color theme of the app by name."));

        // set scale
        set->add(cmd(
            Signature{
                .id = "scale",
                .args = std::tuple{ArgInfo<std::string>{"name"}},
            },
            [](PS &ps, std::string name) {
                name = to_lower(name);
                if (name == "chromatic")
                {
                    auto state = ps.timeline.get_state();
                    state.sequencer.scale = std::nullopt;
                    ps.timeline.stage(std::move(state));
                    ps.timeline.set_commit_flag();
                    return minfo("Scale Set to " + name + ".");
                }
                // Scale names are stored as all lower case.
                auto const at = std::ranges::find(
                    ps.scales, name, [](Scale const &s) { return s.name; });
                if (at != std::end(ps.scales))
                {
                    auto state = ps.timeline.get_state();
                    state.sequencer.scale = *at;
                    ps.timeline.stage(std::move(state));
                    ps.timeline.set_commit_flag();
                    return minfo("Scale Set to " + name + ".");
                }
                else
                {
                    return merror("No Scale Found: " + name + ".");
                }
            },
            "Set the current scale by name."));

        // set mode
        set->add(cmd(
            Signature{
                .id = "mode",
                .args = std::tuple{ArgInfo<std::size_t>{"mode_index"}},
            },
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
                ps.timeline.set_commit_flag();
                return minfo("Scale Mode Set");
            },
            "Set the mode of the current scale. [1, scale size]."));

        // set translateDirection
        set->add(cmd(
            Signature{
                .id = "translateDirection",
                .args = std::tuple{ArgInfo<std::string>{"direction"}},
            },
            [](PS &ps, std::string direction) {
                direction = to_lower(direction);
                auto state = ps.timeline.get_state();
                if (direction == "up")
                {
                    state.sequencer.scale_translate_direction = TranslateDirection::Up;
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
                ps.timeline.set_commit_flag();
                return minfo("Translate Direction Set");
            },
            "Set the Scale's translate direction to either Up or Down."));

        // set key
        set->add(cmd(
            Signature{
                .id = "key",
                .args = std::tuple{ArgInfo<int>{"key", 0}},
            },
            [](PS &ps, int key) {
                auto state = ps.timeline.get_state();
                state.sequencer.key = key;
                ps.timeline.stage(std::move(state));
                ps.timeline.set_commit_flag();
                return minfo("Key Set to " + std::to_string(key) + ".");
            },
            "Set the key to tranpose to, any integer value is valid."));

        head.add(std::move(set));
    }

    {
        auto shift = cmd_group("shift");

        // shift pitch
        shift->add(cmd(
            PatternedSignature{
                .id = "pitch",
                .args = std::tuple{ArgInfo<int>{"amount", 1}},
            },
            [](PS &ps, sequence::Pattern const &pattern, int amount) {
                increment_state(ps.timeline, &sequence::modify::shift_pitch, pattern,
                                amount);
                ps.timeline.set_commit_flag();
                return minfo("Pitch Shifted");
            },
            "Increment/Decrement the pitch of all selected Notes."));

        // shift octave
        shift->add(cmd(
            PatternedSignature{
                .id = "octave",
                .args = std::tuple{ArgInfo<int>{"amount", 1}},
            },
            [](PS &ps, sequence::Pattern const &pattern, int amount) {
                auto [_, aux] = ps.timeline.get_state();
                ps.timeline.stage({
                    action::shift_octave(ps.timeline, pattern, amount),
                    std::move(aux),
                });
                ps.timeline.set_commit_flag();
                return minfo("Octave Shifted");
            },
            "Increment/Decrement the octave of all selected Notes."));

        // shift velocity
        shift->add(cmd(
            PatternedSignature{
                .id = "velocity",
                .args = std::tuple{ArgInfo<float>{"amount", 0.1f}},
            },
            [](PS &ps, sequence::Pattern const &pattern, float amount) {
                increment_state(ps.timeline, &sequence::modify::shift_velocity, pattern,
                                amount);
                ps.timeline.set_commit_flag();
                return minfo("Velocity Shifted");
            },
            "Increment/Decrement the velocity of all selected Notes."));

        // shift delay
        shift->add(cmd(
            PatternedSignature{
                .id = "delay",
                .args = std::tuple{ArgInfo<float>{"amount", 0.1f}},
            },
            [](PS &ps, sequence::Pattern const &pattern, float amount) {
                increment_state(ps.timeline, &sequence::modify::shift_delay, pattern,
                                amount);
                ps.timeline.set_commit_flag();
                return minfo("Delay Shifted");
            },
            "Increment/Decrement the delay of all selected Notes."));

        // shift gate
        shift->add(cmd(
            PatternedSignature{
                .id = "gate",
                .args = std::tuple{ArgInfo<float>{"amount", 0.1f}},
            },
            [](PS &ps, sequence::Pattern const &pattern, float amount) {
                increment_state(ps.timeline, &sequence::modify::shift_gate, pattern,
                                amount);
                ps.timeline.set_commit_flag();
                return minfo("Gate Shifted");
            },
            "Increment/Decrement the gate of all selected Notes."));

        // shift selectedSequence
        shift->add(cmd(
            Signature{
                .id = "selectedSequence",
                .args = std::tuple{ArgInfo<int>{"amount"}},
            },
            [](PS &ps, int amount) {
                auto [seq, aux] = ps.timeline.get_state();
                auto const size = (int)seq.sequence_bank.size();
                auto const index =
                    (((int)aux.selected.measure + amount) % size + size) % size;
                aux = action::set_selected_sequence(aux, index);
                ps.timeline.stage({std::move(seq), std::move(aux)});
                return mdebug("Selected Sequence Shifted");
            },
            "Change the selected/displayed sequence by `amount`. This wraps around "
            "edges of the SequenceBank. `amount` can be positive or negative."));

        // shift scale
        shift->add(cmd(
            Signature{
                .id = "scale",
                .args = std::tuple{ArgInfo<int>{"amount", 1}},
            },
            [](PS &ps, int amount) {
                auto [seq, aux] = ps.timeline.get_state();
                auto const index = action::shift_scale_index(ps.scale_shift_index,
                                                             amount, ps.scales.size());
                ps.scale_shift_index = index;
                if (index.has_value() && *index < ps.scales.size())
                {
                    seq.scale = ps.scales[*index];
                }
                else
                {
                    seq.scale = std::nullopt;
                }
                ps.timeline.stage({std::move(seq), std::move(aux)});
                ps.timeline.set_commit_flag();
                return minfo("Scale Shifted");
            },
            "Move Forward/Backward through the loaded Scales."));

        // shift scaleMode
        shift->add(cmd(
            Signature{
                .id = "scaleMode",
                .args = std::tuple{ArgInfo<int>{"amount", 1}},
            },
            [](PS &ps, int amount) {
                auto [seq, aux] = ps.timeline.get_state();
                if (seq.scale.has_value())
                {
                    seq.scale = action::shift_scale_mode(*seq.scale, amount);
                    ps.timeline.stage({std::move(seq), std::move(aux)});
                    ps.timeline.set_commit_flag();
                }
                return minfo("Scale Mode Shifted");
            },
            "Increment/Decrement the mode of the current scale."));

        head.add(std::move(shift));
    }

    {
        auto humanize = cmd_group("humanize");

        // humanize velocity
        humanize->add(cmd(
            PatternedSignature{
                .id = "velocity",
                .args = std::tuple{ArgInfo<float>{"amount", 0.1f}},
            },
            [](PS &ps, sequence::Pattern const &pattern, float amount) {
                increment_state(ps.timeline, &sequence::modify::humanize_velocity,
                                pattern, amount);
                ps.timeline.set_commit_flag();
                return minfo("Humanized Velocity");
            },
            "Apply a random shift to the velocity of any selected Notes."));

        // humanize delay
        humanize->add(cmd(
            PatternedSignature{
                .id = "delay",
                .args = std::tuple{ArgInfo<float>{"amount", 0.1f}},
            },
            [](PS &ps, sequence::Pattern const &pattern, float amount) {
                increment_state(ps.timeline, &sequence::modify::humanize_delay, pattern,
                                amount);
                ps.timeline.set_commit_flag();
                return minfo("Humanized Delay");
            },
            "Apply a random shift to the delay of any selected Notes."));

        // humanize gate
        humanize->add(cmd(
            PatternedSignature{
                .id = "gate",
                .args = std::tuple{ArgInfo<float>{"amount", 0.1f}},
            },
            [](PS &ps, sequence::Pattern const &pattern, float amount) {
                increment_state(ps.timeline, &sequence::modify::humanize_gate, pattern,
                                amount);
                ps.timeline.set_commit_flag();
                return minfo("Humanized Gate");
            },
            "Apply a random shift to the gate of any selected Notes."));

        head.add(std::move(humanize));
    }

    {
        auto randomize = cmd_group("randomize");

        // randomize pitch
        randomize->add(cmd(
            PatternedSignature{
                .id = "pitch",
                .args = std::tuple{ArgInfo<int>{"min", -12}, ArgInfo<int>{"max", 12}},
            },
            [](PS &ps, sequence::Pattern const &pattern, int min, int max) {
                increment_state(ps.timeline, &sequence::modify::randomize_pitch,
                                pattern, min, max);
                ps.timeline.set_commit_flag();
                return minfo("Randomized Pitch");
            },
            "Set the pitch of any selected Notes to a random value."));

        // randomize velocity
        randomize->add(cmd(
            PatternedSignature{
                .id = "velocity",
                .args = std::tuple{ArgInfo<float>{"min", 0.01f},
                                   ArgInfo<float>{"max", 1.f}},
            },
            [](PS &ps, sequence::Pattern const &pattern, float min, float max) {
                increment_state(ps.timeline, &sequence::modify::randomize_velocity,
                                pattern, min, max);
                ps.timeline.set_commit_flag();
                return minfo("Randomized Velocity");
            },
            "Set the velocity of any selected Notes to a random value."));

        // randomize delay
        randomize->add(cmd(
            PatternedSignature{
                .id = "delay",
                .args = std::tuple{ArgInfo<float>{"min", 0.f},
                                   ArgInfo<float>{"max", 0.95f}},
            },
            [](PS &ps, sequence::Pattern const &pattern, float min, float max) {
                increment_state(ps.timeline, &sequence::modify::randomize_delay,
                                pattern, min, max);
                ps.timeline.set_commit_flag();
                return minfo("Randomized Delay");
            },
            "Set the delay of any selected Notes to a random value."));

        // randomize gate
        randomize->add(cmd(
            PatternedSignature{
                .id = "gate",
                .args = std::tuple{ArgInfo<float>{"min", 0.f},
                                   ArgInfo<float>{"max", 0.95f}},
            },
            [](PS &ps, sequence::Pattern const &pattern, float min, float max) {
                increment_state(ps.timeline, &sequence::modify::randomize_gate, pattern,
                                min, max);
                ps.timeline.set_commit_flag();
                return minfo("Randomized Gate");
            },
            "Set the gate of any selected Notes to a random value."));

        head.add(std::move(randomize));
    }

    // stretch
    head.add(cmd(
        PatternedSignature{.id = "stretch",
                           .args = std::tuple{ArgInfo<std::size_t>{"count", 2}}},
        [](PS &ps, sequence::Pattern const &pattern, std::size_t count) {
            increment_state(ps.timeline, &sequence::modify::stretch, pattern, count);
            ps.timeline.set_commit_flag();
            return minfo("Stretched Selection by " + std::to_string(count));
        },
        "Duplicates items in the current selection `count` times, replacing the "
        "current selection.\n\nThis is similar to `split`, the difference is this "
        "does not split sequences, it will traverse until it finds a Note or Rest "
        "and will then duplicate it. This can also take a Pattern, whereas split "
        "cannot."));

    // compress
    head.add(cmd(
        PatternedSignature{.id = "compress"},
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
        },
        "Keep items from the current selection that match the given Pattern, "
        "replacing the current selection."));

    // shuffle
    head.add(cmd(
        Signature{.id = "shuffle"},
        [](PS &ps) {
            increment_state(ps.timeline, &sequence::modify::shuffle);
            ps.timeline.set_commit_flag();
            return minfo("Selection Shuffled");
        },
        "Randomly shuffle Notes and Rests in current selection."));

    // rotate
    head.add(cmd(
        Signature{.id = "rotate", .args = std::tuple{ArgInfo<int>{"amount", 1}}},
        [](PS &ps, int amount) {
            increment_state(ps.timeline, &sequence::modify::rotate, amount);
            ps.timeline.set_commit_flag();
            return minfo("Selection Rotated");
        },
        "Shift Cells in the current selection by `amount`.\n\nPositive values shift "
        "right, negative values shift left."));

    // reverse
    head.add(cmd(
        Signature{.id = "reverse"},
        [](PS &ps) {
            increment_state(ps.timeline, &sequence::modify::reverse);
            ps.timeline.set_commit_flag();
            return minfo("Selection Reversed");
        },
        "Reverse the order of all Notes and Rests in the current selection."));

    // mirror
    head.add(cmd(
        PatternedSignature{
            .id = "mirror",
            .args = std::tuple{ArgInfo<int>{"centerPitch", 0}},
        },
        [](PS &ps, sequence::Pattern const &pattern, int center_pitch) {
            increment_state(ps.timeline, &sequence::modify::mirror, pattern,
                            center_pitch);
            ps.timeline.set_commit_flag();
            return minfo("Selection Mirrored");
        },
        "Mirror the note pitches of the current selection around `centerPitch`."));

    // quantize
    head.add(cmd(
        PatternedSignature{.id = "quantize"},
        [](PS &ps, sequence::Pattern const &pattern) {
            increment_state(ps.timeline, &sequence::modify::quantize, pattern);
            ps.timeline.set_commit_flag();
            return minfo("Selection Quantized");
        },
        "Set the delay to zero and gate to one for all Notes in the current "
        "selection."));

    // swing
    head.add(cmd(
        Signature{.id = "swing", .args = std::tuple{ArgInfo<float>{"amount", 0.1f}}},
        [](PS &ps, float amount) {
            increment_state(ps.timeline, &sequence::modify::swing, amount, false);
            ps.timeline.set_commit_flag();
            return minfo("Selection Swung by " + std::to_string(amount));
        },
        "Set the delay of every other Note in the current selection to `amount`."));

    // step
    head.add(cmd(
        Signature{
            .id = "step",
            .args = std::tuple{ArgInfo<std::size_t>{"count", 1},
                               ArgInfo<int>{"pitchDistance", 0},
                               ArgInfo<float>{"velocityDistance", 0.1f}},
        },
        [](PS &ps, std::size_t count, int pitch_distance, float velocity_distance) {
            auto [state, aux] = ps.timeline.get_state();
            auto &selected = get_selected_cell(state.sequence_bank, aux.selected);
            selected = action::step(selected, count, pitch_distance, velocity_distance);
            ps.timeline.stage({std::move(state), std::move(aux)});
            ps.timeline.set_commit_flag();
            return minfo("Stepped");
        },
        "Repeat the selected Cell with incrementing pitch and velocity applied."));

    // arp
    head.add(cmd(
        PatternedSignature{
            .id = "arp",
            .args =
                std::tuple{
                    ArgInfo<std::string>{"chord", "cycle"},
                    ArgInfo<int>{"inversion", -1},
                },
        },
        [](PS &ps, sequence::Pattern const &pattern, std::string chord_name,
           int inversion) {
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
                    find_next_chord(ps.chords, aux.arp_state.previous_chord_name).name;
                auto const chord = find_chord(ps.chords, chord_name);
                inversion = std::min(inversion, (int)chord.intervals.size() - 1);
            }
            else if (chord_name != "cycle" && inversion == -1)
            {
                auto const chord = find_chord(ps.chords, chord_name);
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
                    auto const chord = find_chord(ps.chords, chord_name);
                    inversion =
                        increment_inversion(chord, aux.arp_state.previous_inversion);
                }
                if (inversion == 0)
                {
                    chord_name = find_next_chord(ps.chords, chord_name).name;
                }
            }

            // chord_name and inversion are now valid.
            aux.arp_state.previous_chord_name = chord_name;
            aux.arp_state.previous_inversion = inversion;
            aux.arp_state.previous_commit_id = ps.timeline.get_next_commit_id();

            state = aux.arp_state.sequencer;
            aux.selected = aux.arp_state.selected;

            auto &selected = get_selected_cell(state.sequence_bank, aux.selected);
            auto const chord = find_chord(ps.chords, chord_name);
            auto const intervals =
                invert_chord(chord, inversion, state.tuning.intervals.size());
            selected = action::arp(selected, pattern, intervals);

            ps.timeline.stage({std::move(state), std::move(aux)});
            ps.timeline.set_commit_flag();

            return minfo("Arpeggiated with " + chord_name +
                         " inversion: " + std::to_string(inversion));
        },
        "Plays a given chord across the current selection, each interval in the "
        "chord is applied in order to child cells in the selection."));

    return head;
}

} // namespace xen