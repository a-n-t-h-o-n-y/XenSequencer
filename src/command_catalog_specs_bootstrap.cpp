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
#include <xen/serialize.hpp>
#include <xen/string_manip.hpp>
#include <xen/submission_effects.hpp>

namespace xen::catalog_detail
{

void append_bootstrap_specs(std::vector<CommandSpec> &specs)
{
    specs.push_back(non_repeatable_command(
        {"welcome"}, false, "Display welcome message.", std::make_tuple(),
        [](PluginState &, CommandInvocation const &) {
            return minfo(std::string{"Welcome to XenSequencer v"} + VERSION);
        }));

    specs.push_back(non_repeatable_command(
        {"version"}, false, "Print the current XenSequencer version.",
        std::make_tuple(), [](PluginState &, CommandInvocation const &) {
            return minfo(std::string{"v"} + VERSION);
        }));

    specs.push_back(
        replay_command({"again"}, "Replay the previously executed command chain."));

    specs.push_back(non_repeatable_command(
        {"reset"}, false, "Reset XenSequencer to its initial state.", std::make_tuple(),
        [](PluginState &ps, CommandInvocation const &) {
            ps.timeline.stage(EngineState{});
            ps.editor = EditorSessionState{};
            ps.library.scale_shift_index = std::nullopt;
            return minfo("XenSequencer Reset");
        }));

    specs.push_back(history_command(
        {"undo"}, "Revert state to before the last action.", ExecutionRole::Undo,
        std::make_tuple(), [](PluginState &ps, CommandInvocation const &) {
            return ps.timeline.undo() ? minfo("Undone") : minfo("Nothing to undo.");
        }));

    specs.push_back(history_command(
        {"redo"}, "Reapply the last undone action.", ExecutionRole::Redo,
        std::make_tuple(), [](PluginState &ps, CommandInvocation const &) {
            return ps.timeline.redo() ? minfo("Redone") : minfo("Nothing to redo.");
        }));

    specs.push_back(non_repeatable_command(
        {"copy"}, false, "Copy the current selection.", std::make_tuple(),
        [](PluginState &ps, SubmissionEffects &effects, CommandInvocation const &) {
            auto const state = ps.timeline.get_state();
            effects.write_text(
                copy_buffer_filepath(),
                serialize_copy_buffer_content(action::copy(state, ps.editor)));
            return minfo("Copied Selection");
        }));

    specs.push_back(command(
        {"cut"}, false, "Cut the current selection.", std::make_tuple(),
        [](PluginState &ps, SubmissionEffects &effects, CommandInvocation const &) {
            auto state = ps.timeline.get_state();
            effects.write_text(
                copy_buffer_filepath(),
                serialize_copy_buffer_content(action::copy(state, ps.editor)));
            action::delete_cell(state, ps.editor);
            ps.timeline.stage(std::move(state));
            return minfo("Selection Cut");
        }));

    specs.push_back(command(
        {"paste"}, false, "Paste over the current selection.", std::make_tuple(),
        [](PluginState &ps, SubmissionEffects &effects, CommandInvocation const &) {
            auto state = ps.timeline.get_state();
            auto const text = effects.read_text(copy_buffer_filepath());
            if (!text.has_value() || text->empty())
            {
                throw std::runtime_error{"Copy Buffer Is Empty"};
            }
            auto const content = deserialize_copy_buffer_content(*text);
            state = action::paste(std::move(state), ps.editor, content);
            if (selection_kind(ps.editor.selected) == SelectionKind::Element &&
                std::holds_alternative<sequence::Cell>(content))
            {
                ps.editor.selected = select_parent_cell(ps.editor.selected);
            }
            ps.timeline.stage(std::move(state));
            return minfo("Selection Pasted Over");
        }));

    specs.push_back(command({"duplicate"}, false, "Duplicate the current selection.",
                            std::make_tuple(),
                            [](PluginState &ps, CommandInvocation const &) {
                                auto state = ps.timeline.get_state();
                                action::duplicate(state, ps.editor);
                                ps.timeline.stage(std::move(state));
                                return minfo("Selection Duplicated");
                            }));

    specs.push_back(non_repeatable_command(
        {"inputMode"}, false, "Change the input mode used by editing commands.",
        std::make_tuple(required_arg<InputMode>("InputMode", "mode")),
        [](PluginState &ps, CommandInvocation const &, InputMode mode) {
            ps.editor = action::set_input_mode(std::move(ps.editor), mode);
            return minfo("Input Mode Set to " + single_quote(to_string(mode)));
        }));

    specs.push_back(non_repeatable_command(
        {"load", "measure"}, false,
        "Load a measure from the current sequence directory.",
        std::make_tuple(required_arg<std::string>("String", "filename")),
        [](PluginState &ps, SubmissionEffects &effects, CommandInvocation const &,
           std::string const &filename) {
            auto const cd = ps.config.current_sequence_directory;
            if (!cd.isDirectory())
            {
                return merror("Invalid Current Sequence Directory");
            }
            auto const filepath = cd.getChildFile(filename + ".xss");
            auto const text = effects.read_text(filepath);
            if (!text.has_value())
            {
                return merror("File Not Found: " +
                              filepath.getFullPathName().toStdString());
            }
            auto state = ps.timeline.get_state();
            if (text->size() > (128 * 1'024 * 1'024))
            {
                throw std::runtime_error{"Measure file size exceeds 128MB"};
            }
            state.measure = deserialize_measure(*text);
            ps.timeline.stage(std::move(state));
            return minfo("Measure Loaded");
        }));

    specs.push_back(non_repeatable_command(
        {"load", "tuning"}, false, "Load a tuning from the current tuning directory.",
        std::make_tuple(required_arg<std::string>("String", "filename")),
        [](PluginState &ps, CommandInvocation const &, std::string const &filename) {
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
            state.tuning_name = filepath.getFileNameWithoutExtension().toStdString();
            state.tuning =
                sequence::from_scala(filepath.getFullPathName().toStdString());
            ps.timeline.stage(std::move(state));
            return minfo("Tuning Loaded");
        }));

    specs.push_back(non_repeatable_command(
        {"load", "scales"}, false, "Load scales from library files.", std::make_tuple(),
        [](PluginState &ps, CommandInvocation const &) {
            ps.library.scales = load_scales_from_files();
            return minfo("Scales Loaded: " + std::to_string(ps.library.scales.size()));
        }));

    specs.push_back(non_repeatable_command(
        {"load", "chords"}, false, "Load chords from library files.", std::make_tuple(),
        [](PluginState &ps, CommandInvocation const &) {
            ps.library.chords = load_chords_from_files();
            return minfo("Chords Loaded: " + std::to_string(ps.library.chords.size()));
        }));

    specs.push_back(non_repeatable_command(
        {"save", "measure"}, false, "Save the current measure to file.",
        std::make_tuple(required_arg<std::string>("String", "filename")),
        [](PluginState &ps, SubmissionEffects &effects, CommandInvocation const &,
           std::string const &filename) {
            auto const cd = ps.config.current_sequence_directory;
            if (!cd.isDirectory())
            {
                return merror("Invalid Current Sequence Directory");
            }
            auto const filepath = cd.getChildFile(filename + ".xss");
            auto const state = ps.timeline.get_state();
            effects.write_text(filepath, serialize_measure(state.measure));
            return minfo("Measure Saved to " +
                         single_quote(filepath.getFullPathName().toStdString()));
        }));

    specs.push_back(non_repeatable_command(
        {"libraryDirectory"}, false, "Display the user library directory path.",
        std::make_tuple(), [](PluginState &, CommandInvocation const &) {
            return minfo(get_user_library_directory().getFullPathName().toStdString());
        }));
}

} // namespace xen::catalog_detail
