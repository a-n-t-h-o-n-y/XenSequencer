#include <catch2/catch_test_macros.hpp>

#include <xen/guide_text.hpp>
#include <xen/xen_command_tree.hpp>

TEST_CASE("Engine command guide text basic behavior", "[unit][command][guide]")
{
    auto const tree = xen::create_command_tree();

    REQUIRE(xen::generate_guide_text(tree, "") == "");
    REQUIRE(xen::generate_guide_text(tree, "   ") == "");
    REQUIRE(xen::generate_guide_text(tree, "cu") == "t");

    REQUIRE(xen::generate_guide_text(tree, "randomize g") == "ate");
}

TEST_CASE("Engine command completion IDs", "[unit][command][guide]")
{
    auto const tree = xen::create_command_tree();

    REQUIRE(xen::complete_id(tree, "rand") == "omize");
    REQUIRE(xen::complete_id(tree, "randomize ga") == "te");
}
