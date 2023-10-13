#include <iostream>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include <xen/command.hpp>
#include <xen/guide_text.hpp>
#include <xen/message_type.hpp>
#include <xen/xen_timeline.hpp>

using namespace xen;

TEST_CASE("Command", "[Command Tree]")
{
    auto const command_tree = cmd_group(
        "", ArgInfo<std::string>{"command_name"},
        cmd("browse", "",
            [](XenTimeline &) {
                return std::pair{MessageType::Error, "Can't Browse..."};
            }),
        cmd(
            "help", "",
            [](XenTimeline &, std::string s, int v) {
                return std::pair{MessageType::Success,
                                 "found: " + std::to_string(v) + " and: " + s};
            },
            ArgInfo<std::string>{"names", "WOW"}, ArgInfo<int>{"value", 5}),
        pattern(cmd(
            "pat", "",
            [](XenTimeline &, sequence::Pattern const &p, int v) {
                auto msg = "Pattern: +" + std::to_string(p.offset) + " ";
                for (auto const &i : p.intervals)
                {
                    msg += std::to_string(i) + " ";
                }
                msg += "\nvalue: " + std::to_string(v);
                return std::pair{MessageType::Success, msg};
            },
            ArgInfo<int>{"value", 3})),
        cmd_group("group", ArgInfo<std::string>{"subcommand"},
                  cmd("browse", "",
                      [](XenTimeline &) {
                          return std::pair{MessageType::Success, "Browsing..."};
                      }),
                  cmd(
                      "help", "",
                      [](XenTimeline &, std::string s, int v) {
                          return std::pair{MessageType::Success,
                                           "found: " + std::to_string(v) +
                                               " and: " + s};
                      },
                      ArgInfo<std::string>{"names", "WOW"}, ArgInfo<int>{"value", 5}),
                  pattern(cmd(
                      "pat", "",
                      [](XenTimeline &, sequence::Pattern const &p, int v) {
                          auto msg = "Pattern: +" + std::to_string(p.offset) + " ";
                          for (auto const &i : p.intervals)
                          {
                              msg += std::to_string(i) + " ";
                          }
                          msg += "\nvalue: " + std::to_string(v);
                          return std::pair{MessageType::Success, msg};
                      },
                      ArgInfo<int>{"value", 3}))));

    auto tl = XenTimeline{{}, {}};
    {
        auto const [type, msg] = execute(command_tree, tl, "help \"hi world\" 3");
        CHECK(type == MessageType::Success);
        CHECK(msg == "found: 3 and: hi world");
    }
    {
        auto const [type, msg] = execute(command_tree, tl, "browse");
        CHECK(type == MessageType::Error);
        CHECK(msg == "Can't Browse...");
    }
    {
        auto const [type, msg] = execute(command_tree, tl, "+5 4 pat");
        CHECK(type == MessageType::Success);
        CHECK(msg == "Pattern: +5 4 \nvalue: 3");
    }
    {
        auto const [type, msg] =
            execute(command_tree, tl, "group help \"thing  \" 432");
        CHECK(type == MessageType::Success);
        CHECK(msg == "found: 432 and: thing  ");
    }
}

TEST_CASE("Command", "[generate_guide_text]")
{
    CHECK(generate_guide_text("") == "");
    CHECK(generate_guide_text("     ") == "");
    CHECK(generate_guide_text("c") == "");
    CHECK(generate_guide_text("cu") == "t");
    CHECK(generate_guide_text("addm") == "easure [TimeSignature: duration=4/4]");
    CHECK(generate_guide_text("move") == " [String: direction]");
    CHECK(generate_guide_text("move ") == "[String: direction]");
    CHECK(generate_guide_text("move     ") == "[String: direction]");
    CHECK(generate_guide_text("move up") == "");
    CHECK(generate_guide_text("human") == "ize [InputMode: mode]");
    CHECK(generate_guide_text("human ") == "");
    CHECK(generate_guide_text("humanize") == " [InputMode: mode]");
    CHECK(generate_guide_text("humanize ") == "[InputMode: mode]");
    CHECK(generate_guide_text("humanize    ") == "[InputMode: mode]");
    CHECK(generate_guide_text("humanize velo") == "city [Float: amount=0.1]");
    CHECK(generate_guide_text("humanize velo ") == "");
    CHECK(generate_guide_text("1 2 3") == "");
    CHECK(generate_guide_text("    1 2 3") == "");
    CHECK(generate_guide_text("1      2 3") == "");
    CHECK(generate_guide_text("1 2 3humanize") == "");
    CHECK(generate_guide_text("+1 2 3 humanize velo") == "city [Float: amount=0.1]");
    CHECK(generate_guide_text("1 2 3 humanize velo") == "city [Float: amount=0.1]");
    CHECK(generate_guide_text("    1 2 3 humanize velo") == "city [Float: amount=0.1]");
    CHECK(generate_guide_text("1     2 3 humanize velo") == "city [Float: amount=0.1]");
    CHECK(generate_guide_text("humanize           velo") == "city [Float: amount=0.1]");
    CHECK(generate_guide_text("addm ") == "");
    CHECK(generate_guide_text(" addm") == "easure [TimeSignature: duration=4/4]");
    CHECK(generate_guide_text("   addm") == "easure [TimeSignature: duration=4/4]");
    CHECK(generate_guide_text("   aDDm") == "easure [TimeSignature: duration=4/4]");
    CHECK(generate_guide_text("   addmeasure") == " [TimeSignature: duration=4/4]");
    CHECK(generate_guide_text("   ADDMEasuRE") == " [TimeSignature: duration=4/4]");
    CHECK(generate_guide_text("   addmeasure ") == "[TimeSignature: duration=4/4]");
    CHECK(generate_guide_text("   addmeasure   ") == "[TimeSignature: duration=4/4]");
    CHECK(generate_guide_text("RANdomiZe") == " [InputMode: mode]");
    CHECK(generate_guide_text("RANdomiZe ") == "[InputMode: mode]");
    CHECK(generate_guide_text("randomize    ") == "[InputMode: mode]");
    CHECK(generate_guide_text("randomize gate") == " [Float: min=0] [Float: max=0.95]");
    CHECK(generate_guide_text("randomize gate ") == "[Float: min=0] [Float: max=0.95]");
    CHECK(generate_guide_text("randomize gate   ") ==
          "[Float: min=0] [Float: max=0.95]");
    CHECK(generate_guide_text("randomize gate 0.3") == " [Float: max=0.95]");
    CHECK(generate_guide_text("randomize gate 0.3 ") == "[Float: max=0.95]");
    CHECK(generate_guide_text("randomize gate 0.3     ") == "[Float: max=0.95]");
    CHECK(generate_guide_text("randomize gate 0.3 0.5") == "");
    CHECK(generate_guide_text("randomize asdfsd") == "");
}

TEST_CASE("Command", "[complete_id]")
{
    CHECK(complete_id("randomize ga") == "te");
    CHECK(complete_id("randomize ga  ") == "");
    CHECK(complete_id("randomize gate 0.4") == "");
    CHECK(complete_id("randomize gate 0.4 0.6") == "");
    CHECK(complete_id("randomize gate 0.4 0.6   ") == "");
    CHECK(complete_id("randomize") == "");
    CHECK(complete_id("randomize   ") == "");
    CHECK(complete_id("rand") == "omize");
    CHECK(complete_id("rand ") == "");
    CHECK(complete_id("add") == "Measure");
    CHECK(complete_id("addm") == "easure");
    CHECK(complete_id("addm 12343") == "");
}