#include <catch2/catch_test_macros.hpp>

#include <sequence/sequence.hpp>

#include <xen/selection.hpp>
#include <xen/sequencer_session.hpp>

using namespace xen;

namespace
{

auto execute(SequencerSession &session, std::string const &command,
             std::optional<SelectionPath> selection = std::nullopt)
    -> CommandApplicationResult
{
    return session.execute_command_string(
        command,
        {.selection = std::move(selection),
         .expected_project_revision = session.project_snapshot().project_revision});
}

} // namespace

TEST_CASE("Targeted edits require a supplied selection", "[core][state-actions]")
{
    auto session = SequencerSession{};

    auto const result = session.execute_command_string(
        "delete",
        {.expected_project_revision = session.project_snapshot().project_revision});

    CHECK(result.status.first == MessageLevel::Error);
    CHECK(result.status.second == "selection is required");
}

TEST_CASE("Delete suggests nearest surviving sibling", "[core][state-actions]")
{
    auto session = SequencerSession{};
    REQUIRE(execute(session, "note 1", SelectionPath{}).status.first ==
            MessageLevel::Info);
    REQUIRE(execute(session, "note 2", SelectionPath{}).status.first ==
            MessageLevel::Info);

    auto const selection = select_element_in_cell({}, 0);
    auto const result = execute(session, "delete", selection);

    CHECK(result.status.first == MessageLevel::Info);
    REQUIRE(result.suggested_selection.has_value());
    CHECK(*result.suggested_selection == select_element_in_cell({}, 0));
}
