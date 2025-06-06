#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/command.hpp>
#include <xen/state.hpp>

using namespace xen;

auto ps = PluginState{
    .timeline = XenTimeline{TrackedState{
        .sequencer = SequencerState{},
        .aux = AuxState{},
    }},
};

TEST_CASE("Construct a Command", "[Command]")
{
    std::unique_ptr<CommandBase> cmd_ptr =
        cmd(signature("add", ArgInfo<int>{"param1"}, ArgInfo<int>{"param2"}),
            "cmd description",
            [](PluginState &, int a, int b) -> std::pair<MessageLevel, std::string> {
                return {MessageLevel::Debug, std::to_string(a + b)};
            });

    CHECK(cmd_ptr
              ->execute(ps,
                        SplitInput{
                            .pattern = {0, {1}},
                            .words = {"1", "2"},
                        })
              .second == "3");
}

TEST_CASE("Command has Pattern", "[Command]")
{
    auto cmd_ptr = cmd(signature("add", arg<sequence::Pattern>(""),
                                 ArgInfo<int>{"param1"}, ArgInfo<int>{"param2"}),
                       "cmd description",
                       [](PluginState &, sequence::Pattern const &, int a,
                          int b) -> std::pair<MessageLevel, std::string> {
                           return {MessageLevel::Debug, std::to_string(a + b)};
                       });

    CHECK(cmd_ptr
              ->execute(ps,
                        SplitInput{
                            .pattern = {0, {1}},
                            .words = {"5", "6"},
                        })
              .second == "11");
}

TEST_CASE("Construct a CommandGroup", "[Command]")
{
    auto cmd_group_ptr = cmd_group("set");

    cmd_group_ptr->add(
        cmd(signature("add", ArgInfo<int>{"param1"}, ArgInfo<int>{"param2"}),
            "cmd description",
            [](PluginState &, int a, int b) -> std::pair<MessageLevel, std::string> {
                return {MessageLevel::Debug, std::to_string(a + b)};
            }));

    cmd_group_ptr->add(
        cmd(signature("sub", ArgInfo<int>{"param1"}, ArgInfo<int>{"param2"}),
            "cmd description",
            [](PluginState &, int a, int b) -> std::pair<MessageLevel, std::string> {
                return {MessageLevel::Debug, std::to_string(a - b)};
            }));

    {
        auto group2 = cmd_group("do");

        group2->add(cmd(
            signature("add", ArgInfo<int>{"param1"}, ArgInfo<int>{"param2"}),
            "cmd description",
            [](PluginState &, int a, int b) -> std::pair<MessageLevel, std::string> {
                return {MessageLevel::Debug, std::to_string(a + b)};
            }));

        group2->add(cmd(
            signature("sub", ArgInfo<int>{"param1"}, ArgInfo<int>{"param2"}),
            "cmd description",
            [](PluginState &, int a, int b) -> std::pair<MessageLevel, std::string> {
                return {MessageLevel::Debug, std::to_string(a - b)};
            }));

        cmd_group_ptr->add(std::move(group2));
    }

    CHECK(cmd_group_ptr
              ->execute(ps,
                        SplitInput{
                            .pattern = {0, {1}},
                            .words = {"add", "1", "2"},
                        })
              .second == "3");

    CHECK(cmd_group_ptr
              ->execute(ps,
                        SplitInput{
                            .pattern = {0, {1}},
                            .words = {"sub", "1", "2"},
                        })
              .second == "-1");

    CHECK(cmd_group_ptr
              ->execute(ps,
                        SplitInput{
                            .pattern = {0, {1}},
                            .words = {"do", "add", "1", "2"},
                        })
              .second == "3");

    CHECK(cmd_group_ptr
              ->execute(ps,
                        SplitInput{
                            .pattern = {0, {1}},
                            .words = {"do", "sub", "1", "2"},
                        })
              .second == "-1");
}

TEST_CASE("Pattern", "[Command]")
{
    auto cmd_group_ptr = cmd_group("set");

    cmd_group_ptr->add(cmd(signature("add", ArgInfo<sequence::Pattern>{""},
                                     ArgInfo<int>{"param1"}, ArgInfo<int>{"param2"}),
                           "cmd description",
                           [](PluginState &, sequence::Pattern const &p, int a,
                              int b) -> std::pair<MessageLevel, std::string> {
                               CHECK(p == sequence::Pattern{1, {1, 2}});
                               return {MessageLevel::Debug, std::to_string(a + b)};
                           }));

    cmd_group_ptr->add(
        cmd(signature("sub", ArgInfo<int>{"param1"}, ArgInfo<int>{"param2"}),
            "cmd description",
            [](PluginState &, int a, int b) -> std::pair<MessageLevel, std::string> {
                return {MessageLevel::Debug, std::to_string(a - b)};
            }));

    CHECK(cmd_group_ptr
              ->execute(ps,
                        SplitInput{
                            .pattern = {1, {1, 2}},
                            .words = {"add", "1", "2"},
                        })
              .second == "3");

    CHECK(cmd_group_ptr
              ->execute(ps,
                        SplitInput{
                            .pattern = {0, {1}},
                            .words = {"sub", "1", "2"},
                        })
              .second == "-1");
}