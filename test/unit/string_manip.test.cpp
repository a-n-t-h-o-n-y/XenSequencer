#include <catch2/catch_test_macros.hpp>

#include <xen/string_manip.hpp>

TEST_CASE("minimize_spaces preserves quoted content", "[unit][string]")
{
    auto const input = R"(   hello   "x   y"   world   )";
    REQUIRE(xen::minimize_spaces(input) == R"(hello "x   y" world)");
}

TEST_CASE("split_quoted_string preserves quoted and json payloads", "[unit][string]")
{
    auto const input = R"(cmd "a b" {"k": 1, "v": "x y"} tail)";
    auto const parts = xen::split_quoted_string(input);

    REQUIRE(parts.size() == 4);
    REQUIRE(parts[0] == "cmd");
    REQUIRE(parts[1] == "a b");
    REQUIRE(parts[2] == R"({"k": 1, "v": "x y"})");
    REQUIRE(parts[3] == "tail");
}

TEST_CASE("first-word and pop-first-word honor quotes", "[unit][string]")
{
    auto const input = R"(   "hello there"   rest of line)";

    REQUIRE(xen::get_first_word(input) == "hello there");
    REQUIRE(xen::strip(xen::pop_first_word(input)) == "rest of line");
}

TEST_CASE("split and join round-trip with delimiters", "[unit][string]")
{
    auto const words = xen::split("a;b;c", ';');
    REQUIRE(words.size() == 3);
    REQUIRE(words[0] == "a");
    REQUIRE(words[1] == "b");
    REQUIRE(words[2] == "c");

    REQUIRE(xen::join(words, ';') == "a;b;c");
}

TEST_CASE("word_count handles quoted words", "[unit][string]")
{
    REQUIRE(xen::word_count(R"(hello "wide world" test)") == 3);
    REQUIRE(xen::word_count("   ") == 0);
}
