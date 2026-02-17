#include "command_catalog_spec_builder.hpp"
#include "command_catalog_specs_internal.hpp"

namespace xen::catalog_detail
{

void append_bootstrap_specs(std::vector<CommandSpec> &specs)
{
    specs.push_back(make_spec(
        {"welcome"}, false, "Display welcome message.", std::make_tuple(),
        [](CommandInvocation const &) { return WelcomeAction{}; }));

    specs.push_back(make_spec(
        {"version"}, false, "Print the current XenSequencer version.",
        std::make_tuple(),
        [](CommandInvocation const &) { return VersionAction{}; }));

    specs.push_back(make_spec(
        {"again"}, false, "Replay the previously executed command chain.",
        std::make_tuple(), [](CommandInvocation const &) { return AgainAction{}; }));

    specs.push_back(make_spec(
        {"commit"}, false, "Force a commit on the current command chain.",
        std::make_tuple(), [](CommandInvocation const &) { return CommitAction{}; }));

    specs.push_back(make_spec(
        {"reset"}, false, "Reset XenSequencer to its initial state.",
        std::make_tuple(), [](CommandInvocation const &) { return ResetAction{}; }));

    specs.push_back(make_spec(
        {"undo"}, false, "Revert state to before the last action.",
        std::make_tuple(), [](CommandInvocation const &) { return UndoAction{}; }));

    specs.push_back(make_spec(
        {"redo"}, false, "Reapply the last undone action.",
        std::make_tuple(), [](CommandInvocation const &) { return RedoAction{}; }));

    specs.push_back(make_spec(
        {"copy"}, false, "Copy the current selection.", std::make_tuple(),
        [](CommandInvocation const &) { return CopySelectionAction{}; }));

    specs.push_back(make_spec(
        {"cut"}, false, "Cut the current selection.", std::make_tuple(),
        [](CommandInvocation const &) { return CutSelectionAction{}; }));

    specs.push_back(make_spec(
        {"paste"}, false, "Paste over the current selection.", std::make_tuple(),
        [](CommandInvocation const &) { return PasteSelectionAction{}; }));

    specs.push_back(make_spec(
        {"duplicate"}, false, "Duplicate the current selection.",
        std::make_tuple(),
        [](CommandInvocation const &) { return DuplicateSelectionAction{}; }));

    specs.push_back(make_spec(
        {"inputMode"}, false,
        "Change the input mode used by editing commands.",
        std::make_tuple(required_arg<InputMode>("InputMode", "mode")),
        [](CommandInvocation const &, InputMode mode) {
            return SetInputModeAction{.mode = mode};
        }));

    specs.push_back(make_spec(
        {"focus"}, false,
        "Deprecated. UI focus is handled by the UI adapter layer.",
        std::make_tuple(required_arg<std::string>("String", "component_id")),
        [](CommandInvocation const &, std::string component_id) {
            return DeprecatedFocusAction{.component_id = std::move(component_id)};
        }));

    specs.push_back(make_spec(
        {"show"}, false,
        "Deprecated. UI routing is handled by the UI adapter layer.",
        std::make_tuple(required_arg<std::string>("String", "component_id")),
        [](CommandInvocation const &, std::string component_id) {
            return DeprecatedShowAction{.component_id = std::move(component_id)};
        }));

    specs.push_back(make_spec(
        {"load", "sequenceBank"}, false,
        "Load the sequence bank from the current sequence directory.",
        std::make_tuple(required_arg<std::string>("String", "filename")),
        [](CommandInvocation const &, std::string filename) {
            return LoadSequenceBankAction{.filename = std::move(filename)};
        }));

    specs.push_back(make_spec(
        {"load", "tuning"}, false,
        "Load a tuning from the current tuning directory.",
        std::make_tuple(required_arg<std::string>("String", "filename")),
        [](CommandInvocation const &, std::string filename) {
            return LoadTuningAction{.filename = std::move(filename)};
        }));

    specs.push_back(make_spec(
        {"load", "keys"}, false, "Deprecated command.", std::make_tuple(),
        [](CommandInvocation const &) { return LoadKeysAction{}; }));

    specs.push_back(make_spec(
        {"load", "scales"}, false, "Load scales from library files.",
        std::make_tuple(), [](CommandInvocation const &) { return LoadScalesAction{}; }));

    specs.push_back(make_spec(
        {"load", "chords"}, false, "Load chords from library files.",
        std::make_tuple(), [](CommandInvocation const &) { return LoadChordsAction{}; }));

    specs.push_back(make_spec(
        {"save", "sequenceBank"}, false,
        "Save the current sequence bank to file.",
        std::make_tuple(required_arg<std::string>("String", "filename")),
        [](CommandInvocation const &, std::string filename) {
            return SaveSequenceBankAction{.filename = std::move(filename)};
        }));

    specs.push_back(make_spec(
        {"libraryDirectory"}, false,
        "Display the user library directory path.", std::make_tuple(),
        [](CommandInvocation const &) { return LibraryDirectoryAction{}; }));
}

} // namespace xen::catalog_detail
