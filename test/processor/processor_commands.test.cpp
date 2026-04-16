#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <string>
#include <vector>

#include <sequence/sequence.hpp>

#include <xen/message_level.hpp>
#include <xen/selection.hpp>
#include <xen/xen_processor.hpp>

using namespace xen;

namespace
{

auto singleton_sequence_cell_selection(
    std::vector<std::size_t> const &indices) -> SelectedState
{
    auto selected = SelectedState{};
    for (auto const index : indices)
    {
        selected.path.push_back(
            {.kind = SelectionStepKind::Element, .index = 0});
        selected.path.push_back(
            {.kind = SelectionStepKind::SequenceCell, .index = index});
    }
    return selected;
}

} // namespace

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
    CHECK(after_again.commit_id >= first_commit);
}

TEST_CASE("Processor multi-command executes in order and returns last command status",
          "[processor][commands]")
{
    auto processor = XenProcessor{};

    auto const [level, message] = processor.execute_command_string(
        "set key 3; set baseFrequency 300; version");

    CHECK(level == MessageLevel::Info);
    CHECK(message == "v0.3.1");

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
    CHECK(version_message == "v0.3.1");

    auto const [again_level, again_message] = processor.execute_command_string("again");
    CHECK(again_level == MessageLevel::Info);
    CHECK(again_message == "v0.3.1");
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
    CHECK(after_again.commit_id >= first_commit);
}

TEST_CASE("Processor command-chain splitting ignores semicolons in quoted args",
          "[processor][commands]")
{
    auto processor = XenProcessor{};

    auto const [level, message] = processor.execute_command_string(
        "load measure \"semi;colon\"; version");

    CHECK(level == MessageLevel::Error);
    CHECK(message == "File Not Found: " +
                     processor.plugin_state.config.current_sequence_directory
                         .getChildFile("semi;colon.xss")
                         .getFullPathName()
                         .toStdString());
}

TEST_CASE("Processor command-chain splitting ignores semicolons in structured args",
          "[processor][commands]")
{
    auto processor = XenProcessor{};

    auto const [level, message] = processor.execute_command_string(
        "load measure {\"label\":\"semi;colon\"}; version");

    CHECK(level == MessageLevel::Error);
    CHECK(message == "File Not Found: " +
                     processor.plugin_state.config.current_sequence_directory
                         .getChildFile("{\"label\":\"semi;colon\"}.xss")
                         .getFullPathName()
                         .toStdString());
}

TEST_CASE("Processor carries selection context across chained commands",
          "[processor][commands]")
{
    auto processor = XenProcessor{};

    REQUIRE(processor.execute_command_string("split 2").first == MessageLevel::Info);
    auto state = processor.plugin_state.timeline.get_state();
    state.aux.selected = singleton_sequence_cell_selection({0});
    processor.plugin_state.timeline.stage(std::move(state));

    auto const [level, message] =
        processor.execute_command_string("move right; note 7");

    CHECK(level == MessageLevel::Info);
    CHECK(message == "Note Created");

    auto const after = processor.get_engine_snapshot();
    CHECK(after.editor.selected == singleton_sequence_cell_selection({1}));

    auto const &selected =
        get_selected_cell_const(after.engine.measure, after.editor.selected);
    REQUIRE(selected.elements.size() == 1);
    REQUIRE(std::holds_alternative<sequence::Note>(selected.elements.front()));
    CHECK(std::get<sequence::Note>(selected.elements.front()).pitch == 7);
}

TEST_CASE("Processor measure defaults use updated chain context", "[processor][commands]")
{
    auto processor = XenProcessor{};

    auto const [level, message] = processor.execute_command_string(
        "set measure timeSignature 7/8");

    CHECK(level == MessageLevel::Info);
    CHECK(message == "Measure TimeSignature Set: 7/8");

    auto const after = processor.get_engine_snapshot();
    CHECK(after.engine.measure.time_signature.numerator == 7);
    CHECK(after.engine.measure.time_signature.denominator == 8);
}

TEST_CASE("Processor rejects unknown commands", "[processor][commands]")
{
    auto processor = XenProcessor{};

    CHECK(processor.execute_command_string("notACommand").first ==
          MessageLevel::Error);
    CHECK(processor.execute_command_string("notACommand 123").first ==
          MessageLevel::Error);
}
