#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include <juce_core/juce_core.h>

#include <signals_light/signal.hpp>

#include <sequence/sequence.hpp>

#include <xen/command.hpp>
#include <xen/guide_text.hpp>
#include <xen/message_level.hpp>
#include <xen/xen_timeline.hpp>

using namespace xen;

TEST_CASE("Command", "[Command Tree]")
{
    auto const command_tree = cmd_group(
        "", ArgInfo<std::string>{"command_name"},
        cmd("browse", "",
            [](XenTimeline &) {
                return std::pair{MessageLevel::Error, "Can't Browse..."};
            }),
        cmd(
            "help", "",
            [](XenTimeline &, std::string s, int v) {
                return minfo("found: " + std::to_string(v) + " and: " + s);
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
                return minfo(msg);
            },
            ArgInfo<int>{"value", 3})),
        cmd_group("group", ArgInfo<std::string>{"subcommand"},
                  cmd("browse", "", [](XenTimeline &) { return minfo("Browsing..."); }),
                  cmd(
                      "help", "",
                      [](XenTimeline &, std::string s, int v) {
                          return minfo("found: " + std::to_string(v) + " and: " + s);
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
                          return minfo(msg);
                      },
                      ArgInfo<int>{"value", 3}))));

    auto tl = XenTimeline{{}, {}};
    {
        auto const [type, msg] = execute(command_tree, tl, "help \"hi world\" 3");
        CHECK(type == MessageLevel::Info);
        CHECK(msg == "found: 3 and: hi world");
    }
    {
        auto const [type, msg] = execute(command_tree, tl, "browse");
        CHECK(type == MessageLevel::Error);
        CHECK(msg == "Can't Browse...");
    }
    {
        auto const [type, msg] = execute(command_tree, tl, "+5 4 pat");
        CHECK(type == MessageLevel::Info);
        CHECK(msg == "Pattern: +5 4 \nvalue: 3");
    }
    {
        auto const [type, msg] =
            execute(command_tree, tl, "group help \"thing  \" 432");
        CHECK(type == MessageLevel::Info);
        CHECK(msg == "found: 432 and: thing  ");
    }
}

TEST_CASE("Command", "[generate_guide_text]")
{
    auto sig1 = sl::Signal<void(std::string const &)>{};
    auto sig2 = sl::Signal<void()>{};
    auto mtx1 = std::mutex{};
    auto buf = std::optional<sequence::Cell>{};
    auto mtx2 = std::mutex{};
    auto const uuid = juce::Uuid{};
    auto const tree = xen::create_command_tree(sig1, sig2, mtx1, buf, mtx2, uuid);

    CHECK(generate_guide_text(tree, "") == "");
    CHECK(generate_guide_text(tree, "     ") == "");
    CHECK(generate_guide_text(tree, "c") == "");
    CHECK(generate_guide_text(tree, "cu") == "t");
    CHECK(generate_guide_text(tree, "append m") ==
          "easure [TimeSignature: duration=4/4]");
    CHECK(generate_guide_text(tree, "move") == " [String: direction]");
    CHECK(generate_guide_text(tree, "move ") == "[String: direction]");
    CHECK(generate_guide_text(tree, "move     ") == "[String: direction]");
    CHECK(generate_guide_text(tree, "move up") == " [Unsigned: amount=1]");
    CHECK(generate_guide_text(tree, "human") == "ize [InputMode: mode]");
    CHECK(generate_guide_text(tree, "human ") == "");
    CHECK(generate_guide_text(tree, "humanize") == " [InputMode: mode]");
    CHECK(generate_guide_text(tree, "humanize ") == "[InputMode: mode]");
    CHECK(generate_guide_text(tree, "humanize    ") == "[InputMode: mode]");
    CHECK(generate_guide_text(tree, "humanize velo") == "city [Float: amount=0.1]");
    CHECK(generate_guide_text(tree, "humanize velo ") == "");
    CHECK(generate_guide_text(tree, "1 2 3") == "");
    CHECK(generate_guide_text(tree, "    1 2 3") == "");
    CHECK(generate_guide_text(tree, "1      2 3") == "");
    CHECK(generate_guide_text(tree, "1 2 3humanize") == "");
    CHECK(generate_guide_text(tree, "+1 2 3 humanize velo") ==
          "city [Float: amount=0.1]");
    CHECK(generate_guide_text(tree, "1 2 3 humanize velo") ==
          "city [Float: amount=0.1]");
    CHECK(generate_guide_text(tree, "    1 2 3 humanize velo") ==
          "city [Float: amount=0.1]");
    CHECK(generate_guide_text(tree, "1     2 3 humanize velo") ==
          "city [Float: amount=0.1]");
    CHECK(generate_guide_text(tree, "humanize           velo") ==
          "city [Float: amount=0.1]");
    CHECK(generate_guide_text(tree, "append m ") == "");
    CHECK(generate_guide_text(tree, " append m") ==
          "easure [TimeSignature: duration=4/4]");
    CHECK(generate_guide_text(tree, "   append m") ==
          "easure [TimeSignature: duration=4/4]");
    CHECK(generate_guide_text(tree, "   aPPend m") ==
          "easure [TimeSignature: duration=4/4]");
    CHECK(generate_guide_text(tree, "   append measure") ==
          " [TimeSignature: duration=4/4]");
    CHECK(generate_guide_text(tree, "   APPenD MEasuRE") ==
          " [TimeSignature: duration=4/4]");
    CHECK(generate_guide_text(tree, "   append measure ") ==
          "[TimeSignature: duration=4/4]");
    CHECK(generate_guide_text(tree, "   append measure   ") ==
          "[TimeSignature: duration=4/4]");
    CHECK(generate_guide_text(tree, "RANdomiZe") == " [InputMode: mode]");
    CHECK(generate_guide_text(tree, "RANdomiZe ") == "[InputMode: mode]");
    CHECK(generate_guide_text(tree, "randomize    ") == "[InputMode: mode]");
    CHECK(generate_guide_text(tree, "randomize gate") ==
          " [Float: min=0] [Float: max=0.95]");
    CHECK(generate_guide_text(tree, "randomize gate ") ==
          "[Float: min=0] [Float: max=0.95]");
    CHECK(generate_guide_text(tree, "randomize gate   ") ==
          "[Float: min=0] [Float: max=0.95]");
    CHECK(generate_guide_text(tree, "randomize gate 0.3") == " [Float: max=0.95]");
    CHECK(generate_guide_text(tree, "randomize gate 0.3 ") == "[Float: max=0.95]");
    CHECK(generate_guide_text(tree, "randomize gate 0.3     ") == "[Float: max=0.95]");
    CHECK(generate_guide_text(tree, "randomize gate 0.3 0.5") == "");
    CHECK(generate_guide_text(tree, "randomize asdfsd") == "");
}

TEST_CASE("Command", "[complete_id]")
{
    auto sig1 = sl::Signal<void(std::string const &)>{};
    auto sig2 = sl::Signal<void()>{};
    auto mtx1 = std::mutex{};
    auto buf = std::optional<sequence::Cell>{};
    auto mtx2 = std::mutex{};
    auto const uuid = juce::Uuid{};
    auto const tree = xen::create_command_tree(sig1, sig2, mtx1, buf, mtx2, uuid);

    CHECK(complete_id(tree, "randomize ga") == "te");
    CHECK(complete_id(tree, "randomize ga  ") == "");
    CHECK(complete_id(tree, "randomize gate 0.4") == "");
    CHECK(complete_id(tree, "randomize gate 0.4 0.6") == "");
    CHECK(complete_id(tree, "randomize gate 0.4 0.6   ") == "");
    CHECK(complete_id(tree, "randomize") == "");
    CHECK(complete_id(tree, "randomize   ") == "");
    CHECK(complete_id(tree, "rand") == "omize");
    CHECK(complete_id(tree, "rand ") == "");
    CHECK(complete_id(tree, "append") == "");
    CHECK(complete_id(tree, "append m") == "easure");
    CHECK(complete_id(tree, "append m 12343") == "");
}