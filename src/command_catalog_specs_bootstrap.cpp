#include "command_catalog_specs_internal.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

#include <sequence/modify.hpp>
#include <sequence/sequence.hpp>

#include <xen/actions.hpp>
#include <xen/chord.hpp>
#include <xen/command_dsl.hpp>
#include <xen/constants.hpp>
#include <xen/document_storage.hpp>
#include <xen/message_level.hpp>
#include <xen/selection.hpp>
#include <xen/user_directory.hpp>

namespace xen::catalog_detail
{
namespace
{

auto as_juce_file(std::filesystem::path const &path) -> juce::File
{
    return juce::File{path.string()};
}

constexpr auto informational_policy = CommandPolicy{
    ProjectOperation::None, LibraryAccess::None,    WorkspaceAccess::None,
    FileAccess::None,       CopyBufferAccess::None, TargetRequirement::None,
    RepeatPolicy::Never,    HistoryPolicy::None};
constexpr auto new_project_policy = CommandPolicy{ProjectOperation::ReplaceHistory,
                                                  LibraryAccess::None,
                                                  WorkspaceAccess::None,
                                                  FileAccess::None,
                                                  CopyBufferAccess::None,
                                                  TargetRequirement::None,
                                                  RepeatPolicy::Never,
                                                  HistoryPolicy::None};
constexpr auto history_navigation_policy =
    CommandPolicy{ProjectOperation::NavigateHistory,
                  LibraryAccess::None,
                  WorkspaceAccess::None,
                  FileAccess::None,
                  CopyBufferAccess::None,
                  TargetRequirement::None,
                  RepeatPolicy::Never,
                  HistoryPolicy::None};
constexpr auto copy_policy = CommandPolicy{
    ProjectOperation::Read, LibraryAccess::None,     WorkspaceAccess::None,
    FileAccess::None,       CopyBufferAccess::Write, TargetRequirement::CellOrElement,
    RepeatPolicy::Never,    HistoryPolicy::None};
constexpr auto cut_policy = CommandPolicy{ProjectOperation::Edit,
                                          LibraryAccess::None,
                                          WorkspaceAccess::None,
                                          FileAccess::None,
                                          CopyBufferAccess::Write,
                                          TargetRequirement::CellOrElement,
                                          RepeatPolicy::OnSuccessfulProjectChange,
                                          HistoryPolicy::Commit};
constexpr auto paste_policy = CommandPolicy{ProjectOperation::Edit,
                                            LibraryAccess::None,
                                            WorkspaceAccess::None,
                                            FileAccess::None,
                                            CopyBufferAccess::Read,
                                            TargetRequirement::CellOrElement,
                                            RepeatPolicy::OnSuccessfulProjectChange,
                                            HistoryPolicy::Commit};
constexpr auto targeted_edit_policy =
    CommandPolicy{ProjectOperation::Edit,
                  LibraryAccess::None,
                  WorkspaceAccess::None,
                  FileAccess::None,
                  CopyBufferAccess::None,
                  TargetRequirement::CellOrElement,
                  RepeatPolicy::OnSuccessfulProjectChange,
                  HistoryPolicy::Commit};
constexpr auto load_project_resource_policy = CommandPolicy{
    ProjectOperation::Edit, LibraryAccess::None,    WorkspaceAccess::Read,
    FileAccess::Read,       CopyBufferAccess::None, TargetRequirement::None,
    RepeatPolicy::Never,    HistoryPolicy::Commit};
constexpr auto open_project_policy = CommandPolicy{ProjectOperation::ReplaceHistory,
                                                   LibraryAccess::None,
                                                   WorkspaceAccess::Read,
                                                   FileAccess::Read,
                                                   CopyBufferAccess::None,
                                                   TargetRequirement::None,
                                                   RepeatPolicy::Never,
                                                   HistoryPolicy::None};
constexpr auto reload_library_policy = CommandPolicy{
    ProjectOperation::None, LibraryAccess::Mutate,  WorkspaceAccess::None,
    FileAccess::Read,       CopyBufferAccess::None, TargetRequirement::None,
    RepeatPolicy::Never,    HistoryPolicy::None};
constexpr auto save_document_policy = CommandPolicy{
    ProjectOperation::Read, LibraryAccess::None,    WorkspaceAccess::Read,
    FileAccess::Write,      CopyBufferAccess::None, TargetRequirement::None,
    RepeatPolicy::Never,    HistoryPolicy::None};
constexpr auto save_cell_policy = CommandPolicy{
    ProjectOperation::Read, LibraryAccess::None,    WorkspaceAccess::Read,
    FileAccess::Write,      CopyBufferAccess::None, TargetRequirement::Cell,
    RepeatPolicy::Never,    HistoryPolicy::None};
constexpr auto workspace_mutation_policy = CommandPolicy{
    ProjectOperation::None, LibraryAccess::None,    WorkspaceAccess::Mutate,
    FileAccess::None,       CopyBufferAccess::None, TargetRequirement::None,
    RepeatPolicy::Never,    HistoryPolicy::None};

} // namespace

void append_bootstrap_specs(std::vector<CommandSpec> &specs)
{
    specs.push_back(command({"version"}, false,
                            "Print the current XenSequencer version.",
                            informational_policy, std::make_tuple(),
                            [](CommandHandlerContext &, CommandInvocation const &) {
                                return make_result(minfo(std::string{"v"} + VERSION));
                            }));

    specs.push_back(replay_command({"again"},
                                   "Replay the previously executed command chain.",
                                   informational_policy, {"repeat", "replay"}));

    specs.push_back(command(
        {"project", "new"}, false, "Create a new Project document.", new_project_policy,
        std::make_tuple(), [](CommandHandlerContext &, CommandInvocation const &) {
            throw std::logic_error{
                "Project new must execute through SequencerSession."};
            return make_result(merror("Project new unavailable"));
        }));

    specs.push_back(history_navigation_command(
        {"undo"}, "Revert state to before the last action.", history_navigation_policy,
        HistoryNavigationDirection::Undo, {"revert"}));

    specs.push_back(history_navigation_command(
        {"redo"}, "Reapply the last undone action.", history_navigation_policy,
        HistoryNavigationDirection::Redo, {"reapply"}));

    specs.push_back(command(
        {"copy"}, false, "Copy the current selection.", {"clipboard"}, copy_policy,
        std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto const &state = context.project();
            context.write_copy_buffer(action::copy(
                state, context.execution.cursor, require_selection(context.execution)));
            return unchanged_selection_result(minfo("Copied Selection"),
                                              context.execution);
        }));

    specs.push_back(command(
        {"cut"}, false, "Cut the current selection.", {"remove", "clipboard"},
        cut_policy, std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto state = context.project();
            context.write_copy_buffer(action::copy(
                state, context.execution.cursor, require_selection(context.execution)));
            auto const mutation = action::delete_cell(
                state, context.execution.cursor, require_selection(context.execution));
            context.edit_project() = std::move(state);
            return make_result(minfo("Selection Cut"), mutation.selection);
        }));

    specs.push_back(command(
        {"paste"}, false, "Paste over the current selection.", {"insert", "clipboard"},
        paste_policy, std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto state = context.project();
            auto const &content = context.copy_buffer();
            if (!content.has_value())
            {
                throw std::runtime_error{"Copy Buffer Is Empty"};
            }
            auto const mutation =
                action::paste(state, context.execution.cursor,
                              require_selection(context.execution), *content);
            context.edit_project() = std::move(state);
            return make_result(minfo("Selection Pasted Over"), mutation.selection);
        }));

    specs.push_back(command(
        {"duplicate"}, false, "Duplicate the current selection.", {"clone"},
        targeted_edit_policy, std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto state = context.project();
            auto const mutation = action::duplicate(
                state, context.execution.cursor, require_selection(context.execution));
            context.edit_project() = std::move(state);
            return make_result(minfo("Selection Duplicated"), mutation.selection);
        }));

    specs.push_back(command(
        {"load", "cell"}, false, "Load a Cell into a new selected Sequence.",
        {"open", "file", "sequence"}, load_project_resource_policy,
        std::make_tuple(required_arg<std::string>("relative_path", "path")),
        [](CommandHandlerContext &, CommandInvocation const &, std::string const &) {
            throw std::logic_error{"Cell load must execute through SequencerSession."};
            return make_result(merror("Cell load unavailable"));
        }));

    specs.push_back(command(
        {"project", "open"}, false, "Open a Project document.", open_project_policy,
        std::make_tuple(required_arg<std::string>("relative_path", "path")),
        [](CommandHandlerContext &, CommandInvocation const &, std::string const &) {
            throw std::logic_error{
                "Project open must execute through SequencerSession."};
            return make_result(merror("Project open unavailable"));
        }));

    specs.push_back(command(
        {"load", "tuning"}, false, "Load a tuning from the current tuning directory.",
        {"open", "file", "scala", "scl"}, load_project_resource_policy,
        std::make_tuple(required_arg<std::string>("relative_path", "path")),
        [](CommandHandlerContext &context, CommandInvocation const &,
           std::string const &filename) {
            auto const cd = context.workspace().tuning_directory;
            auto const directory = as_juce_file(cd);
            if (!directory.isDirectory())
            {
                return make_result(merror("Invalid Current Tuning Library Directory"));
            }
            auto const filepath = resolve_content_path(cd, filename, ".scl");
            auto const file = as_juce_file(filepath);
            if (!file.exists())
            {
                return make_result(merror("File Not Found: " + filepath.string()));
            }
            if (file.getSize() < 0 ||
                static_cast<std::uint64_t>(file.getSize()) > MAX_TUNING_FILE_BYTES)
            {
                throw std::runtime_error{"Tuning file exceeds the permitted size."};
            }
            auto state = context.project();
            auto &pitch = selected_column(state, context.execution.cursor).pitch;
            pitch.tuning.name = file.getFileNameWithoutExtension().toStdString();
            pitch.tuning.definition = sequence::from_scala(filepath.string());
            context.edit_project() = std::move(state);
            return make_result(minfo("Tuning Loaded"));
        }));

    specs.push_back(command(
        {"load", "scales"}, false, "Load scales from library files.",
        {"reload", "library"}, reload_library_policy, std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto const system = context.read_text(
                get_system_scales_file().getFullPathName().toStdString());
            auto const user = context.read_text(
                get_user_scales_file().getFullPathName().toStdString());
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
        {"reload", "library"}, reload_library_policy, std::make_tuple(),
        [](CommandHandlerContext &context, CommandInvocation const &) {
            auto const system = context.read_text(
                get_system_chords_file().getFullPathName().toStdString());
            auto const user = context.read_text(
                get_user_chords_file().getFullPathName().toStdString());
            if (!system.has_value() || !user.has_value())
            {
                throw std::runtime_error{"Chord library file is missing."};
            }
            context.edit_library().chords = load_chords(*system, *user);
            return make_result(minfo("Chords Loaded: " +
                                     std::to_string(context.library().chords.size())));
        }));

    specs.push_back(command(
        {"save", "cell"}, false, "Save the selected Cell to file.",
        {"write", "file", "sequence"}, save_cell_policy,
        std::make_tuple(required_arg<std::string>("relative_path", "path")),
        [](CommandHandlerContext &, CommandInvocation const &, std::string const &) {
            throw std::logic_error{"Cell save must execute through SequencerSession."};
            return make_result(merror("Cell save unavailable"));
        }));

    specs.push_back(
        command({"project", "save"}, false, "Save the current Project document.",
                save_document_policy, std::make_tuple(),
                [](CommandHandlerContext &, CommandInvocation const &) {
                    throw std::logic_error{
                        "Project save must execute through SequencerSession."};
                    return make_result(merror("Project save unavailable"));
                }));

    specs.push_back(command(
        {"project", "save", "as"}, false,
        "Save the current Project document under a new name.", save_document_policy,
        std::make_tuple(required_arg<std::string>("relative_path", "path")),
        [](CommandHandlerContext &, CommandInvocation const &, std::string const &) {
            throw std::logic_error{
                "Project save as must execute through SequencerSession."};
            return make_result(merror("Project save as unavailable"));
        }));

    specs.push_back(
        command({"libraryDirectory"}, false, "Display the user library directory path.",
                informational_policy, std::make_tuple(),
                [](CommandHandlerContext &, CommandInvocation const &) {
                    return make_result(minfo(
                        get_user_library_directory().getFullPathName().toStdString()));
                }));

    auto const set_directory = [](auto member, std::string label) {
        return [member, label = std::move(label)](CommandHandlerContext &context,
                                                  CommandInvocation const &,
                                                  std::string const &path) {
            auto const directory = juce::File{path};
            if (!directory.isDirectory())
            {
                return make_result(
                    merror(label + " directory does not exist: " + path));
            }
            (context.edit_workspace()).*member =
                std::filesystem::path{directory.getFullPathName().toStdString()};
            return make_result(minfo(label + " Directory Set"));
        };
    };
    specs.push_back(
        command({"set", "contentDirectory"}, false, "Set the content directory.",
                workspace_mutation_policy,
                std::make_tuple(required_arg<std::string>("directory_path", "path")),
                set_directory(&WorkspaceSettings::content_directory, "Content")));
    specs.push_back(
        command({"set", "tuningDirectory"}, false, "Set the tuning library directory.",
                workspace_mutation_policy,
                std::make_tuple(required_arg<std::string>("directory_path", "path")),
                set_directory(&WorkspaceSettings::tuning_directory, "Tuning")));
}

} // namespace xen::catalog_detail
