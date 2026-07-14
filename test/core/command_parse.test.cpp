#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>

#include <xen/command.hpp>

using namespace xen;

namespace
{

auto parse_error(std::string const &input) -> std::string
{
    try
    {
        static_cast<void>(parse_command_chain(input));
    }
    catch (std::invalid_argument const &error)
    {
        return error.what();
    }
    return "";
}

} // namespace

TEST_CASE("parse_command_chain parses quoted arguments with default pattern",
          "[core][command]")
{
    auto const chain = parse_command_chain("load cell \"my seq\"");
    REQUIRE(chain.size() == 1);
    auto const &parsed = chain.front().input;

    CHECK(parsed.pattern == sequence::Pattern{0, {1}});
    REQUIRE(parsed.words.size() == 3);
    CHECK(parsed.words[0] == "load");
    CHECK(parsed.words[1] == "cell");
    CHECK(parsed.words[2] == "my seq");
    CHECK(parsed.word_spans[2].begin == 10);
    CHECK(parsed.word_spans[2].end == 18);
}

TEST_CASE("parse_command_chain parses explicit pattern prefix", "[core][command]")
{
    auto const chain = parse_command_chain("+5 4 set key 7");
    REQUIRE(chain.size() == 1);
    auto const &parsed = chain.front().input;

    CHECK(parsed.pattern == sequence::Pattern{5, {4}});
    REQUIRE(parsed.words.size() == 3);
    CHECK(parsed.words[0] == "set");
    CHECK(parsed.words[1] == "key");
    CHECK(parsed.words[2] == "7");
    CHECK(parsed.word_spans[0].begin == 5);
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
    CHECK(chain[0].source_span.begin == 2);
    CHECK(chain[0].source_span.end == 11);
}

TEST_CASE(
    "parse_command_chain ignores semicolons inside quoted and structured arguments",
    "[core][command]")
{
    auto const quoted_chain = parse_command_chain("load cell \"semi;colon\"; version");

    REQUIRE(quoted_chain.size() == 2);
    CHECK(quoted_chain[0].canonical_segment == "load cell \"semi;colon\"");
    REQUIRE(quoted_chain[0].input.words.size() == 3);
    CHECK(quoted_chain[0].input.words[2] == "semi;colon");
    CHECK(quoted_chain[1].canonical_segment == "version");

    auto const structured_chain =
        parse_command_chain("load cell {\"label\":\"semi;colon\"}; version");

    REQUIRE(structured_chain.size() == 2);
    CHECK(structured_chain[0].canonical_segment ==
          "load cell {\"label\":\"semi;colon\"}");
    REQUIRE(structured_chain[0].input.words.size() == 3);
    CHECK(structured_chain[0].input.words[2] == "{\"label\":\"semi;colon\"}");
    CHECK(structured_chain[1].canonical_segment == "version");
}

TEST_CASE("parse_command_chain preserves quoted and structured token contents",
          "[core][command]")
{
    auto const chain = parse_command_chain(
        "custom \"\" \"a\\\"b\" {\"outer\": { \"text\": \"a; b\" }}; version");

    REQUIRE(chain.size() == 2);
    REQUIRE(chain[0].input.words.size() == 4);
    CHECK(chain[0].input.words[0] == "custom");
    CHECK(chain[0].input.words[1].empty());
    CHECK(chain[0].input.words[2] == "a\\\"b");
    CHECK(chain[0].input.words[3] == "{\"outer\": { \"text\": \"a; b\" }}");
    CHECK(chain[0].canonical_segment ==
          "custom \"\" \"a\\\"b\" {\"outer\": { \"text\": \"a; b\" }}");
}

TEST_CASE("strict command parsing reports malformed syntax offsets", "[core][command]")
{
    CHECK(parse_error("version \"oops") == "Unterminated quoted string at offset 8");
    CHECK(parse_error("version \"oops\\") ==
          "Dangling escape in quoted string at offset 13");
    CHECK(parse_error("load cell {x") == "Unmatched opening brace at offset 10");
    CHECK(parse_error("version }") == "Unexpected closing brace at offset 8");
}
