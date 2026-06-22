#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <sequence/sequence.hpp>

#include <xen/selection.hpp>
#include <xen/xen_processor.hpp>

using namespace xen;

namespace
{

auto execute(XenProcessor &processor, std::string const &command,
             std::optional<SelectionPath> selection = std::nullopt)
    -> CommandApplicationResult
{
    return processor.execute_command_string(
        command, {.selection = std::move(selection),
                  .expected_project_revision =
                      processor.get_project_snapshot().project_revision});
}

} // namespace

TEST_CASE("Processor requires current revisions only for project-aware submissions",
          "[processor][commands][context]")
{
    auto processor = XenProcessor{};
    auto const before = processor.get_project_snapshot();

    auto const version_result =
        processor.execute_command_string("version", CommandContext{});
    CHECK(version_result.status.first == MessageLevel::Info);

    auto const missing_result =
        processor.execute_command_string("set key 9", CommandContext{});
    CHECK(missing_result.status.first == MessageLevel::Error);
    CHECK(missing_result.status.second == "expected project revision is required");
    CHECK(processor.get_project_snapshot().project == before.project);
}

TEST_CASE("Processor rejects stale revisions before resolving selection",
          "[processor][commands][context]")
{
    auto processor = XenProcessor{};
    auto const stale_revision = processor.get_project_snapshot().project_revision;
    REQUIRE(execute(processor, "set key 3").status.first == MessageLevel::Info);
    auto const before_rejection = processor.get_project_snapshot();

    auto const result = processor.execute_command_string(
        "set key 9", {.expected_project_revision = stale_revision});

    CHECK(result.status.first == MessageLevel::Error);
    CHECK(result.status.second ==
          "stale project revision: expected " + std::to_string(stale_revision.value()) +
              ", current " + std::to_string(before_rejection.project_revision.value()));
}

TEST_CASE("Processor rejects missing and invalid targeted selections",
          "[processor][commands][selection]")
{
    auto processor = XenProcessor{};

    auto const missing = execute(processor, "delete");
    CHECK(missing.status.second == "selection is required");

    auto const invalid = processor.execute_command_string(
        "delete", {.selection = select_element_in_cell({}, 9),
                   .expected_project_revision =
                       processor.get_project_snapshot().project_revision});
    CHECK(invalid.status.first == MessageLevel::Error);
    CHECK(invalid.status.second == "selection path does not resolve");
}

TEST_CASE("Processor rejects wrong-kind targets", "[processor][commands][selection]")
{
    auto processor = XenProcessor{};
    REQUIRE(execute(processor, "note 5", SelectionPath{}).status.first ==
            MessageLevel::Info);
    auto const result =
        execute(processor, "set weight 0.5", select_element_in_cell({}, 0));
    CHECK(result.status.first == MessageLevel::Error);
    CHECK(result.status.second == "selection must resolve to a cell");
}

TEST_CASE("Processor reports unchanged-selection suggestions for transforms",
          "[processor][commands][selection]")
{
    auto processor = XenProcessor{};
    auto const selection = SelectionPath{};

    REQUIRE(execute(processor, "note 5", selection).status.first == MessageLevel::Info);
    auto const result = execute(processor, "set velocity 0.5", selection);
    CHECK(result.status.first == MessageLevel::Info);
    REQUIRE(result.suggested_selection.has_value());
    CHECK(*result.suggested_selection == selection);
}

TEST_CASE("Removed navigation and input-mode commands are unknown",
          "[processor][commands][selection]")
{
    auto processor = XenProcessor{};

    auto const move = processor.execute_command_string("move right", CommandContext{});
    CHECK(move.status.first == MessageLevel::Error);
    CHECK(move.status.second == "Command not found: move");

    auto const input_mode =
        processor.execute_command_string("inputMode gate", CommandContext{});
    CHECK(input_mode.status.first == MessageLevel::Error);
    CHECK(input_mode.status.second == "Command not found: inputMode");
}
