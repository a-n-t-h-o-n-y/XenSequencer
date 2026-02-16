#include <catch2/catch_test_macros.hpp>

#include <signals_light/signal.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/themes.hpp>
#include <xen/runtime_command_tree.hpp>
#include <xen/runtime_state.hpp>

TEST_CASE("Runtime commands emit focus and show requests", "[unit][runtime]")
{
    auto runtime = xen::RuntimeState{};
    auto tree = xen::RuntimeCommandTree{};

    auto lifetime = sl::Lifetime{};
    auto focused_id = std::string{};
    auto shown_id = std::string{};

    auto focus_slot = sl::Slot<void(std::string const &)>{
        [&](std::string const &id) { focused_id = id; }};
    focus_slot.track(lifetime);
    runtime.on_focus_request.connect(focus_slot);

    auto show_slot = sl::Slot<void(std::string const &)>{
        [&](std::string const &id) { shown_id = id; }};
    show_slot.track(lifetime);
    runtime.on_show_request.connect(show_slot);

    auto const focus_status = tree.execute(runtime, "focus command_bar");
    REQUIRE(focus_status.has_value());
    REQUIRE(focus_status->first == xen::MessageLevel::Debug);
    REQUIRE(focused_id == "command_bar");

    auto const show_status = tree.execute(runtime, "show library_view");
    REQUIRE(show_status.has_value());
    REQUIRE(show_status->first == xen::MessageLevel::Debug);
    REQUIRE(shown_id == "library_view");
}

TEST_CASE("Runtime load keys and theme updates emit shared signals", "[unit][runtime]")
{
    auto runtime = xen::RuntimeState{};
    auto tree = xen::RuntimeCommandTree{};
    auto lifetime = sl::Lifetime{};

    auto load_count = 0;
    auto theme_count = 0;

    auto load_slot = sl::Slot<void()>{[&] { ++load_count; }};
    load_slot.track(lifetime);
    runtime.shared.on_load_keys_request.connect(load_slot);

    auto theme_slot = sl::Slot<void(xen::gui::Theme const &)>{[&](xen::gui::Theme const &) {
        ++theme_count;
    }};
    theme_slot.track(lifetime);
    runtime.shared.on_theme_update.connect(theme_slot);

    auto const load_status = tree.execute(runtime, "load keys");
    REQUIRE(load_status.has_value());
    REQUIRE(load_status->first == xen::MessageLevel::Info);
    REQUIRE(load_count >= 1);

    auto const theme_status = tree.execute(runtime, "set theme dark");
    REQUIRE(theme_status.has_value());
    REQUIRE(theme_status->first == xen::MessageLevel::Info);
    REQUIRE(theme_count >= 1);

    auto const expected = xen::gui::find_theme("apollo");
    REQUIRE(runtime.shared.theme.background == expected.background);
}

TEST_CASE("Runtime command argument errors and unknown commands", "[unit][runtime]")
{
    auto runtime = xen::RuntimeState{};
    auto tree = xen::RuntimeCommandTree{};

    auto const bad_focus = tree.execute(runtime, "focus");
    REQUIRE(bad_focus.has_value());
    REQUIRE(bad_focus->first == xen::MessageLevel::Error);

    auto const bad_theme = tree.execute(runtime, "set theme does-not-exist");
    REQUIRE(bad_theme.has_value());
    REQUIRE(bad_theme->first == xen::MessageLevel::Error);

    auto const unknown = tree.execute(runtime, "welcome");
    REQUIRE_FALSE(unknown.has_value());
}

TEST_CASE("Runtime guide text and completion", "[unit][runtime]")
{
    auto tree = xen::RuntimeCommandTree{};

    REQUIRE(tree.guide_text("") == "");
    REQUIRE(tree.guide_text("fo") == "cus");
    REQUIRE(tree.guide_text("set") == "");
    REQUIRE(tree.guide_text("set t") == "heme");
    REQUIRE(tree.guide_text("set theme") == "");

    REQUIRE(tree.complete_id("fo") == "cus");
    REQUIRE(tree.complete_id("set th") == "eme");
    REQUIRE(tree.complete_id("set theme ") == "");
}
