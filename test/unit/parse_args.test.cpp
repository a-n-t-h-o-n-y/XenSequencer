#include <catch2/catch_test_macros.hpp>

#include <sequence/time_signature.hpp>

#include <xen/input_mode.hpp>
#include <xen/parse_args.hpp>

TEST_CASE("parse_int supports decimal, negative, and prefixed bases", "[unit][parse]")
{
    REQUIRE(xen::parse_int("42") == 42);
    REQUIRE(xen::parse_int("-7") == -7);
    REQUIRE(xen::parse_int("0x10") == 16);
    REQUIRE_FALSE(xen::parse_int("42x").has_value());
}

TEST_CASE("parse_unsigned rejects negative and invalid values", "[unit][parse]")
{
    REQUIRE(xen::parse_unsigned<std::size_t>("12") == 12U);
    REQUIRE_FALSE(xen::parse_unsigned<std::size_t>("-1").has_value());
    REQUIRE_FALSE(xen::parse_unsigned<std::size_t>("x").has_value());
}

TEST_CASE("parse_float rejects NaN/Inf and invalid content", "[unit][parse]")
{
    REQUIRE(xen::parse_float<float>("0.25") == 0.25f);
    REQUIRE_FALSE(xen::parse_float<float>("nan").has_value());
    REQUIRE_FALSE(xen::parse_float<float>("inf").has_value());
    REQUIRE_FALSE(xen::parse_float<float>("1.2x").has_value());
}

TEST_CASE("parse_bool is case-insensitive", "[unit][parse]")
{
    REQUIRE(xen::parse_bool("true") == true);
    REQUIRE(xen::parse_bool("FALSE") == false);
    REQUIRE_FALSE(xen::parse_bool("yes").has_value());
}

TEST_CASE("parse_time_signature parses valid and rejects invalid formats", "[unit][parse]")
{
    auto const ts = xen::parse_time_signature("7/8");
    REQUIRE(ts.numerator == 7U);
    REQUIRE(ts.denominator == 8U);

    REQUIRE_THROWS_AS(xen::parse_time_signature("7/8 trailing"), std::invalid_argument);
}

TEST_CASE("generic parse handles InputMode and throws on invalid mode", "[unit][parse]")
{
    REQUIRE(xen::parse<xen::InputMode>("velocity") == xen::InputMode::Velocity);
    REQUIRE_THROWS_AS(xen::parse<xen::InputMode>("not-a-mode"), std::invalid_argument);
}
