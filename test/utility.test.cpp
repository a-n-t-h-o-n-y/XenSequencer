#include <catch2/catch_test_macros.hpp>

#include <xen/utility.hpp>

TEST_CASE("normalize_interval", "[Utility]")
{
    CHECK(xen::normalize_interval(0, 12) == 0);
    CHECK(xen::normalize_interval(1, 12) == 1);
    CHECK(xen::normalize_interval(5, 12) == 5);
    CHECK(xen::normalize_interval(11, 12) == 11);
    CHECK(xen::normalize_interval(12, 12) == 0);
    CHECK(xen::normalize_interval(13, 12) == 1);
    CHECK(xen::normalize_interval(18, 12) == 6);
    CHECK(xen::normalize_interval(24, 12) == 0);

    CHECK(xen::normalize_interval(-1, 12) == 11);
    CHECK(xen::normalize_interval(-2, 12) == 10);
    CHECK(xen::normalize_interval(-5, 12) == 7);
    CHECK(xen::normalize_interval(-11, 12) == 1);
    CHECK(xen::normalize_interval(-12, 12) == 0);
    CHECK(xen::normalize_interval(-13, 12) == 11);
    CHECK(xen::normalize_interval(-18, 12) == 6);
    CHECK(xen::normalize_interval(-23, 12) == 1);
    CHECK(xen::normalize_interval(-24, 12) == 0);
    CHECK(xen::normalize_interval(-25, 12) == 11);
}

TEST_CASE("get_octave", "[Utility]")
{
    CHECK(xen::utility::get_octave(0, 12) == 0);
    CHECK(xen::utility::get_octave(1, 12) == 0);
    CHECK(xen::utility::get_octave(11, 12) == 0);
    CHECK(xen::utility::get_octave(12, 12) == 1);
    CHECK(xen::utility::get_octave(23, 12) == 1);
    CHECK(xen::utility::get_octave(24, 12) == 2);

    CHECK(xen::utility::get_octave(-1, 12) == -1);
    CHECK(xen::utility::get_octave(-2, 12) == -1);
    CHECK(xen::utility::get_octave(-11, 12) == -1);
    CHECK(xen::utility::get_octave(-12, 12) == -1);
    CHECK(xen::utility::get_octave(-13, 12) == -2);
    CHECK(xen::utility::get_octave(-23, 12) == -2);
    CHECK(xen::utility::get_octave(-24, 12) == -2);
    CHECK(xen::utility::get_octave(-25, 12) == -3);
}