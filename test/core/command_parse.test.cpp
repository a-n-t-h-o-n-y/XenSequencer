#include <catch2/catch_test_macros.hpp>

#include <xen/command.hpp>
#include <xen/guide_text.hpp>

using namespace xen;

TEST_CASE("split_input parses quoted arguments with default pattern", "[core][command]")
{
    auto const split = split_input("load measure \"my seq\"");

    CHECK(split.pattern == sequence::Pattern{0, {1}});
    REQUIRE(split.words.size() == 3);
    CHECK(split.words[0] == "load");
    CHECK(split.words[1] == "measure");
    CHECK(split.words[2] == "my seq");
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

TEST_CASE("parse_command_chain normalizes segments and drops empty items",
          "[core][command]")
{
    auto const chain = parse_command_chain("  set key 9  ; ; version ;   ");

    REQUIRE(chain.size() == 2);
    CHECK(chain[0].canonical_segment == "set key 9");
    CHECK(chain[0].input.pattern == sequence::Pattern{0, {1}});
    REQUIRE(chain[0].input.words.size() == 3);
    CHECK(chain[0].input.words[0] == "set");
    CHECK(chain[0].input.words[1] == "key");
    CHECK(chain[0].input.words[2] == "9");

    CHECK(chain[1].canonical_segment == "version");
    REQUIRE(chain[1].input.words.size() == 1);
    CHECK(chain[1].input.words[0] == "version");
}

TEST_CASE(
    "parse_command_chain ignores semicolons inside quoted and structured arguments",
    "[core][command]")
{
    auto const quoted_chain =
        parse_command_chain("load measure \"semi;colon\"; version");

    REQUIRE(quoted_chain.size() == 2);
    CHECK(quoted_chain[0].canonical_segment == "load measure \"semi;colon\"");
    REQUIRE(quoted_chain[0].input.words.size() == 3);
    CHECK(quoted_chain[0].input.words[2] == "semi;colon");
    CHECK(quoted_chain[1].canonical_segment == "version");

    auto const structured_chain = parse_command_chain(
        "load measure {\"label\":\"semi;colon\"}; version");

    REQUIRE(structured_chain.size() == 2);
    CHECK(structured_chain[0].canonical_segment ==
          "load measure {\"label\":\"semi;colon\"}");
    REQUIRE(structured_chain[0].input.words.size() == 3);
    CHECK(structured_chain[0].input.words[2] == "{\"label\":\"semi;colon\"}");
    CHECK(structured_chain[1].canonical_segment == "version");
}

TEST_CASE("parse_command_chain marks only exact 'again' invocation as replay",
          "[core][command]")
{
    auto const chain = parse_command_chain("again;again 2");

    REQUIRE(chain.size() == 2);
    CHECK(is_again_invocation(chain[0]));
    CHECK_FALSE(is_again_invocation(chain[1]));
}

TEST_CASE("guide text and id completion use command catalog", "[core][command]")
{
    CHECK(generate_guide_text("") == "");
    CHECK(generate_guide_text("   ") == "");
    CHECK(generate_guide_text("set ba") == "seFrequency");
    CHECK(complete_id("set ba") == "seFrequency");

    CHECK(generate_guide_text("set baseFrequency") == "[Float: freq=440]");
    CHECK(generate_guide_text("set baseFrequency ") == "[Float: freq=440]");
    CHECK(complete_id("set baseFrequency ") == "");
}
