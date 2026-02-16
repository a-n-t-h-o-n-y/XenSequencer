#include <catch2/catch_test_macros.hpp>

#include <xen/utility.hpp>

TEST_CASE("normalize_pitch wraps around positive and negative ranges", "[unit][utility]")
{
    REQUIRE(xen::utility::normalize_pitch(0, 12) == 0U);
    REQUIRE(xen::utility::normalize_pitch(1, 12) == 1U);
    REQUIRE(xen::utility::normalize_pitch(11, 12) == 11U);
    REQUIRE(xen::utility::normalize_pitch(12, 12) == 0U);
    REQUIRE(xen::utility::normalize_pitch(13, 12) == 1U);
    REQUIRE(xen::utility::normalize_pitch(-1, 12) == 11U);
    REQUIRE(xen::utility::normalize_pitch(-12, 12) == 0U);
    REQUIRE(xen::utility::normalize_pitch(-13, 12) == 11U);
}

TEST_CASE("get_octave handles positive and negative pitches", "[unit][utility]")
{
    REQUIRE(xen::utility::get_octave(0, 12) == 0);
    REQUIRE(xen::utility::get_octave(11, 12) == 0);
    REQUIRE(xen::utility::get_octave(12, 12) == 1);
    REQUIRE(xen::utility::get_octave(24, 12) == 2);

    REQUIRE(xen::utility::get_octave(-1, 12) == -1);
    REQUIRE(xen::utility::get_octave(-12, 12) == -1);
    REQUIRE(xen::utility::get_octave(-13, 12) == -2);
    REQUIRE(xen::utility::get_octave(-25, 12) == -3);
}
