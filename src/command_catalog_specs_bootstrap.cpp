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
namespace
{

constexpr auto informational_policy = CommandPolicy{
    ProjectOperation::None, LibraryAccess::None,     WorkspaceAccess::None,
    FileAccess::None,       TargetRequirement::None, RepeatPolicy::Never,
    HistoryPolicy::None};
constexpr auto reset_policy = CommandPolicy{
    ProjectOperation::Edit, LibraryAccess::Mutate,   WorkspaceAccess::None,
    FileAccess::None,       TargetRequirement::None, RepeatPolicy::Never,
    HistoryPolicy::Commit};
constexpr auto history_navigation_policy =
    CommandPolicy{ProjectOperation::NavigateHistory,
                  LibraryAccess::None,
                  WorkspaceAccess::None,
                  FileAccess::None,
                  TargetRequirement::None,
                  RepeatPolicy::Never,
                  HistoryPolicy::None};
constexpr auto copy_policy = CommandPolicy{ProjectOperation::Read,
                                           LibraryAccess::None,
                                           WorkspaceAccess::None,
                                           FileAccess::Write,
                                           TargetRequirement::CellOrElement,
                                           RepeatPolicy::Never,
                                           HistoryPolicy::None};
constexpr auto cut_policy = CommandPolicy{ProjectOperation::Edit,
                                          LibraryAccess::None,
                                          WorkspaceAccess::None,
                                          FileAccess::Write,
                                          TargetRequirement::CellOrElement,
                                          RepeatPolicy::OnSuccessfulProjectChange,
                                          HistoryPolicy::Commit};
constexpr auto paste_policy = CommandPolicy{ProjectOperation::Edit,
                                            LibraryAccess::None,
                                            WorkspaceAccess::None,
                                            FileAccess::Read,
                                            TargetRequirement::CellOrElement,
                                            RepeatPolicy::OnSuccessfulProjectChange,
                                            HistoryPolicy::Commit};
constexpr auto targeted_edit_policy =
    CommandPolicy{ProjectOperation::Edit,
                  LibraryAccess::None,
                  WorkspaceAccess::None,
                  FileAccess::None,
                  TargetRequirement::CellOrElement,
                  RepeatPolicy::OnSuccessfulProjectChange,
                  HistoryPolicy::Commit};
constexpr auto load_project_resource_policy = CommandPolicy{
    ProjectOperation::Edit, LibraryAccess::None,     WorkspaceAccess::Read,
    FileAccess::Read,       TargetRequirement::None, RepeatPolicy::Never,
    HistoryPolicy::Commit};
constexpr auto reload_library_policy = CommandPolicy{
    ProjectOperation::None, LibraryAccess::Mutate,   WorkspaceAccess::None,
    FileAccess::Read,       TargetRequirement::None, RepeatPolicy::Never,
    HistoryPolicy::None};
constexpr auto save_measure_policy = CommandPolicy{
    ProjectOperation::Read, LibraryAccess::None,     WorkspaceAccess::Read,
    FileAccess::Write,      TargetRequirement::None, RepeatPolicy::Never,
    HistoryPolicy::None};

} // namespace

void append_bootstrap_specs(std::vector<CommandSpec> &specs)
{
    specs.push_back(command(
        {"welcome"}, false, "Display welcome message.", informational_policy,
        std::make_tuple(), [](CommandHandlerContext &, CommandInvocation const &) {
            return make_result(
                minfo(std::string{"Welcome to XenSequencer v"} + VERSION));
        }));

    specs.push_back(command({"version"}, false,
                            "Print the current XenSequencer version.",
                            informational_policy, std::make_tuple(),
                            [](CommandHandlerContext &, CommandInvocation const &) {
                                return make_result(minfo(std::string{"v"} + VERSION));
                            }));

    specs.push_back(replay_command({"again"},
                                   "Replay the previously executed command chain.",
                                   informational_policy));

    specs.push_back(
        command({"reset"}, false, "Reset XenSequencer to its initial state.",
                reset_policy, std::make_tuple(),
                [](CommandHandlerContext &context, CommandInvocation const &) {
                    context.edit_project() = EngineState{};
                    context.edit_library().scale_shift_index = std::nullopt;
                    return make_result(minfo("XenSequencer Reset"));
                }));

    specs.push_back(history_navigation_command(
        {"undo"}, "Revert state to before the last action.", history_navigation_policy,
        HistoryNavigationDirection::Undo));

    specs.push_back(history_navigation_command(
        {"redo"}, "Reapply the last undone action.", history_navigation_policy,
        HistoryNavigationDirection::Redo));

    specs.push_back(command(
        {"copy"}, false, "Copy the current selection.", copy_policy, std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto const &state = context.project();
            context.write_text(copy_buffer_filepath(),
                               serialize_copy_buffer_content(action::copy(
                                   state, require_selection(context.execution))));
            return unchanged_selection_result(minfo("Copied Selection"),
                                              context.execution);
        }));

    specs.push_back(command(
        {"cut"}, false, "Cut the current selection.", cut_policy, std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto state = context.project();
            context.write_text(copy_buffer_filepath(),
                               serialize_copy_buffer_content(action::copy(
                                   state, require_selection(context.execution))));
            auto const mutation =
                action::delete_cell(state, require_selection(context.execution));
            context.edit_project() = std::move(state);
            return make_result(minfo("Selection Cut"), mutation.selection);
        }));

    specs.push_back(command(
        {"paste"}, false, "Paste over the current selection.", paste_policy,
        std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto state = context.project();
            auto const text = context.read_text(copy_buffer_filepath());
            if (!text.has_value() || text->empty())
            {
                throw std::runtime_error{"Copy Buffer Is Empty"};
            }
            auto const content = deserialize_copy_buffer_content(*text);
            auto const mutation =
                action::paste(state, require_selection(context.execution), content);
            context.edit_project() = std::move(state);
            return make_result(minfo("Selection Pasted Over"), mutation.selection);
        }));

    specs.push_back(command(
        {"duplicate"}, false, "Duplicate the current selection.", targeted_edit_policy,
        std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto state = context.project();
            auto const mutation =
                action::duplicate(state, require_selection(context.execution));
            context.edit_project() = std::move(state);
            return make_result(minfo("Selection Duplicated"), mutation.selection);
        }));

    specs.push_back(command(
        {"load", "measure"}, false,
        "Load a measure from the current sequence directory.",
        load_project_resource_policy,
        std::make_tuple(required_arg<std::string>("String", "filename")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           std::string const &filename) {
            auto const cd = context.workspace().current_sequence_directory;
            if (!cd.isDirectory())
            {
                return make_result(merror("Invalid Current Sequence Directory"));
            }
            auto const filepath = cd.getChildFile(filename + ".xss");
            auto const text = context.read_text(filepath);
            if (!text.has_value())
            {
                return make_result(merror("File Not Found: " +
                                          filepath.getFullPathName().toStdString()));
            }
            auto state = context.project();
            if (text->size() > (128 * 1'024 * 1'024))
            {
                throw std::runtime_error{"Measure file size exceeds 128MB"};
            }
            state.measure = deserialize_measure(*text);
            context.edit_project() = std::move(state);
            return make_result(minfo("Measure Loaded"));
        }));

    specs.push_back(command(
        {"load", "tuning"}, false, "Load a tuning from the current tuning directory.",
        load_project_resource_policy,
        std::make_tuple(required_arg<std::string>("String", "filename")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           std::string const &filename) {
            auto const cd = context.workspace().current_tuning_directory;
            if (!cd.isDirectory())
            {
                return make_result(merror("Invalid Current Tuning Library Directory"));
            }
            auto const filepath = cd.getChildFile(filename + ".scl");
            if (!filepath.exists())
            {
                return make_result(merror("File Not Found: " +
                                          filepath.getFullPathName().toStdString()));
            }
            auto state = context.project();
            state.tuning_name = filepath.getFileNameWithoutExtension().toStdString();
            state.tuning =
                sequence::from_scala(filepath.getFullPathName().toStdString());
            context.edit_project() = std::move(state);
            return make_result(minfo("Tuning Loaded"));
        }));

    specs.push_back(command(
        {"load", "scales"}, false, "Load scales from library files.",
        reload_library_policy, std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto const system = context.read_text(get_system_scales_file());
            auto const user = context.read_text(get_user_scales_file());
            if (!system.has_value() || !user.has_value())
            {
                throw std::runtime_error{"Scale library file is missing."};
            }
            context.edit_library().scales = load_scales(*system, *user);
            return make_result(minfo("Scales Loaded: " +
                                     std::to_string(context.library().scales.size())));
        }));

    specs.push_back(command(
        {"load", "chords"}, false, "Load chords from library files.",
        reload_library_policy, std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto const system = context.read_text(get_system_chords_file());
            auto const user = context.read_text(get_user_chords_file());
            if (!system.has_value() || !user.has_value())
            {
                throw std::runtime_error{"Chord library file is missing."};
            }
            context.edit_library().chords = load_chords(*system, *user);
            return make_result(minfo("Chords Loaded: " +
                                     std::to_string(context.library().chords.size())));
        }));

    specs.push_back(command(
        {"save", "measure"}, false, "Save the current measure to file.",
        save_measure_policy,
        std::make_tuple(required_arg<std::string>("String", "filename")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           std::string const &filename) {
            auto const cd = context.workspace().current_sequence_directory;
            if (!cd.isDirectory())
            {
                return make_result(merror("Invalid Current Sequence Directory"));
            }
            auto const filepath = cd.getChildFile(filename + ".xss");
            context.write_text(filepath, serialize_measure(context.project().measure));
            return make_result(
                minfo("Measure Saved to " +
                      single_quote(filepath.getFullPathName().toStdString())));
        }));

    specs.push_back(
        command({"libraryDirectory"}, false, "Display the user library directory path.",
                informational_policy, std::make_tuple(),
                [](CommandHandlerContext &, CommandInvocation const &) {
                    return make_result(minfo(
                        get_user_library_directory().getFullPathName().toStdString()));
                }));
}

} // namespace xen::catalog_detail
