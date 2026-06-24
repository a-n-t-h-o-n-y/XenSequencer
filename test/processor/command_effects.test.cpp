#include <catch2/catch_test_macros.hpp>

#include <string>

#include <xen/message_level.hpp>
#include <xen/submission_effects.hpp>
#include <xen/xen_processor.hpp>

using namespace xen;

TEST_CASE("Effect failures leave backend state unchanged and report rollback failures",
          "[processor][command][transaction][effects]")
{
    auto const directory =
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("xen-transaction-test", "", false);
    REQUIRE(directory.createDirectory());
    REQUIRE(directory.getChildFile("effect-test.xss").replaceWithText("baseline"));

    for (auto const failure : {SubmissionEffects::FailurePoint::Prepare,
                               SubmissionEffects::FailurePoint::Apply,
                               SubmissionEffects::FailurePoint::ApplyAndRollback})
    {
        auto processor = XenProcessor{failure};
        processor.plugin_state.workspace.sequence_directory = directory;
        auto const before = processor.get_project_snapshot();
        auto const result = processor.execute_command_string(
            "save measure effect-test",
            {.expected_project_revision = before.project_revision});

        CHECK(result.status.first == MessageLevel::Error);
        CHECK(processor.get_project_snapshot().project_revision ==
              before.project_revision);
        CHECK(processor.get_project_snapshot().project == before.project);
        if (failure == SubmissionEffects::FailurePoint::ApplyAndRollback)
        {
            CHECK(result.status.second.find("rollback failed for:") !=
                  std::string::npos);
        }
    }

    CHECK(directory.deleteRecursively());
}
