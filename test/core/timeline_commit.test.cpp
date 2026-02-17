#include <catch2/catch_test_macros.hpp>

#include <xen/input_mode.hpp>
#include <xen/message_level.hpp>
#include <xen/xen_processor.hpp>

using namespace xen;

TEST_CASE("Mutating commands advance commit id while non-mutating commands do not",
          "[core][timeline][commit]")
{
    auto processor = XenProcessor{};

    auto const initial = processor.get_engine_snapshot();

    auto const [version_level, _version_message] =
        processor.execute_command_string("version");
    CHECK(version_level == MessageLevel::Info);
    CHECK(processor.get_engine_snapshot().commit_id == initial.commit_id);

    auto const [select_level, _select_message] =
        processor.execute_command_string("select sequence 2");
    CHECK(select_level == MessageLevel::Debug);
    CHECK(processor.get_engine_snapshot().commit_id == initial.commit_id);

    auto const [set_key_level, _set_key_message] =
        processor.execute_command_string("set key 12");
    CHECK(set_key_level == MessageLevel::Info);

    auto const after = processor.get_engine_snapshot();
    CHECK(after.commit_id != initial.commit_id);
    CHECK(after.engine.key == 12);
}

TEST_CASE("Undo reverts engine commit while preserving current selection and input mode",
          "[core][timeline][commit]")
{
    auto processor = XenProcessor{};

    REQUIRE(processor.execute_command_string("select sequence 7").first ==
            MessageLevel::Debug);
    REQUIRE(processor.execute_command_string("inputMode gate").first ==
            MessageLevel::Info);
    REQUIRE(processor.execute_command_string("set key 9").first == MessageLevel::Info);
    auto const previous_commit = processor.get_engine_snapshot();
    REQUIRE(previous_commit.editor.selected.measure == 7);
    REQUIRE(previous_commit.editor.input_mode == InputMode::Gate);

    REQUIRE(processor.execute_command_string("set key 11").first == MessageLevel::Info);
    auto const current = processor.get_engine_snapshot();
    REQUIRE(current.engine.key == 11);
    REQUIRE(current.commit_id != previous_commit.commit_id);

    // Stage aux changes that should be discarded by undo's reset_stage().
    REQUIRE(processor.execute_command_string("select sequence 2").first ==
            MessageLevel::Debug);
    REQUIRE(processor.execute_command_string("inputMode pitch").first ==
            MessageLevel::Info);

    auto const [undo_level, _undo_message] = processor.execute_command_string("undo");
    CHECK(undo_level == MessageLevel::Info);

    auto const after_undo = processor.get_engine_snapshot();
    CHECK(after_undo.commit_id == previous_commit.commit_id);
    CHECK(after_undo.engine.key == previous_commit.engine.key);
    CHECK(after_undo.editor.selected.measure == 7);
    CHECK(after_undo.editor.input_mode == InputMode::Gate);
}

TEST_CASE("New commit after undo truncates redo history", "[core][timeline][commit]")
{
    auto processor = XenProcessor{};

    REQUIRE(processor.execute_command_string("set key 1").first == MessageLevel::Info);
    REQUIRE(processor.execute_command_string("set key 2").first == MessageLevel::Info);

    auto const commit_with_key_2 = processor.get_engine_snapshot();
    REQUIRE(commit_with_key_2.engine.key == 2);

    REQUIRE(processor.execute_command_string("undo").first == MessageLevel::Info);
    auto const after_undo = processor.get_engine_snapshot();
    REQUIRE(after_undo.engine.key == 1);

    REQUIRE(processor.execute_command_string("set key 3").first == MessageLevel::Info);
    auto const after_new_commit = processor.get_engine_snapshot();
    REQUIRE(after_new_commit.engine.key == 3);
    REQUIRE(after_new_commit.commit_id != commit_with_key_2.commit_id);

    auto const [redo_level, redo_message] = processor.execute_command_string("redo");
    CHECK(redo_level == MessageLevel::Info);
    CHECK(redo_message == "Nothing to redo.");

    auto const after_redo = processor.get_engine_snapshot();
    CHECK(after_redo.engine.key == 3);
    CHECK(after_redo.commit_id == after_new_commit.commit_id);
}
