#include "command_catalog_specs_internal.hpp"

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <sequence/modify.hpp>
#include <sequence/sequence.hpp>

#include <xen/actions.hpp>
#include <xen/chord.hpp>
#include <xen/command_dsl.hpp>
#include <xen/constants.hpp>
#include <xen/copy_paste.hpp>
#include <xen/message_level.hpp>
#include <xen/selection.hpp>
#include <xen/string_manip.hpp>

namespace xen::catalog_detail
{

void append_bootstrap_specs(std::vector<CommandSpec> &specs)
{
    specs.push_back(
        command({"welcome"}, false, "Display welcome message.", std::make_tuple(),
                [](PluginState &, ExecutionContext, CommandInvocation const &) {
                    return minfo(std::string{"Welcome to XenSequencer v"} + VERSION);
                }));

    specs.push_back(
        command({"version"}, false, "Print the current XenSequencer version.",
                std::make_tuple(),
                [](PluginState &, ExecutionContext, CommandInvocation const &) {
                    return minfo(std::string{"v"} + VERSION);
                }));

    specs.push_back(
        replay_command({"again"}, "Replay the previously executed command chain."));

    specs.push_back(
        command({"commit"}, false, "Force a commit on the current command chain.",
                std::make_tuple(),
                [](PluginState &ps, ExecutionContext, CommandInvocation const &) {
                    ps.commit_intent = CommitIntent::Force;
                    return mdebug("commit made");
                }));

    specs.push_back(command(
        {"reset"}, false, "Reset XenSequencer to its initial state.", std::make_tuple(),
        [](PluginState &ps, ExecutionContext, CommandInvocation const &) {
            ps.timeline.stage({EngineState{}, EditorSessionState{}});
            ps.library.scale_shift_index = std::nullopt;
            return minfo("XenSequencer Reset");
        }));

    specs.push_back(command(
        {"undo"}, false, "Revert state to before the last action.", std::make_tuple(),
        [](PluginState &ps, ExecutionContext, CommandInvocation const &) {
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
        }));

    specs.push_back(command(
        {"redo"}, false, "Reapply the last undone action.", std::make_tuple(),
        [](PluginState &ps, ExecutionContext, CommandInvocation const &) {
            return ps.timeline.redo() ? minfo("Redone") : minfo("Nothing to redo.");
        }));

    specs.push_back(command(
        {"copy"}, false, "Copy the current selection.", std::make_tuple(),
        [](PluginState &ps, ExecutionContext context, CommandInvocation const &) {
            auto const state = ps.timeline.get_state();
            action::copy(state.sequencer, context);
            return minfo("Copied Selection");
        }));

    specs.push_back(command(
        {"cut"}, false, "Cut the current selection.", std::make_tuple(),
        [](PluginState &ps, ExecutionContext context, CommandInvocation const &) {
            auto state = ps.timeline.get_state();
            state.aux = context;
            action::copy(state.sequencer, state.aux);
            state = action::delete_cell(std::move(state));
            ps.timeline.stage(std::move(state));
            return minfo("Selection Cut");
        }));

    specs.push_back(command(
        {"paste"}, false, "Paste over the current selection.", std::make_tuple(),
        [](PluginState &ps, ExecutionContext context, CommandInvocation const &) {
            auto state = ps.timeline.get_state();
            state.aux = context;
            auto const content = read_copy_buffer();
            state.sequencer = action::paste(std::move(state.sequencer), state.aux);
            if (content.has_value() &&
                selection_kind(state.aux.selected) == SelectionKind::Element &&
                std::holds_alternative<sequence::Cell>(*content))
            {
                state.aux.selected = select_parent_cell(state.aux.selected);
            }
            ps.timeline.stage(std::move(state));
            return minfo("Selection Pasted Over");
        }));

    specs.push_back(command(
        {"duplicate"}, false, "Duplicate the current selection.", std::make_tuple(),
        [](PluginState &ps, ExecutionContext context, CommandInvocation const &) {
            auto state = ps.timeline.get_state();
            state.aux = context;
            state = action::duplicate(std::move(state));
            ps.timeline.stage(std::move(state));
            return minfo("Selection Duplicated");
        }));

    specs.push_back(
        command({"inputMode"}, false, "Change the input mode used by editing commands.",
                std::make_tuple(required_arg<InputMode>("InputMode", "mode")),
                [](PluginState &ps, ExecutionContext context, CommandInvocation const &,
                   InputMode mode) {
                    auto state = ps.timeline.get_state();
                    state.aux = action::set_input_mode(std::move(context), mode);
                    ps.timeline.stage(std::move(state));
                    return minfo("Input Mode Set to " + single_quote(to_string(mode)));
                }));

    specs.push_back(
        command({"load", "measure"}, false,
                "Load a measure from the current sequence directory.",
                std::make_tuple(required_arg<std::string>("String", "filename")),
                [](PluginState &ps, ExecutionContext context, CommandInvocation const &,
                   std::string const &filename) {
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
                    auto state = ps.timeline.get_state();
                    state.aux = context;
                    state.sequencer.measure = action::load_measure(filepath);
                    ps.timeline.stage(std::move(state));
                    return minfo("Measure Loaded");
                }));

    specs.push_back(command(
        {"load", "tuning"}, false, "Load a tuning from the current tuning directory.",
        std::make_tuple(required_arg<std::string>("String", "filename")),
        [](PluginState &ps, ExecutionContext context, CommandInvocation const &,
           std::string const &filename) {
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
            auto state = ps.timeline.get_state();
            state.aux = context;
            state.sequencer.tuning_name =
                filepath.getFileNameWithoutExtension().toStdString();
            state.sequencer.tuning =
                sequence::from_scala(filepath.getFullPathName().toStdString());
            ps.timeline.stage(std::move(state));
            return minfo("Tuning Loaded");
        }));

    specs.push_back(command(
        {"load", "scales"}, false, "Load scales from library files.", std::make_tuple(),
        [](PluginState &ps, ExecutionContext, CommandInvocation const &) {
            ps.library.scales = load_scales_from_files();
            return minfo("Scales Loaded: " + std::to_string(ps.library.scales.size()));
        }));

    specs.push_back(command(
        {"load", "chords"}, false, "Load chords from library files.", std::make_tuple(),
        [](PluginState &ps, ExecutionContext, CommandInvocation const &) {
            ps.library.chords = load_chords_from_files();
            return minfo("Chords Loaded: " + std::to_string(ps.library.chords.size()));
        }));

    specs.push_back(command(
        {"save", "measure"}, false, "Save the current measure to file.",
        std::make_tuple(required_arg<std::string>("String", "filename")),
        [](PluginState &ps, ExecutionContext, CommandInvocation const &,
           std::string const &filename) {
            auto const cd = ps.config.current_sequence_directory;
            if (!cd.isDirectory())
            {
                return merror("Invalid Current Sequence Directory");
            }
            auto const filepath = cd.getChildFile(filename + ".xss");
            auto const state = ps.timeline.get_state();
            action::save_measure(filepath, state.sequencer.measure);
            return minfo("Measure Saved to " +
                         single_quote(filepath.getFullPathName().toStdString()));
        }));

    specs.push_back(command(
        {"libraryDirectory"}, false, "Display the user library directory path.",
        std::make_tuple(),
        [](PluginState &, ExecutionContext, CommandInvocation const &) {
            return minfo(get_user_library_directory().getFullPathName().toStdString());
        }));
}

} // namespace xen::catalog_detail
