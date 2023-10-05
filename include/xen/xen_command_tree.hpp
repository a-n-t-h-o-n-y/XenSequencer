#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

#include <sequence/sequence.hpp>
#include <signals_light/signal.hpp>

#include <xen/actions.hpp>
#include <xen/command.hpp>
#include <xen/input_mode.hpp>
#include <xen/message_type.hpp>
#include <xen/xen_timeline.hpp>

namespace xen
{

inline auto on_focus_change_request = sl::Signal<void(std::string const &)>{};

inline auto copy_buffer = std::optional<sequence::Cell>{std::nullopt};

inline auto const command_tree = cmd_group(
    "", ArgInfo<std::string>{"command_name"},

    cmd("undo",
        [](XenTimeline &tl) {
            return tl.undo() ? msuccess("Undo Successful") : mwarning("Can't Undo");
        }),

    cmd("redo",
        [](XenTimeline &tl) {
            return tl.redo() ? msuccess("Redo Successful") : mwarning("Can't Redo");
        }),

    cmd("copy",
        [](XenTimeline &tl) {
            auto copy = action::copy(tl);
            if (copy.has_value())
            {
                copy_buffer = std::move(*copy);
                return msuccess("Copied.");
            }
            return mwarning("Nothing to copy.");
        }),

    cmd("cut",
        [](XenTimeline &tl) {
            auto cut = action::cut(tl);
            if (!cut.has_value())
            {
                return mwarning("Nothing to cut.");
            }
            auto &[buffer, state] = *cut;
            tl.add_state(std::move(state));
            copy_buffer = std::move(buffer);
            return msuccess("Cut.");
        }),

    cmd("paste",
        [](XenTimeline &tl) {
            auto const result = action::paste(tl, copy_buffer);
            if (result.has_value())
            {
                tl.add_state(*result);
                return msuccess("Pasted.");
            }
            return mwarning("Nothing to paste.");
        }),

    cmd("duplicate",
        [](XenTimeline &tl) {
            auto [aux, state] = action::duplicate(tl);
            tl.set_aux_state(std::move(aux), false);
            tl.add_state(std::move(state));
            return msuccess("Duplicated.");
        }),

    cmd(
        "mode",
        [](XenTimeline &tl, InputMode mode) {
            tl.set_aux_state(action::set_mode(tl, mode));
            return msuccess("Changed mode to '" + to_string(mode) + "'.");
        },
        ArgInfo<InputMode>{"mode"}),

    cmd(
        "focus",
        [](auto &, std::string const &name) {
            on_focus_change_request(name);
            return msuccess("Focused on '" + name + "'.");
        },
        ArgInfo<std::string>{"component"}),

    cmd(
        "save",
        [](XenTimeline &tl, std::string const &filepath) {
            action::save_state(tl, filepath);
            return msuccess("Saved state to '" + filepath + "'.");
        },
        ArgInfo<std::filesystem::path>{"filepath"}),

    cmd(
        "load",
        [](XenTimeline &tl, std::string const &filepath) {
            // Call first in case of error
            auto new_state = action::load_state(filepath);
            tl.set_aux_state({{0, {}}}, false); // Manually reset selection on overwrite
            tl.add_state(std::move(new_state));
            return msuccess("Loaded state from '" + filepath + "'.");
        },
        ArgInfo<std::filesystem::path>{"filepath"}),

    cmd_group(
        "move", ArgInfo<std::string>{"direction"},

        cmd("left",
            [](XenTimeline &tl) {
                tl.set_aux_state(action::move_left(tl));
                return msuccess("Moved Left.");
            }),

        cmd("right",
            [](XenTimeline &tl) {
                tl.set_aux_state(action::move_right(tl));
                return msuccess("Moved Right.");
            }),

        cmd("up",
            [](XenTimeline &tl) {
                tl.set_aux_state(action::move_up(tl));
                // TODO message depending on if moved or hit ceiling
                return msuccess("Moved Up.");
            }),

        cmd("down",
            [](XenTimeline &tl) {
                tl.set_aux_state(action::move_down(tl));
                // TODO message depending on if moved or hit floor
                return msuccess("Moved Down.");
            })),

    cmd(
        "addMeasure",
        [](XenTimeline &tl, sequence::TimeSignature const &ts) {
            auto [aux, state] = action::add_measure(tl, ts);
            tl.set_aux_state(std::move(aux), false);
            tl.add_state(std::move(state));
            return msuccess("Added measure.");
        },
        ArgInfo<sequence::TimeSignature>("duration", {{4, 4}})),

    cmd(
        "note",
        [](XenTimeline &tl, int interval, float velocity, float delay, float gate) {
            increment_state(tl, [](sequence::Cell const& , auto... args) -> sequence::Cell {
                return sequence::modify::note(args...);
            }, interval, velocity, delay, gate);
            return msuccess("Note Added");
        }, ArgInfo<int>{"interval", 0}, ArgInfo<float>{"velocity", 0.8f},
        ArgInfo<float>{"delay", 0.f}, ArgInfo<float>{"gate", 1.f}
    ),

    cmd(
        "rest",
        [](XenTimeline &tl) {
            increment_state(tl, [](sequence::Cell const&) -> sequence::Cell {
                return sequence::modify::rest();
            });
            return msuccess("Rest Added");
        }
    ),

    pattern(cmd(
        "flip",
        [](XenTimeline &tl, sequence::Pattern const& pattern) {
            increment_state(tl, &sequence::modify::flip, pattern, sequence::Note{});
            return msuccess("Flipped");
        }
    )),

    cmd(
        "delete",
        [](XenTimeline &tl) {
            auto [aux, state] = action::delete_cell(tl.get_aux_state(), tl.get_state().first);
            tl.set_aux_state(aux, false);
            tl.add_state(std::move(state));
            return msuccess("Deleted.");
        }
    ),

    cmd(
        "split",
        [](XenTimeline &tl, std::size_t count) {
            increment_state(tl, &sequence::modify::repeat, count);
            return msuccess("Split");
        }, ArgInfo<std::size_t>{"count", 2}
    ),

    cmd(
        "lift",
        [](XenTimeline &tl) {
            auto [state, aux] = action::lift(tl);
            tl.set_aux_state(aux, false);
            tl.add_state(state);
            return msuccess("Lifted.");
        }
    ),

    pattern(cmd(
        "stretch",
        [](XenTimeline &tl, sequence::Pattern const& pattern, std::size_t amount) {
            increment_state(tl, &sequence::modify::stretch, pattern, amount);
            return msuccess("Stretched.");
        }, ArgInfo<std::size_t>{"amount", 2}
    )),

    pattern(cmd(
        "compress",
        [](XenTimeline &tl, sequence::Pattern const& pattern) {
            if (pattern == sequence::Pattern{0, {1}})
            {
                return mwarning("Use pattern prefix to define compression.");
            }
            else {
                increment_state(tl, &sequence::modify::compress, pattern);
                return msuccess("Compressed.");
            }
        }
    )),

    pattern(cmd_group(
        "fill", ArgInfo<std::string>{"type"},

        cmd(
            "note",
            [](XenTimeline &tl, sequence::Pattern const& pattern, int interval,
               float velocity, float delay, float gate) {
                increment_state(tl, &sequence::modify::notes_fill, pattern, sequence::Note{interval,
                                velocity, delay, gate});
                return msuccess("Filled With Notes");
            }, ArgInfo<int>{"interval", 0}, ArgInfo<float>{"velocity", 0.8f},
            ArgInfo<float>{"delay", 0.f}, ArgInfo<float>{"gate", 1.f}
        ),

        cmd(
            "rest",
            [](XenTimeline &tl, sequence::Pattern const& pattern) {
                increment_state(tl, &sequence::modify::rests_fill, pattern);
                return msuccess("Filled With Rests");
            }
        )
    )),

    pattern(cmd_group(
        "set", ArgInfo<std::string>{"trait"},

        cmd(
            "note",
            [](XenTimeline &tl, sequence::Pattern const& pattern, int interval) {
                increment_state(tl, &sequence::modify::set_interval, pattern, interval);
                return msuccess("Note Set");
            }, ArgInfo<int>{"interval", 0}
        ),

        cmd(
            "octave",
            [](XenTimeline &tl, sequence::Pattern const& pattern, int octave) {
                tl.add_state(action::set_note_octave(tl, pattern, octave));
                return msuccess("Octave Set");
            }, ArgInfo<int>{"octave", 0}
        ),

        cmd(
            "velocity",
            [](XenTimeline &tl, sequence::Pattern const& pattern, float velocity) {
                increment_state(tl, &sequence::modify::set_velocity, pattern, velocity);
                return msuccess("Velocity Set");
            }, ArgInfo<float>{"velocity", 0.8f}
        ),

        cmd(
            "delay",
            [](XenTimeline &tl, sequence::Pattern const& pattern, float delay) {
                increment_state(tl, &sequence::modify::set_delay, pattern, delay);
                return msuccess("Delay Set");
            }, ArgInfo<float>{"delay", 0.f}
        ),

        cmd(
            "gate",
            [](XenTimeline &tl, sequence::Pattern const& pattern, float gate) {
                increment_state(tl, &sequence::modify::set_gate, pattern, gate);
                return msuccess("Gate Set");
            }, ArgInfo<float>{"gate", 1.f}
        )
    )),

    pattern(cmd_group(
        "shift", ArgInfo<std::string>{"trait"},

        cmd(
            "note",
            [](XenTimeline &tl, sequence::Pattern const& pattern, int amount) {
                increment_state(tl, &sequence::modify::shift_interval, pattern, amount);
                return msuccess("Note Shifted");
            },
            ArgInfo<int>{"amount", 1}
        ),

        cmd(
            "octave",
            [](XenTimeline &tl, sequence::Pattern const& pattern, int amount) {
                tl.add_state(action::shift_note_octave(tl, pattern, amount));
                return msuccess("Octave Shifted");
            }, ArgInfo<int>{"amount", 1}
        ),

        cmd(
            "velocity",
            [](XenTimeline &tl, sequence::Pattern const& pattern, float amount) {
                increment_state(tl, &sequence::modify::shift_velocity, pattern, amount);
                return msuccess("Velocity Shifted");
            },
            ArgInfo<float>{"amount", 0.1f}
        ),

        cmd(
            "delay",
            [](XenTimeline &tl, sequence::Pattern const& pattern, float amount) {
                increment_state(tl, &sequence::modify::shift_delay, pattern, amount);
                return msuccess("Delay Shifted");
            },
            ArgInfo<float>{"amount", 0.1f}
        ),

        cmd(
            "gate",
            [](XenTimeline &tl, sequence::Pattern const& pattern, float amount) {
                increment_state(tl, &sequence::modify::shift_gate, pattern, amount);
                return msuccess("Gate Shifted");
            },
            ArgInfo<float>{"amount", 0.1f}
        )
    )),

    pattern(cmd_group(
        "humanize", ArgInfo<InputMode>{"mode"},

        cmd(
            InputMode::Velocity,
            [](XenTimeline &tl, sequence::Pattern const &pattern, float amount) {
                increment_state(tl, &sequence::modify::humanize_velocity, pattern,
                                amount);
                return msuccess("Humanized Velocity.");
            },
            ArgInfo<float>{"amount", 0.1f}
        ),

        cmd(
            InputMode::Delay,
            [](XenTimeline &tl, sequence::Pattern const &pattern, float amount) {
                increment_state(tl, &sequence::modify::humanize_delay, pattern, amount);
                return msuccess("Humanized Delay.");
            },
            ArgInfo<float>{"amount", 0.1f}
        ),

        cmd(
            InputMode::Gate,
            [](XenTimeline &tl, sequence::Pattern const &pattern, float amount) {
                increment_state(tl, &sequence::modify::humanize_gate, pattern, amount);
                return msuccess("Humanized Gate.");
            },
            ArgInfo<float>{"amount", 0.1f})
        )
    ),

    pattern(cmd_group(
        "randomize", ArgInfo<InputMode>{"mode"},

        cmd(
            InputMode::Note,
            [](XenTimeline &tl, sequence::Pattern const &pattern, int min, int max) {
                increment_state(tl, &sequence::modify::randomize_intervals, pattern,
                                min, max);
                return msuccess("Randomized Note.");
            },
            ArgInfo<int>{"min", -12}, ArgInfo<int>{"max", 12}),

        cmd(
            InputMode::Velocity,
            [](XenTimeline &tl, sequence::Pattern const &pattern, float min,
               float max) {
                increment_state(tl, &sequence::modify::randomize_velocity, pattern, min,
                                max);
                return msuccess("Randomized Velocity.");
            },
            ArgInfo<float>{"min", 0.01f}, ArgInfo<float>{"max", 1.f}),

        cmd(
            InputMode::Delay,
            [](XenTimeline &tl, sequence::Pattern const &pattern, float min,
               float max) {
                increment_state(tl, &sequence::modify::randomize_delay, pattern, min,
                                max);
                return msuccess("Randomized Delay.");
            },
            ArgInfo<float>{"min", 0.f}, ArgInfo<float>{"max", 0.95f}),

        cmd(
            InputMode::Gate,
            [](XenTimeline &tl, sequence::Pattern const &pattern, float min,
               float max) {
                increment_state(tl, &sequence::modify::randomize_gate, pattern, min,
                                max);
                return msuccess("Randomized Gate.");
            },
            ArgInfo<float>{"min", 0.f}, ArgInfo<float>{"max", 0.95f}))),

    cmd(
        "shuffle",
        [](XenTimeline &tl) {
            increment_state(tl, &sequence::modify::shuffle);
            return msuccess("Shuffled.");
        }
    ),

    cmd(
        "rotate",
        [](XenTimeline &tl, int amount) {
            increment_state(tl, &sequence::modify::rotate, amount);
            return msuccess("Rotated.");
        }, ArgInfo<int>{"amount", 1}
    ),

    cmd(
        "reverse",
        [](XenTimeline &tl) {
            increment_state(tl, &sequence::modify::reverse);
            return msuccess("Reversed.");
        }
    ),

    pattern(cmd(
        "mirror",
        [](XenTimeline &tl, sequence::Pattern const& pattern, int center_note) {
            increment_state(tl, &sequence::modify::mirror, pattern, center_note);
            return msuccess("Mirrored.");
        }, ArgInfo<int>{"centerNote", 0}
    )),

    pattern(cmd(
        "quantize",
        [](XenTimeline &tl, sequence::Pattern const &pattern) {
            increment_state(tl, &sequence::modify::quantize, pattern);
            return msuccess("Quantized.");
        }
    )),

    cmd(
        "swing",
        [](XenTimeline &tl, float amount) {
            increment_state(tl, &sequence::modify::swing, amount, false);
            return msuccess("Swung.");
        }, ArgInfo<float>{"amount", 0.1f}
    ),

    // Temporary --------------------------------------------------------------

    cmd("demo",
        [](XenTimeline &tl) {
            tl.set_aux_state({{0, {}}}, false); // Manually reset selection on overwrite
            tl.add_state(demo_state());
            return msuccess("Demo state loaded.");
        })
);

} // namespace xen
