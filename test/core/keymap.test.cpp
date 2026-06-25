#include <atomic>
#include <cstdint>
#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include <xen/keymap.hpp>

using namespace xen;

namespace
{

auto temporary_keymap_file() -> std::filesystem::path
{
    static auto counter = std::atomic<std::uint64_t>{0};
    auto const path =
        std::filesystem::temp_directory_path() /
        ("xen-keymap-store-" +
         std::to_string(counter.fetch_add(1, std::memory_order_relaxed)) + ".json");
    std::filesystem::remove(path);
    return path;
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

TEST_CASE("Default keymap exposes command bar contexts", "[core][keymap]")
{
    auto store = KeymapStore{temporary_keymap_file()};
    auto const snapshot = store.snapshot();

    auto const open_binding =
        find_binding(snapshot, "sequence", {.key = "k", .command = true});
    REQUIRE(open_binding != nullptr);
    CHECK(open_binding->target.type == KeymapTargetType::UiAction);
    CHECK(open_binding->target.value == "command.open");
    CHECK(open_binding->target.arguments.empty());

    auto const submit_binding =
        find_binding(snapshot, "command.input", {.key = "Enter"});
    REQUIRE(submit_binding != nullptr);
    CHECK(submit_binding->target.value == "command.submit");

    auto const next_completion =
        find_binding(snapshot, "command.completions", {.key = "ArrowDown"});
    REQUIRE(next_completion != nullptr);
    CHECK(next_completion->target.value == "command.completion.next");

    auto const composition_binding = find_binding(snapshot, "sequence", {.key = "Tab"});
    REQUIRE(composition_binding != nullptr);
    CHECK(composition_binding->target.value == "workspace.view.composition");

    auto const move_binding =
        find_binding(snapshot, "composition", {.key = "ArrowRight"});
    REQUIRE(move_binding != nullptr);
    CHECK(move_binding->target.type == KeymapTargetType::UiAction);
    CHECK(move_binding->target.value == "composition.selection.move");
    CHECK(move_binding->target.arguments.at("direction") == "right");

    auto const edit_binding = find_binding(snapshot, "composition", {.key = "Enter"});
    REQUIRE(edit_binding != nullptr);
    CHECK(edit_binding->target.value == "composition.cell.edit_measure");

    auto const loop_start_binding = find_binding(snapshot, "composition", {.key = "["});
    REQUIRE(loop_start_binding != nullptr);
    CHECK(loop_start_binding->target.value == "composition.loop.set_start");

    auto const sequencer_binding =
        find_binding(snapshot, "composition", {.key = "Tab"});
    REQUIRE(sequencer_binding != nullptr);
    CHECK(sequencer_binding->target.value == "workspace.view.sequencer");
}

TEST_CASE("Keymap accepts command UI action overrides in dotted contexts",
          "[core][keymap]")
{
    auto store = KeymapStore{temporary_keymap_file()};
    auto const snapshot = store.snapshot();
    auto const target = KeymapTarget{
        .type = KeymapTargetType::UiAction,
        .value = "command.close_if_empty",
    };

    auto const updated = store.set_override(snapshot.revision, "command.input",
                                            {.key = "Escape"}, target);

    auto const binding = find_binding(updated, "command.input", {.key = "Escape"});
    REQUIRE(binding != nullptr);
    CHECK(binding->target == target);
}

TEST_CASE("Keymap persists workspace view toggle UI action overrides", "[core][keymap]")
{
    auto const file = temporary_keymap_file();
    auto const trigger =
        KeymapTrigger{.key = "l", .shift = true, .command = false, .alt = false};
    auto const target = KeymapTarget{
        .type = KeymapTargetType::UiAction,
        .value = "workspace.view.toggle",
        .arguments = nlohmann::json::object(),
    };

    {
        auto store = KeymapStore{file};
        auto const snapshot = store.snapshot();

        auto const updated =
            store.set_override(snapshot.revision, "sequence", trigger, target);

        auto const binding = find_binding(updated, "sequence", trigger);
        REQUIRE(binding != nullptr);
        CHECK(binding->target == target);
    }

    auto restored_store = KeymapStore{file};
    auto const restored = restored_store.snapshot();
    auto const restored_binding = find_binding(restored, "sequence", trigger);
    REQUIRE(restored_binding != nullptr);
    CHECK(restored_binding->target == target);
}

TEST_CASE("Keymap rejects command UI action arguments", "[core][keymap]")
{
    auto const target = KeymapTarget{
        .type = KeymapTargetType::UiAction,
        .value = "command.open",
        .arguments = {{"unexpected", true}},
    };

    CHECK_THROWS_AS(validate(target), std::invalid_argument);
}

TEST_CASE("Keymap rejects workspace view toggle arguments", "[core][keymap]")
{
    auto const target = KeymapTarget{
        .type = KeymapTargetType::UiAction,
        .value = "workspace.view.toggle",
        .arguments = {{"unexpected", true}},
    };

    CHECK_THROWS_AS(validate(target), std::invalid_argument);
}

TEST_CASE("Keymap validates composition UI action arguments", "[core][keymap]")
{
    auto const move = KeymapTarget{
        .type = KeymapTargetType::UiAction,
        .value = "composition.selection.move",
        .arguments = {{"direction", "down"}, {"amount", 2}},
    };
    CHECK_NOTHROW(validate(move));

    auto invalid_move = move;
    invalid_move.arguments = {{"direction", "sideways"}, {"amount", 1}};
    CHECK_THROWS_AS(validate(invalid_move), std::invalid_argument);

    for (auto const *action :
         {"composition.cell.edit_measure", "composition.loop.set_start",
          "composition.loop.set_end", "workspace.view.composition",
          "workspace.view.sequencer"})
    {
        auto const target = KeymapTarget{
            .type = KeymapTargetType::UiAction,
            .value = action,
            .arguments = nlohmann::json::object(),
        };
        CHECK_NOTHROW(validate(target));
    }
}
