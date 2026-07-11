#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <utility>

#include <xen/message_level.hpp>
#include <xen/selection.hpp>
#include <xen/sequencer_session.hpp>

using namespace xen;

namespace
{

auto current_context(SequencerSession const &session,
                     std::optional<SelectionPath> selection = std::nullopt)
    -> CommandContext
{
    return {
        .selection = std::move(selection),
        .expected_project_revision = session.project_snapshot().project_revision,
    };
}

} // namespace

TEST_CASE("Mutating commands advance history identity and project revision",
          "[processor][timeline][commit]")
{
    auto session = SequencerSession{};
    auto const initial = session.project_snapshot();

    auto const result =
        session.execute_command_string("set key 12", current_context(session));
    CHECK(result.status.first == MessageLevel::Info);

    auto const after = session.project_snapshot();
    CHECK(after.history_entry_id != initial.history_entry_id);
    CHECK(after.project_revision != initial.project_revision);
    CHECK(after.project.composition.columns.front().pitch.transposition == 12);
}
