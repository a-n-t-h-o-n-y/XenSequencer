#include <catch2/catch_test_macros.hpp>

#include <xen/command.hpp>
#include <xen/constants.hpp>
#include <xen/guide_text.hpp>
#include <xen/message_level.hpp>
#include <xen/state.hpp>
#include <xen/xen_command_tree.hpp>

using namespace xen;

namespace
{

auto make_plugin_state() -> PluginState
{
    return PluginState{
        .timeline = XenTimeline{
            TimelineState{
                .sequencer = {},
                .aux = {},
            },
        },
    };
}

} // namespace

TEST_CASE("split_input parses quoted arguments with default pattern", "[core][command]")
{
    auto const split = split_input("set sequence name \"my seq\" 2");

    CHECK(split.pattern == sequence::Pattern{0, {1}});
    REQUIRE(split.words.size() == 5);
    CHECK(split.words[0] == "set");
    CHECK(split.words[1] == "sequence");
    CHECK(split.words[2] == "name");
    CHECK(split.words[3] == "my seq");
    CHECK(split.words[4] == "2");
}

TEST_CASE("split_input parses explicit pattern prefix", "[core][command]")
{
    auto const split = split_input("+5 4 set key 7");

    CHECK(split.pattern == sequence::Pattern{5, {4}});
    REQUIRE(split.words.size() == 3);
    CHECK(split.words[0] == "set");
    CHECK(split.words[1] == "key");
    CHECK(split.words[2] == "7");
}

TEST_CASE("command tree executes case-insensitive command ids", "[core][command]")
{
    auto tree = create_command_tree();
    auto ps = make_plugin_state();

    auto const [level, message] = tree.execute(ps, split_input("SeT KeY -12"));
    CHECK(level == MessageLevel::Info);
    CHECK(message == "Key Set to -12.");
    CHECK(ps.timeline.get_state().sequencer.key == -12);
    CHECK(ps.timeline.get_commit_flag());
}

TEST_CASE("command tree parses quoted nested command args", "[core][command]")
{
    auto tree = create_command_tree();
    auto ps = make_plugin_state();

    auto const [level, message] =
        tree.execute(ps, split_input("set sequence name \"my lead\" 3"));

    CHECK(level == MessageLevel::Info);
    CHECK(message == "Sequence Name Set");
    CHECK(ps.timeline.get_state().sequencer.sequence_names[3] == "my lead");
    CHECK(ps.timeline.get_commit_flag());
}

TEST_CASE("command tree reports missing command tokens", "[core][command]")
{
    auto tree = create_command_tree();
    auto ps = make_plugin_state();

    auto const [empty_level, empty_message] = tree.execute(ps, split_input("   "));
    CHECK(empty_level == MessageLevel::Error);
    CHECK(empty_message == "No command given.");

    auto const [missing_level, missing_message] =
        tree.execute(ps, split_input("set notACommand"));
    CHECK(missing_level == MessageLevel::Error);
    CHECK(missing_message == "Command not found: notACommand");
}

TEST_CASE("guide text and id completion use command tree structure", "[core][command]")
{
    auto tree = create_command_tree();

    CHECK(generate_guide_text(tree, "") == "");
    CHECK(generate_guide_text(tree, "   ") == "");
    CHECK(generate_guide_text(tree, "set ba") == "seFrequency");
    CHECK(complete_id(tree, "set ba") == "seFrequency");

    CHECK(generate_guide_text(tree, "set baseFrequency") == "[Float: freq=440]");
    CHECK(generate_guide_text(tree, "set baseFrequency ") == "[Float: freq=440]");
    CHECK(complete_id(tree, "set baseFrequency ") == "");
}
