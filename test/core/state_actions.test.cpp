#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <xen/message_level.hpp>
#include <xen/xen_processor.hpp>

using namespace xen;

namespace
{

auto check_editor_stable(EngineSnapshot const &after, EngineSnapshot const &before)
    -> void
{
    CHECK(after.editor.selected == before.editor.selected);
    CHECK(after.editor.input_mode == before.editor.input_mode);
}

} // namespace

TEST_CASE("Select sequence changes measure and clears nested cell selection",
          "[core][actions]")
{
    auto processor = XenProcessor{};

    REQUIRE(processor.execute_command_string("split 2").first == MessageLevel::Info);
    REQUIRE(processor.execute_command_string("move down").first == MessageLevel::Debug);

    auto before = processor.get_engine_snapshot();
    REQUIRE_FALSE(before.editor.selected.cell.empty());

    auto const [level, _message] = processor.execute_command_string("select sequence 3");
    CHECK(level == MessageLevel::Debug);

    auto const after = processor.get_engine_snapshot();
    CHECK(after.editor.selected.measure == 3);
    CHECK(after.editor.selected.cell.empty());
}

TEST_CASE("Invalid select sequence returns error and keeps state unchanged",
          "[core][actions]")
{
    auto processor = XenProcessor{};
    auto const before = processor.get_engine_snapshot();

    auto const [level, message] = processor.execute_command_string("select sequence 16");
    auto const after = processor.get_engine_snapshot();

    CHECK(level == MessageLevel::Error);
    CHECK(message.find("Invalid Sequence Index") != std::string::npos);
    CHECK(after.engine == before.engine);
    check_editor_stable(after, before);
    CHECK(after.commit_id == before.commit_id);
}

TEST_CASE("Base frequency command clamps to supported range", "[core][actions]")
{
    auto processor = XenProcessor{};

    REQUIRE(processor.execute_command_string("set baseFrequency -5").first ==
            MessageLevel::Info);
    CHECK(processor.get_engine_snapshot().engine.base_frequency ==
          Catch::Approx(20.f));

    REQUIRE(processor.execute_command_string("set baseFrequency 50000").first ==
            MessageLevel::Info);
    CHECK(processor.get_engine_snapshot().engine.base_frequency ==
          Catch::Approx(20'000.f));
}

TEST_CASE("Set key validates range and does not mutate on invalid input",
          "[core][actions]")
{
    auto processor = XenProcessor{};
    auto const before = processor.get_engine_snapshot();

    auto const [invalid_level, invalid_message] =
        processor.execute_command_string("set key 128");
    auto const after_invalid = processor.get_engine_snapshot();

    CHECK(invalid_level == MessageLevel::Error);
    CHECK(invalid_message ==
          "Invalid Key Value: 128. Must be in range [-127, 127].");
    CHECK(after_invalid.engine == before.engine);
    check_editor_stable(after_invalid, before);
    CHECK(after_invalid.commit_id == before.commit_id);

    auto const [valid_level, _valid_message] =
        processor.execute_command_string("set key -127");
    CHECK(valid_level == MessageLevel::Info);
    CHECK(processor.get_engine_snapshot().engine.key == -127);
}

TEST_CASE("Set sequence name validates index and mutates only target sequence",
          "[core][actions]")
{
    auto processor = XenProcessor{};
    auto const before = processor.get_engine_snapshot();

    auto const [level, _message] =
        processor.execute_command_string("set sequence name \"bravo\" 2");
    REQUIRE(level == MessageLevel::Info);

    auto const after_valid = processor.get_engine_snapshot();
    CHECK(after_valid.engine.sequence_names[2] == "bravo");
    CHECK(after_valid.engine.sequence_names[0] == before.engine.sequence_names[0]);

    auto const before_invalid = after_valid;
    auto const [invalid_level, invalid_message] =
        processor.execute_command_string("set sequence name \"bad\" 99");
    auto const after_invalid = processor.get_engine_snapshot();

    CHECK(invalid_level == MessageLevel::Error);
    CHECK(invalid_message == "Invalid Sequence Index");
    CHECK(after_invalid.engine == before_invalid.engine);
    check_editor_stable(after_invalid, before_invalid);
    CHECK(after_invalid.commit_id == before_invalid.commit_id);
}
