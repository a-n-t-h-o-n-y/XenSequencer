#include <atomic>
#include <barrier>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <xen/keymap.hpp>
#include <xen/text_file.hpp>

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

void write_file(std::filesystem::path const &file, std::string const &text)
{
    auto output = std::ofstream{file, std::ios::binary | std::ios::trunc};
    REQUIRE(output.good());
    output << text;
    REQUIRE(output.good());
}

} // namespace

TEST_CASE("Missing keymap document is an empty resource", "[core][keymap]")
{
    auto store = KeymapStore{temporary_keymap_file()};
    auto const resource = store.read();

    CHECK_FALSE(resource.document.has_value());
    CHECK(resource.revision != 0);
}

TEST_CASE("Keymap store preserves arbitrary JSON values", "[core][keymap]")
{
    auto const file = temporary_keymap_file();
    auto store = KeymapStore{file};
    auto const document = nlohmann::json{
        {"unknown_context", {{{"future", true}, {"arguments", {1, "two"}}}}},
        {"schema_version", 912},
    };

    auto const written = store.write(store.read().revision, document);
    REQUIRE(written.document.has_value());
    CHECK(*written.document == document);

    auto restored = KeymapStore{file};
    auto const resource = restored.read();
    CHECK(resource == written);
}

TEST_CASE("Keymap store exposes legacy JSON without migration", "[core][keymap]")
{
    auto const file = temporary_keymap_file();
    auto const legacy = nlohmann::json{
        {"schema_version", 1},
        {"revision", 42},
        {"overrides", {{{"context", "sequence"}, {"target", nullptr}}}},
    };
    write_file(file, legacy.dump(2));

    auto store = KeymapStore{file};
    REQUIRE(store.read().document.has_value());
    CHECK(*store.read().document == legacy);
}

TEST_CASE("Keymap store accepts scalar and null documents", "[core][keymap]")
{
    auto store = KeymapStore{temporary_keymap_file()};
    auto resource = store.write(store.read().revision, "opaque");
    REQUIRE(resource.document.has_value());
    CHECK(*resource.document == "opaque");

    resource = store.write(resource.revision, nullptr);
    REQUIRE(resource.document.has_value());
    CHECK(resource.document->is_null());
}

TEST_CASE("Keymap store rejects stale writes and deletes", "[core][keymap]")
{
    auto store = KeymapStore{temporary_keymap_file()};
    auto const initial = store.read();
    auto const written = store.write(initial.revision, {{"value", 1}});

    CHECK_THROWS_AS(store.write(initial.revision, {{"value", 2}}), KeymapStorageError);
    CHECK_THROWS_AS(store.erase(initial.revision), KeymapStorageError);
    CHECK(store.read() == written);
}

TEST_CASE("Keymap store deletes persisted documents", "[core][keymap]")
{
    auto const file = temporary_keymap_file();
    auto store = KeymapStore{file};
    auto const written = store.write(store.read().revision, {{"value", true}});
    REQUIRE(std::filesystem::exists(file));

    auto const erased = store.erase(written.revision);
    CHECK_FALSE(erased.document.has_value());
    CHECK_FALSE(std::filesystem::exists(file));
}

TEST_CASE("Keymap store detects valid external changes", "[core][keymap]")
{
    auto const file = temporary_keymap_file();
    auto store = KeymapStore{file};
    auto const initial_revision = store.revision();
    write_file(file, R"({"external":{"action":"future.action"}})");

    CHECK(store.refresh());
    CHECK(store.revision() != initial_revision);
    REQUIRE(store.current().document.has_value());
    CHECK(store.current().document->at("external").at("action") == "future.action");
    CHECK_FALSE(store.refresh());
}

TEST_CASE("Keymap store reports malformed and oversized documents", "[core][keymap]")
{
    auto const malformed_file = temporary_keymap_file();
    write_file(malformed_file, "{not-json");
    auto malformed_store = KeymapStore{malformed_file};
    CHECK_THROWS_AS(malformed_store.read(), KeymapStorageError);

    auto const oversized_file = temporary_keymap_file();
    write_file(oversized_file, std::string(MAX_KEYMAP_DOCUMENT_SIZE + 1, 'x'));
    auto oversized_store = KeymapStore{oversized_file};
    CHECK_THROWS_AS(oversized_store.read(), KeymapStorageError);

    auto const unreadable_path = temporary_keymap_file().replace_extension();
    std::filesystem::create_directory(unreadable_path);
    auto unreadable_store = KeymapStore{unreadable_path};
    try
    {
        (void)unreadable_store.read();
        FAIL("Expected a keymap read error.");
    }
    catch (KeymapStorageError const &error)
    {
        CHECK(error.code == KeymapStorageErrorCode::Read);
    }
    std::filesystem::remove(unreadable_path);
}

TEST_CASE("Failed keymap writes preserve the previous resource", "[core][keymap]")
{
    auto const file = temporary_keymap_file();
    auto write_count = 0;
    auto store = KeymapStore{
        file,
        [&write_count](std::filesystem::path const &path, std::string const &text) {
            if (++write_count == 2)
            {
                throw std::runtime_error{"injected atomic write failure"};
            }
            atomic_write_text_file(path, text);
        },
    };
    auto const previous = store.write(store.read().revision, {{"value", "previous"}});

    CHECK_THROWS_AS(store.write(previous.revision, {{"value", "replacement"}}),
                    KeymapStorageError);

    CHECK(store.current() == previous);
    auto restored = KeymapStore{file};
    CHECK(restored.read() == previous);
}

TEST_CASE("Failed keymap deletes preserve the previous resource", "[core][keymap]")
{
    auto const file = temporary_keymap_file();
    auto store = KeymapStore{
        file,
        atomic_write_text_file,
        [](std::filesystem::path const &) {
            throw std::runtime_error{"injected delete failure"};
        },
    };
    auto const previous = store.write(store.read().revision, {{"value", "kept"}});

    CHECK_THROWS_AS(store.erase(previous.revision), KeymapStorageError);
    CHECK(store.current() == previous);
    CHECK(std::filesystem::exists(file));
}

TEST_CASE("Keymap stores serialize writes from the same revision", "[core][keymap]")
{
    auto const file = temporary_keymap_file();
    auto first = KeymapStore{file};
    auto second = KeymapStore{file};
    auto const revision = first.read().revision;
    CHECK(second.read().revision == revision);

    auto ready = std::barrier{2};
    auto successes = std::atomic<int>{0};
    auto conflicts = std::atomic<int>{0};
    auto write = [&](KeymapStore &store, int value) {
        ready.arrive_and_wait();
        try
        {
            (void)store.write(revision, {{"value", value}});
            successes.fetch_add(1, std::memory_order_relaxed);
        }
        catch (KeymapStorageError const &error)
        {
            if (error.code == KeymapStorageErrorCode::Conflict)
            {
                conflicts.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    auto first_thread = std::thread{write, std::ref(first), 1};
    auto second_thread = std::thread{write, std::ref(second), 2};
    first_thread.join();
    second_thread.join();

    CHECK(successes.load(std::memory_order_relaxed) == 1);
    CHECK(conflicts.load(std::memory_order_relaxed) == 1);
}
