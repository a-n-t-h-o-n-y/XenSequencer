#include "command_catalog_specs_internal.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include <sequence/modify.hpp>

#include <xen/actions.hpp>
#include <xen/command_dsl.hpp>
#include <xen/message_level.hpp>

namespace xen::catalog_detail
{
namespace
{

constexpr auto targeted_edit_policy =
    CommandPolicy{ProjectOperation::Edit,
                  LibraryAccess::None,
                  WorkspaceAccess::None,
                  FileAccess::None,
                  CopyBufferAccess::None,
                  TargetRequirement::CellOrElement,
                  RepeatPolicy::OnSuccessfulProjectChange,
                  HistoryPolicy::Commit};
constexpr auto project_edit_policy =
    CommandPolicy{ProjectOperation::Edit,
                  LibraryAccess::None,
                  WorkspaceAccess::None,
                  FileAccess::None,
                  CopyBufferAccess::None,
                  TargetRequirement::None,
                  RepeatPolicy::OnSuccessfulProjectChange,
                  HistoryPolicy::Commit};

auto controller_arg()
{
    return ranged(required_arg<int>("midi_controller", "controller"), 0.0, 127.0,
                  "Must be in range [0, 127].");
}

auto normalized_value_arg()
{
    return ranged(required_arg<float>("normalized_value", "value"), 0.0, 1.0,
                  "Must be in range [0, 1].");
}

auto normalized_delta_arg()
{
    return ranged(required_arg<float>("normalized_delta", "amount"), -1.0, 1.0,
                  "Must be in range [-1, 1].");
}

auto label_arg()
{
    return constrained(
        required_arg<std::string>("string", "label"),
        CatalogArgumentConstraint{.kind = "byte_length",
                                  .minimum = 1.0,
                                  .maximum =
                                      static_cast<double>(MAX_PERSISTED_STRING_BYTES),
                                  .values = {}},
        [](std::string const &label) {
            return !label.empty() && label.size() <= MAX_PERSISTED_STRING_BYTES;
        },
        "Must contain between 1 and 4096 bytes.");
}

auto midi_controller(int controller) -> sequence::MidiControllerNumber
{
    return static_cast<sequence::MidiControllerNumber>(controller);
}

auto label_key(std::string label) -> std::string
{
    for (auto &character : label)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return label;
}

void set_label(ProjectState &state, sequence::MidiControllerNumber controller,
               std::string label)
{
    auto const key = label_key(label);
    auto const duplicate =
        std::ranges::find_if(state.midi_cc_labels, [&](auto const &entry) {
            return entry.first != controller && label_key(entry.second) == key;
        });
    if (duplicate != state.midi_cc_labels.end())
    {
        throw std::invalid_argument{"MIDI CC labels must be unique."};
    }
    state.midi_cc_labels[controller] = std::move(label);
}

} // namespace

void append_midi_cc_specs(std::vector<CommandSpec> &specs)
{
    auto const keywords =
        std::vector<std::string>{"midi", "cc", "controller", "automation"};

    specs.push_back(command(
        {"set", "midiCC"}, true, "Set per-note MIDI controller values.", keywords,
        targeted_edit_policy, std::make_tuple(controller_arg(), normalized_value_arg()),
        [](CommandHandlerContext &context, CommandInvocation const &invocation,
           int controller, float value) {
            auto state = increment_state(
                context.project(), context.execution.cursor,
                require_selection(context.execution),
                [](auto target, sequence::Pattern const &pattern,
                   sequence::MidiControllerNumber cc, float normalized) {
                    return sequence::modify::set_midi_cc(std::move(target), pattern, cc,
                                                         normalized);
                },
                invocation.input.pattern, midi_controller(controller), value);
            context.edit_project() = std::move(state);
            return unchanged_selection_result(minfo("MIDI CC Set"), context.execution);
        }));

    specs.push_back(command(
        {"shift", "midiCC"}, true, "Shift per-note MIDI controller values.", keywords,
        targeted_edit_policy, std::make_tuple(controller_arg(), normalized_delta_arg()),
        [](CommandHandlerContext &context, CommandInvocation const &invocation,
           int controller, float amount) {
            auto state = increment_state(
                context.project(), context.execution.cursor,
                require_selection(context.execution),
                [](auto target, sequence::Pattern const &pattern,
                   sequence::MidiControllerNumber cc, float delta) {
                    return sequence::modify::shift_midi_cc(std::move(target), pattern,
                                                           cc, delta);
                },
                invocation.input.pattern, midi_controller(controller), amount);
            context.edit_project() = std::move(state);
            return unchanged_selection_result(minfo("MIDI CC Shifted"),
                                              context.execution);
        }));

    specs.push_back(
        command({"remove", "midiCC"}, true, "Remove per-note MIDI controller values.",
                keywords, targeted_edit_policy, std::make_tuple(controller_arg()),
                [](CommandHandlerContext &context, CommandInvocation const &invocation,
                   int controller) {
                    auto state = increment_state(
                        context.project(), context.execution.cursor,
                        require_selection(context.execution),
                        [](auto target, sequence::Pattern const &pattern,
                           sequence::MidiControllerNumber cc) {
                            return sequence::modify::remove_midi_cc(std::move(target),
                                                                    pattern, cc);
                        },
                        invocation.input.pattern, midi_controller(controller));
                    context.edit_project() = std::move(state);
                    return unchanged_selection_result(minfo("MIDI CC Removed"),
                                                      context.execution);
                }));

    auto const label_keywords =
        std::vector<std::string>{"midi", "cc", "controller", "label", "alias"};
    specs.push_back(
        command({"set", "midiCCLabel"}, false,
                "Set a persistent MIDI controller label.", label_keywords,
                project_edit_policy, std::make_tuple(controller_arg(), label_arg()),
                [](CommandHandlerContext &context, CommandInvocation const &,
                   int controller, std::string const &label) {
                    auto state = context.project();
                    set_label(state, midi_controller(controller), label);
                    context.edit_project() = std::move(state);
                    return make_result(minfo("MIDI CC Label Set"));
                }));

    specs.push_back(command(
        {"remove", "midiCCLabel"}, false, "Remove a persistent MIDI controller label.",
        label_keywords, project_edit_policy, std::make_tuple(controller_arg()),
        [](CommandHandlerContext &context, CommandInvocation const &, int controller) {
            auto state = context.project();
            state.midi_cc_labels.erase(midi_controller(controller));
            context.edit_project() = std::move(state);
            return make_result(minfo("MIDI CC Label Removed"));
        }));
}

} // namespace xen::catalog_detail
