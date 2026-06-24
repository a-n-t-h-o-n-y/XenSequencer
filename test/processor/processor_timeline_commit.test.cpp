#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <utility>

#include <xen/message_level.hpp>
#include <xen/selection.hpp>
#include <xen/xen_processor.hpp>

using namespace xen;

namespace
{

auto current_context(XenProcessor const &processor,
                     std::optional<SelectionPath> selection = std::nullopt)
    -> CommandContext
{
    return {
        .selection = std::move(selection),
        .expected_project_revision = processor.get_project_snapshot().project_revision,
    };
}

} // namespace

TEST_CASE("Mutating commands advance history identity and project revision",
          "[processor][timeline][commit]")
{
    auto processor = XenProcessor{};
    auto const initial = processor.get_project_snapshot();

    auto const result =
        processor.execute_command_string("set key 12", current_context(processor));
    CHECK(result.status.first == MessageLevel::Info);

    auto const after = processor.get_project_snapshot();
    CHECK(after.history_entry_id != initial.history_entry_id);
    CHECK(after.project_revision != initial.project_revision);
    CHECK(after.project.pitch.transposition == 12);
}
