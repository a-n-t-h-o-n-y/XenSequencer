#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <xen/message_level.hpp>
#include <xen/xen_processor.hpp>

using namespace xen;

TEST_CASE("Processor command 'again' replays previous command string",
          "[processor][commands]")
{
    auto processor = XenProcessor{};

    REQUIRE(processor.execute_command_string("set key 17").first == MessageLevel::Info);
    auto const after_first = processor.get_engine_snapshot();
    REQUIRE(after_first.engine.key == 17);

    auto const first_commit = after_first.commit_id;
    auto const [again_level, _again_message] =
        processor.execute_command_string("again");
    CHECK(again_level == MessageLevel::Info);

    auto const after_again = processor.get_engine_snapshot();
    CHECK(after_again.engine.key == 17);
    CHECK(after_again.commit_id > first_commit);
}

TEST_CASE("Processor multi-command executes in order and returns last command status",
          "[processor][commands]")
{
    auto processor = XenProcessor{};

    auto const [level, message] = processor.execute_command_string(
        "set key 3; set baseFrequency 300; version");

    CHECK(level == MessageLevel::Info);
    CHECK(message == "v0.3.0");

    auto const snapshot = processor.get_engine_snapshot();
    CHECK(snapshot.engine.key == 3);
    CHECK(snapshot.engine.base_frequency == Catch::Approx(300.f));
}

TEST_CASE("Processor returns command-not-found error for invalid command",
          "[processor][commands]")
{
    auto processor = XenProcessor{};

    auto const before = processor.get_engine_snapshot();
    auto const [level, message] = processor.execute_command_string("notacommand 123");
    auto const after = processor.get_engine_snapshot();

    CHECK(level == MessageLevel::Error);
    CHECK(message == "Command not found: notacommand");
    CHECK(after.engine == before.engine);
    CHECK(after.commit_id == before.commit_id);
}

TEST_CASE("Processor multi-command commits prior mutations even if a later command errors",
          "[processor][commands]")
{
    auto processor = XenProcessor{};

    auto const before = processor.get_engine_snapshot();
    auto const [level, message] =
        processor.execute_command_string("set key 22; notARealCommand");

    CHECK(level == MessageLevel::Error);
    CHECK(message == "Command not found: notARealCommand");

    auto const after = processor.get_engine_snapshot();
    CHECK(after.commit_id > before.commit_id);
    CHECK(after.engine.key == 22);
}
