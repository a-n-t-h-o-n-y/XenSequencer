#include <catch2/catch_test_macros.hpp>

#include <xen/keymap.hpp>

using namespace xen;

namespace
{

auto temporary_keymap_file() -> juce::File
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("xen-keymap-store", ".json", false);
}

auto find_binding(KeymapSnapshot const &snapshot, std::string const &context,
                  KeymapTrigger const &trigger) -> KeymapBinding const *
{
    auto const context_it = snapshot.bindings.find(context);
    if (context_it == snapshot.bindings.end())
    {
        return nullptr;
    }
    auto const binding_it =
        std::ranges::find(context_it->second, trigger, &KeymapBinding::trigger);
    return binding_it == context_it->second.end() ? nullptr : &*binding_it;
}

} // namespace

TEST_CASE("Keymap store persists overrides and explicit unbindings", "[core][keymap]")
{
    auto const file = temporary_keymap_file();
    auto const trigger = KeymapTrigger{.key = "ArrowLeft"};

    {
        auto store = KeymapStore{file};
        auto const initial = store.snapshot();
        REQUIRE(find_binding(initial, "sequence", trigger) != nullptr);

        auto const disabled =
            store.set_override(initial.revision, "sequence", trigger, std::nullopt);
        CHECK(find_binding(disabled, "sequence", trigger) == nullptr);
        REQUIRE(disabled.overrides.size() == 1);
        CHECK_FALSE(disabled.overrides.front().target.has_value());
    }

    auto restored_store = KeymapStore{file};
    auto const restored = restored_store.snapshot();
    CHECK(find_binding(restored, "sequence", trigger) == nullptr);
    REQUIRE(restored.overrides.size() == 1);

    auto const defaults =
        restored_store.remove_override(restored.revision, "sequence", trigger);
    CHECK(find_binding(defaults, "sequence", trigger) != nullptr);
    CHECK(defaults.overrides.empty());
}

TEST_CASE("Keymap store rejects stale mutations", "[core][keymap]")
{
    auto store = KeymapStore{temporary_keymap_file()};
    auto const snapshot = store.snapshot();
    auto const target = KeymapTarget{
        .type = KeymapTargetType::Command,
        .value = "rest",
    };
    store.set_override(snapshot.revision, "sequence", {.key = "q"}, target);

    CHECK_THROWS_AS(
        store.set_override(snapshot.revision, "sequence", {.key = "w"}, target),
        std::invalid_argument);
}
