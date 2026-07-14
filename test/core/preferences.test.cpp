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

#include <xen/preferences.hpp>
#include <xen/text_file.hpp>

using namespace xen;

namespace
{

auto temporary_preferences_file() -> std::filesystem::path
{
    static auto counter = std::atomic<std::uint64_t>{0};
    auto const path =
        std::filesystem::temp_directory_path() /
        ("xen-preferences-store-" +
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

TEST_CASE("Missing preferences document is an empty resource", "[core][preferences]")
{
    auto store = PreferencesStore{temporary_preferences_file()};
    auto const resource = store.read();

    CHECK_FALSE(resource.document.has_value());
    CHECK(resource.revision != 0);
}

TEST_CASE("Preferences store preserves unknown object fields", "[core][preferences]")
{
    auto const file = temporary_preferences_file();
    auto store = PreferencesStore{file};
    auto const document = nlohmann::json{
        {"future", {{"enabled", true}, {"arguments", {1, "two"}}}},
        {"schema_version", 912},
    };

    auto const written = store.write(store.read().revision, document);
    REQUIRE(written.document.has_value());
    CHECK(*written.document == document);

    auto restored = PreferencesStore{file};
    CHECK(restored.read() == written);
}

TEST_CASE("Preferences store rejects non-object documents", "[core][preferences]")
{
    auto store = PreferencesStore{temporary_preferences_file()};
    auto const revision = store.read().revision;

    CHECK_THROWS_AS(store.write(revision, nullptr), PreferencesStorageError);
    CHECK_THROWS_AS(store.write(revision, nlohmann::json::array()),
                    PreferencesStorageError);
    CHECK_THROWS_AS(store.write(revision, "opaque"), PreferencesStorageError);

    for (auto const &document : {"null", "[]", R"("opaque")"})
    {
        auto const file = temporary_preferences_file();
        write_file(file, document);
        auto invalid_store = PreferencesStore{file};
        CHECK_THROWS_AS(invalid_store.read(), PreferencesStorageError);
    }
}

TEST_CASE("Preferences store rejects stale writes and deletes", "[core][preferences]")
{
    auto store = PreferencesStore{temporary_preferences_file()};
    auto const initial = store.read();
    auto const written = store.write(initial.revision, {{"value", 1}});

    CHECK_THROWS_AS(store.write(initial.revision, {{"value", 2}}),
                    PreferencesStorageError);
    CHECK_THROWS_AS(store.erase(initial.revision), PreferencesStorageError);
    CHECK(store.read() == written);
}

TEST_CASE("Preferences store skips semantic no-op mutations", "[core][preferences]")
{
    auto const file = temporary_preferences_file();
    auto write_count = 0;
    auto remove_count = 0;
    auto store = PreferencesStore{
        file,
        [&write_count](std::filesystem::path const &path, std::string const &text) {
            ++write_count;
            atomic_write_text_file(path, text);
        },
        [&remove_count](std::filesystem::path const &path) {
            ++remove_count;
            PreferencesStore::remove_file(path);
        },
    };
    auto const initial = store.read();
    CHECK(store.erase(initial.revision) == initial);
    CHECK(remove_count == 0);

    auto const written = store.write(initial.revision, {{"value", true}});
    CHECK(store.write(written.revision, {{"value", true}}) == written);
    CHECK(write_count == 1);
}

TEST_CASE("Preferences store deletes persisted documents", "[core][preferences]")
{
    auto const file = temporary_preferences_file();
    auto store = PreferencesStore{file};
    auto const initial_revision = store.read().revision;
    auto const written = store.write(initial_revision, {{"value", true}});
    REQUIRE(std::filesystem::exists(file));

    auto const erased = store.erase(written.revision);
    CHECK_FALSE(erased.document.has_value());
    CHECK(erased.revision == initial_revision);
    CHECK_FALSE(std::filesystem::exists(file));
}

TEST_CASE("Preferences store detects valid external changes", "[core][preferences]")
{
    auto const file = temporary_preferences_file();
    auto store = PreferencesStore{file};
    auto const initial_revision = store.revision();
    write_file(file, R"({"external":{"reduced_motion":true}})");

    CHECK(store.refresh());
    CHECK(store.revision() != initial_revision);
    REQUIRE(store.current().document.has_value());
    CHECK(store.current().document->at("external").at("reduced_motion") == true);
    CHECK_FALSE(store.refresh());

    REQUIRE(std::filesystem::remove(file));
    CHECK(store.refresh());
    CHECK_FALSE(store.current().document.has_value());
}

TEST_CASE("Preferences store reports malformed and oversized documents",
          "[core][preferences]")
{
    auto const malformed_file = temporary_preferences_file();
    write_file(malformed_file, "{not-json");
    auto malformed_store = PreferencesStore{malformed_file};
    CHECK_THROWS_AS(malformed_store.read(), PreferencesStorageError);

    auto const oversized_file = temporary_preferences_file();
    write_file(oversized_file, std::string(MAX_PREFERENCES_DOCUMENT_SIZE + 1, 'x'));
    auto oversized_store = PreferencesStore{oversized_file};
    CHECK_THROWS_AS(oversized_store.read(), PreferencesStorageError);

    auto write_store = PreferencesStore{temporary_preferences_file()};
    CHECK_THROWS_AS(
        write_store.write(write_store.read().revision,
                          {{"value", std::string(MAX_PREFERENCES_DOCUMENT_SIZE, 'x')}}),
        PreferencesStorageError);

    auto const unreadable_path = temporary_preferences_file().replace_extension();
    std::filesystem::create_directory(unreadable_path);
    auto unreadable_store = PreferencesStore{unreadable_path};
    try
    {
        (void)unreadable_store.read();
        FAIL("Expected a preferences read error.");
    }
    catch (PreferencesStorageError const &error)
    {
        CHECK(error.code == PreferencesStorageErrorCode::Read);
    }
    std::filesystem::remove(unreadable_path);
}

TEST_CASE("Failed preferences writes preserve the previous resource",
          "[core][preferences]")
{
    auto const file = temporary_preferences_file();
    auto write_count = 0;
    auto store = PreferencesStore{
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
                    PreferencesStorageError);
    CHECK(store.current() == previous);

    auto restored = PreferencesStore{file};
    CHECK(restored.read() == previous);
}

TEST_CASE("Failed preferences deletes preserve the previous resource",
          "[core][preferences]")
{
    auto const file = temporary_preferences_file();
    auto store = PreferencesStore{
        file,
        atomic_write_text_file,
        [](std::filesystem::path const &) {
            throw std::runtime_error{"injected delete failure"};
        },
    };
    auto const previous = store.write(store.read().revision, {{"value", "kept"}});

    CHECK_THROWS_AS(store.erase(previous.revision), PreferencesStorageError);
    CHECK(store.current() == previous);
    CHECK(std::filesystem::exists(file));
}

TEST_CASE("Preferences stores serialize writes from the same revision",
          "[core][preferences]")
{
    auto const file = temporary_preferences_file();
    auto first = PreferencesStore{file};
    auto second = PreferencesStore{file};
    auto const revision = first.read().revision;
    CHECK(second.read().revision == revision);

    auto ready = std::barrier{2};
    auto successes = std::atomic<int>{0};
    auto conflicts = std::atomic<int>{0};
    auto write = [&](PreferencesStore &store, int value) {
        ready.arrive_and_wait();
        try
        {
            (void)store.write(revision, {{"value", value}});
            successes.fetch_add(1, std::memory_order_relaxed);
        }
        catch (PreferencesStorageError const &error)
        {
            if (error.code == PreferencesStorageErrorCode::Conflict)
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
