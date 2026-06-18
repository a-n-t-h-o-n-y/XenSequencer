#include "command_catalog_specs_internal.hpp"
#include <xen/command_dsl.hpp>

namespace xen::catalog_detail
{

void append_edit_specs(std::vector<CommandSpec> &specs)
{
    specs.push_back(
        make_spec({"move", "left"}, false, "Move selection left.",
                  std::make_tuple(optional_arg<std::size_t>("Unsigned", "amount", 1)),
                  [](CommandInvocation const &, std::size_t amount) {
                      return MoveSelectionAction{.direction = MoveDirection::Left,
                                                 .amount = amount};
                  }));

    specs.push_back(
        make_spec({"move", "right"}, false, "Move selection right.",
                  std::make_tuple(optional_arg<std::size_t>("Unsigned", "amount", 1)),
                  [](CommandInvocation const &, std::size_t amount) {
                      return MoveSelectionAction{.direction = MoveDirection::Right,
                                                 .amount = amount};
                  }));

    specs.push_back(
        make_spec({"move", "up"}, false, "Move selection up one level.",
                  std::make_tuple(optional_arg<std::size_t>("Unsigned", "amount", 1)),
                  [](CommandInvocation const &, std::size_t amount) {
                      return MoveSelectionAction{.direction = MoveDirection::Up,
                                                 .amount = amount};
                  }));

    specs.push_back(
        make_spec({"move", "down"}, false, "Move selection down one level.",
                  std::make_tuple(optional_arg<std::size_t>("Unsigned", "amount", 1)),
                  [](CommandInvocation const &, std::size_t amount) {
                      return MoveSelectionAction{.direction = MoveDirection::Down,
                                                 .amount = amount};
                  }));

    specs.push_back(make_spec(
        {"note"}, false, "Create a note at the current selection.",
        std::make_tuple(optional_arg<int>("Int", "pitch", 0),
                        optional_arg<float>("Float", "velocity", 100.f / 127.f),
                        optional_arg<float>("Float", "delay", 0.f),
                        optional_arg<float>("Float", "gate", 1.f)),
        [](CommandInvocation const &, int pitch, float velocity, float delay,
           float gate) {
            return CreateNoteAction{
                .pitch = pitch,
                .velocity = velocity,
                .delay = delay,
                .gate = gate,
            };
        }));

    specs.push_back(
        make_spec({"delete"}, false, "Delete the current selection.", std::make_tuple(),
                  [](CommandInvocation const &) { return DeleteSelectionAction{}; }));

    specs.push_back(
        make_spec({"split"}, false, "Split the current selection.",
                  std::make_tuple(optional_arg<std::size_t>("Unsigned", "count", 2)),
                  [](CommandInvocation const &, std::size_t count) {
                      return SplitSelectionAction{.count = count};
                  }));

    specs.push_back(make_spec(
        {"lift"}, false, "Lift the current selection up one level.", std::make_tuple(),
        [](CommandInvocation const &) { return LiftSelectionAction{}; }));
}

} // namespace xen::catalog_detail
