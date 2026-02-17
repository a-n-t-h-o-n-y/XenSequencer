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

TEST_CASE("Processor stops chain execution at first command error",
          "[processor][commands]")
{
    auto processor = XenProcessor{};

    auto const [level, message] = processor.execute_command_string(
        "set key 22; notARealCommand; set key 5");

    CHECK(level == MessageLevel::Error);
    CHECK(message == "Command not found: notARealCommand");

    auto const after = processor.get_engine_snapshot();
    CHECK(after.engine.key == 22);
}

TEST_CASE("Processor 'again' replays most recent non-empty non-mutating chain",
          "[processor][commands]")
{
    auto processor = XenProcessor{};

    auto const [version_level, version_message] =
        processor.execute_command_string("version");
    CHECK(version_level == MessageLevel::Info);
    CHECK(version_message == "v0.3.0");

    auto const [again_level, again_message] = processor.execute_command_string("again");
    CHECK(again_level == MessageLevel::Info);
    CHECK(again_message == "v0.3.0");
}

TEST_CASE("Processor 'again' replays full multi-command chain",
          "[processor][commands]")
{
    auto processor = XenProcessor{};

    auto const [first_level, _first_message] = processor.execute_command_string(
        "set key 3; set baseFrequency 300");
    CHECK(first_level == MessageLevel::Info);

    auto const after_first = processor.get_engine_snapshot();
    REQUIRE(after_first.engine.key == 3);
    REQUIRE(after_first.engine.base_frequency == Catch::Approx(300.f));
    auto const first_commit = after_first.commit_id;

    auto const [again_level, again_message] = processor.execute_command_string("again");
    CHECK(again_level == MessageLevel::Info);
    CHECK(again_message == "Base Frequency Set");

    auto const after_again = processor.get_engine_snapshot();
    CHECK(after_again.engine.key == 3);
    CHECK(after_again.engine.base_frequency == Catch::Approx(300.f));
    CHECK(after_again.commit_id > first_commit);
}

TEST_CASE("Processor command-chain splitting ignores semicolons in quoted args",
          "[processor][commands]")
{
    auto processor = XenProcessor{};

    auto const [level, message] = processor.execute_command_string(
        "set sequence name \"semi;colon\" 0; version");

    CHECK(level == MessageLevel::Info);
    CHECK(message == "v0.3.0");

    auto const after = processor.get_engine_snapshot();
    CHECK(after.engine.sequence_names[0] == "semi;colon");
}
