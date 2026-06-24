#include <catch2/catch_test_macros.hpp>

#include <string>

#include <xen/message_level.hpp>
#include <xen/sequencer_session.hpp>
#include <xen/submission_effects.hpp>

using namespace xen;

TEST_CASE("Effect failures leave backend state unchanged and report rollback failures",
          "[processor][command][transaction][effects]")
{
    auto const directory =
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("xen-transaction-test", "", false);
    REQUIRE(directory.createDirectory());
    auto const tunings = directory.getChildFile("tunings");
    REQUIRE(tunings.createDirectory());
    auto const settings_file = directory.getChildFile("workspace.json");
    REQUIRE(settings_file.replaceWithText(
        "{\"schema\":1,\"sequence_directory\":\"" +
        directory.getFullPathName().toStdString() + "\",\"tuning_directory\":\"" +
        tunings.getFullPathName().toStdString() + "\"}"));
    REQUIRE(directory.getChildFile("effect-test.xss").replaceWithText("baseline"));

    for (auto const failure : {SubmissionEffects::FailurePoint::Prepare,
                               SubmissionEffects::FailurePoint::Apply,
                               SubmissionEffects::FailurePoint::ApplyAndRollback})
    {
        auto session = SequencerSession{failure, settings_file};
        auto const before = session.project_snapshot();
        auto const result = session.execute_command_string(
            "save measure effect-test",
            {.expected_project_revision = before.project_revision});

        CHECK(result.status.first == MessageLevel::Error);
        CHECK(session.project_snapshot().project_revision == before.project_revision);
        CHECK(session.project_snapshot().project == before.project);
        if (failure == SubmissionEffects::FailurePoint::ApplyAndRollback)
        {
            CHECK(result.status.second.find("rollback failed for:") !=
                  std::string::npos);
        }
    }

    CHECK(directory.deleteRecursively());
}
