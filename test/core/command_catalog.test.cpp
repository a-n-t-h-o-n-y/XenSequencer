#include <algorithm>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <xen/bridge_serialize.hpp>
#include <xen/command.hpp>
#include <xen/command_catalog.hpp>
#include <xen/command_dsl.hpp>
#include <xen/command_transaction.hpp>

using namespace xen;

namespace
{

auto policy_for(std::string const &text) -> CommandPolicy
{
    auto const result = bind_invocation(parse_command_chain(text).front());
    REQUIRE(std::holds_alternative<BoundStep>(result));
    auto const &step = std::get<BoundStep>(result);
    if (std::holds_alternative<ExecutableCommand>(step))
    {
        return std::get<ExecutableCommand>(step).policy;
    }
    REQUIRE(std::holds_alternative<ExecutableHistoryNavigation>(step));
    return std::get<ExecutableHistoryNavigation>(step).policy;
}

auto test_definition(CommandPolicy policy, bool uses_submission_effects)
    -> CommandDefinition
{
    auto definition = command_dsl::command(
        {"test"}, false, "Test command.", policy, std::make_tuple(),
        [](CommandHandlerContext &, CommandInvocation const &) {
            return minfo("test");
        });
    return CommandDefinition{
        .metadata = std::move(definition.metadata),
        .policy = definition.policy,
        .uses_submission_effects = uses_submission_effects,
        .bind = std::move(definition.bind),
    };
}

} // namespace

TEST_CASE("Catalog binds command chain to executable handlers",
          "[core][command][catalog]")
{
    auto const chain = parse_command_chain("version; again; set key 7; undo");
    auto const result = bind_chain(chain);

    REQUIRE(std::holds_alternative<std::vector<BoundStep>>(result));
    auto const &bound = std::get<std::vector<BoundStep>>(result);
    REQUIRE(bound.size() == 4);

    REQUIRE(std::holds_alternative<ExecutableCommand>(bound[0]));
    CHECK(std::holds_alternative<RepeatPrevious>(bound[1]));
    REQUIRE(std::holds_alternative<ExecutableCommand>(bound[2]));
    REQUIRE(std::holds_alternative<ExecutableHistoryNavigation>(bound[3]));
    CHECK(std::get<ExecutableCommand>(bound[0]).canonical == "version");
    CHECK(std::get<ExecutableCommand>(bound[2]).canonical == "set key 7");
    CHECK(std::get<ExecutableHistoryNavigation>(bound[3]).canonical == "undo");
}

TEST_CASE("Catalog binder applies defaults for commands", "[core][command][catalog]")
{
    auto const set_key_invocation = parse_command_chain("set key")[0];
    auto const set_key_result = bind_invocation(set_key_invocation);
    REQUIRE(std::holds_alternative<BoundStep>(set_key_result));
    auto const &set_key_step = std::get<BoundStep>(set_key_result);
    REQUIRE(std::holds_alternative<ExecutableCommand>(set_key_step));
    auto const &set_key_command = std::get<ExecutableCommand>(set_key_step);
    REQUIRE(set_key_command.execute);

    auto state = PluginState{
        .timeline = XenTimeline{ProjectState{}},
    };
    auto transaction = CommandTransaction{state, SubmissionEffects::FailurePoint::None};
    auto context = CommandExecutionContext{};
    auto const key_result = set_key_command.execute(transaction, context);
    CHECK(key_result.status.second == "Key Set to 0.");
}

TEST_CASE("Catalog binder reports unknown command", "[core][command][catalog]")
{
    auto const invocation = parse_command_chain("notACommand 123")[0];
    auto const result = bind_invocation(invocation);

    REQUIRE(std::holds_alternative<CatalogBindError>(result));
    auto const &error = std::get<CatalogBindError>(result);
    CHECK(error.kind == CatalogBindErrorKind::UnknownCommand);
    CHECK(error.token == "notACommand");
}

TEST_CASE("Catalog binder reports invalid and missing arguments",
          "[core][command][catalog]")
{
    auto const invalid_invocation = parse_command_chain("set key nope")[0];
    auto const invalid_result = bind_invocation(invalid_invocation);
    REQUIRE(std::holds_alternative<CatalogBindError>(invalid_result));
    auto const &invalid_error = std::get<CatalogBindError>(invalid_result);
    CHECK(invalid_error.kind == CatalogBindErrorKind::InvalidArgument);
    CHECK(invalid_error.message == "Invalid argument 'key': Invalid integer: nope");

    auto const out_of_range_result =
        bind_invocation(parse_command_chain("set key 128")[0]);
    REQUIRE(std::holds_alternative<CatalogBindError>(out_of_range_result));
    auto const &out_of_range_error = std::get<CatalogBindError>(out_of_range_result);
    CHECK(out_of_range_error.kind == CatalogBindErrorKind::InvalidArgument);
    CHECK(out_of_range_error.message ==
          "Invalid argument 'key': Must be in range [-127, 127].");

    auto const invalid_velocity =
        bind_invocation(parse_command_chain("set velocity 1.5")[0]);
    REQUIRE(std::holds_alternative<CatalogBindError>(invalid_velocity));
    auto const &velocity_error = std::get<CatalogBindError>(invalid_velocity);
    CHECK(velocity_error.kind == CatalogBindErrorKind::InvalidArgument);
    CHECK(velocity_error.message ==
          "Invalid argument 'velocity': Must be in range [0, 1].");

    auto const missing_invocation = parse_command_chain("load cell")[0];
    auto const missing_result = bind_invocation(missing_invocation);
    REQUIRE(std::holds_alternative<CatalogBindError>(missing_result));
    auto const &missing_error = std::get<CatalogBindError>(missing_result);
    CHECK(missing_error.kind == CatalogBindErrorKind::MissingArgument);
    CHECK(missing_error.message == "Missing argument: filename");
}

TEST_CASE("Catalog binder rejects trailing arguments and unsupported patterns",
          "[core][command][catalog]")
{
    auto const trailing = bind_invocation(parse_command_chain("set key 3 extra")[0]);
    REQUIRE(std::holds_alternative<CatalogBindError>(trailing));
    CHECK(std::get<CatalogBindError>(trailing).kind ==
          CatalogBindErrorKind::UnexpectedArgument);

    auto const pattern = bind_invocation(parse_command_chain("+2 set key 3")[0]);
    REQUIRE(std::holds_alternative<CatalogBindError>(pattern));
    CHECK(std::get<CatalogBindError>(pattern).kind ==
          CatalogBindErrorKind::PatternPrefixNotAllowed);

    auto const accepted =
        bind_invocation(parse_command_chain("+2 set velocity 0.5")[0]);
    REQUIRE(std::holds_alternative<BoundStep>(accepted));
    CHECK(std::holds_alternative<ExecutableCommand>(std::get<BoundStep>(accepted)));
}

TEST_CASE("Catalog binds non-bootstrap commands to executors",
          "[core][command][catalog]")
{
    auto const chain =
        parse_command_chain("set baseFrequency 333; load scales; save cell foo");
    auto const result = bind_chain(chain);

    REQUIRE(std::holds_alternative<std::vector<BoundStep>>(result));
    auto const &bound = std::get<std::vector<BoundStep>>(result);
    REQUIRE(bound.size() == 3);
    CHECK(std::holds_alternative<ExecutableCommand>(bound[0]));
    CHECK(std::holds_alternative<ExecutableCommand>(bound[1]));
    CHECK(std::holds_alternative<ExecutableCommand>(bound[2]));
}

TEST_CASE("Catalog bind_chain stops at first bind error", "[core][command][catalog]")
{
    auto const chain = parse_command_chain("version; notACommand; set key 4");
    auto const result = bind_chain(chain);

    REQUIRE(std::holds_alternative<CatalogBindError>(result));
    auto const &error = std::get<CatalogBindError>(result);
    CHECK(error.kind == CatalogBindErrorKind::UnknownCommand);
    CHECK(error.token == "notACommand");
}

TEST_CASE("Catalog rejects incoherent command policies",
          "[core][command][catalog][policy]")
{
    auto const none = CommandPolicy{ProjectOperation::None,  LibraryAccess::None,
                                    WorkspaceAccess::None,   FileAccess::None,
                                    TargetRequirement::None, RepeatPolicy::Never,
                                    HistoryPolicy::None};

    for (auto const project : {ProjectOperation::None, ProjectOperation::ReplaceHistory,
                               ProjectOperation::NavigateHistory})
    {
        auto policy = none;
        policy.project = project;
        policy.target = TargetRequirement::Cell;
        CHECK_THROWS_AS(CommandCatalog({test_definition(policy, false)}),
                        std::invalid_argument);
    }

    for (auto const history :
         {HistoryPolicy::Commit, HistoryPolicy::AmendCompatibleTransform})
    {
        auto policy = none;
        policy.project = ProjectOperation::Read;
        policy.history = history;
        CHECK_THROWS_AS(CommandCatalog({test_definition(policy, false)}),
                        std::invalid_argument);
    }

    for (auto const project :
         {ProjectOperation::ReplaceHistory, ProjectOperation::NavigateHistory})
    {
        auto policy = none;
        policy.project = project;
        policy.history = HistoryPolicy::Commit;
        CHECK_THROWS(CommandCatalog({test_definition(policy, false)}));
    }

    {
        auto policy = none;
        policy.files = FileAccess::Read;
        CHECK_THROWS_AS(CommandCatalog({test_definition(policy, false)}),
                        std::invalid_argument);
    }
    {
        CHECK_THROWS_AS(CommandCatalog({test_definition(none, true)}),
                        std::invalid_argument);
    }
}

TEST_CASE("Catalog exposes complete backend command policies",
          "[core][command][catalog][policy]")
{
    CHECK(policy_for("version") ==
          CommandPolicy{ProjectOperation::None, LibraryAccess::None,
                        WorkspaceAccess::None, FileAccess::None,
                        TargetRequirement::None, RepeatPolicy::Never,
                        HistoryPolicy::None});
    CHECK(policy_for("duplicate").target == TargetRequirement::CellOrElement);
    CHECK(policy_for("set key 1").history == HistoryPolicy::Commit);
    CHECK(policy_for("cut").files == FileAccess::Write);
    CHECK(policy_for("paste").files == FileAccess::Read);
    CHECK(policy_for("load cell example").workspace == WorkspaceAccess::Read);
    CHECK(policy_for("save cell example").project == ProjectOperation::Read);
    CHECK(policy_for("load chords").library == LibraryAccess::Mutate);
    CHECK(policy_for("undo").project == ProjectOperation::NavigateHistory);
    CHECK(policy_for("composition loop start 0").history == HistoryPolicy::Commit);
    CHECK(policy_for("composition loop end 0").project == ProjectOperation::Edit);
    CHECK(policy_for("composition row rename 0 lead").history == HistoryPolicy::Commit);
    CHECK(policy_for("composition row channel 0 channel-1").project ==
          ProjectOperation::Edit);
    CHECK(policy_for("set duration 3/4").history == HistoryPolicy::Commit);
    CHECK(policy_for("composition cell assign -2 8 S1").project ==
          ProjectOperation::Edit);
    CHECK(policy_for("composition cell clear 0 0").history == HistoryPolicy::Commit);
    CHECK(policy_for("composition cell move -2 8 3 -4").project ==
          ProjectOperation::Edit);

    auto const chord = policy_for("chord");
    CHECK(chord.project == ProjectOperation::Edit);
    CHECK(chord.library == LibraryAccess::Read);
    CHECK(chord.target == TargetRequirement::Cell);
    CHECK(chord.history == HistoryPolicy::AmendCompatibleTransform);

    auto const arp = policy_for("arp");
    CHECK(arp.repeat == RepeatPolicy::OnSuccessfulProjectChange);
    CHECK(arp.history == HistoryPolicy::AmendCompatibleTransform);
}

TEST_CASE("Catalog metadata exposes path, args, and docs", "[core][command][catalog]")
{
    auto const &metadata = command_metadata();
    REQUIRE_FALSE(metadata.empty());

    auto const set_pitch = std::find_if(
        metadata.begin(), metadata.end(), [](CatalogCommandMetadata const &entry) {
            return entry.path == std::vector<std::string>{"set", "pitch"};
        });
    REQUIRE(set_pitch != metadata.end());
    REQUIRE(set_pitch->arguments.size() == 1);
    CHECK(set_pitch->arguments[0].kind == "pitch | modulator");

    auto const set_velocity = std::find_if(
        metadata.begin(), metadata.end(), [](CatalogCommandMetadata const &entry) {
            return entry.path == std::vector<std::string>{"set", "velocity"};
        });
    REQUIRE(set_velocity != metadata.end());
    REQUIRE(set_velocity->arguments.size() == 1);
    CHECK(set_velocity->arguments[0].kind == "velocity | modulator");
    REQUIRE(set_velocity->arguments[0].constraints.size() == 1);
    CHECK(set_velocity->arguments[0].constraints[0].kind == "range");
    CHECK(set_velocity->arguments[0].constraints[0].minimum == 0.0);
    CHECK(set_velocity->arguments[0].constraints[0].maximum == 1.0);
    CHECK(set_velocity->keywords ==
          std::vector<std::string>{"volume", "gain", "level", "loudness"});

    auto const set_key = std::find_if(
        metadata.begin(), metadata.end(), [](CatalogCommandMetadata const &entry) {
            return entry.path == std::vector<std::string>{"set", "key"};
        });
    REQUIRE(set_key != metadata.end());
    REQUIRE(set_key->arguments.size() == 1);
    CHECK(set_key->arguments[0].kind == "transpose_key");
    CHECK(set_key->arguments[0].display_name == "key");
    CHECK_FALSE(set_key->arguments[0].required);
    REQUIRE(set_key->arguments[0].default_value.has_value());
    CHECK(*set_key->arguments[0].default_value == "0");
    REQUIRE(set_key->arguments[0].constraints.size() == 1);
    CHECK(set_key->arguments[0].constraints[0].minimum == -127.0);
    CHECK(set_key->arguments[0].constraints[0].maximum == 127.0);
    CHECK_FALSE(set_key->description.empty());
    CHECK(set_key->keywords.empty());

    auto const entire_scale = std::find_if(
        metadata.begin(), metadata.end(), [](CatalogCommandMetadata const &entry) {
            return entry.path == std::vector<std::string>{"shift", "entireScale"};
        });
    REQUIRE(entire_scale != metadata.end());
    CHECK(entire_scale->accepts_pattern_prefix == false);

    auto const load_keys = std::find_if(
        metadata.begin(), metadata.end(), [](CatalogCommandMetadata const &entry) {
            return entry.path == std::vector<std::string>{"load", "keys"};
        });
    CHECK(load_keys == metadata.end());

    auto const load_keys_result =
        bind_invocation(parse_command_chain("load keys").front());
    REQUIRE(std::holds_alternative<CatalogBindError>(load_keys_result));
    CHECK(std::get<CatalogBindError>(load_keys_result).kind ==
          CatalogBindErrorKind::UnknownCommand);
}

TEST_CASE("Catalog bridge payload serializes schema version and keywords",
          "[core][command][catalog][bridge]")
{
    auto const payload = bridge::make_catalog_payload(command_metadata());
    CHECK(payload.at("schema_version") == 3);

    auto const &commands = payload.at("commands");
    REQUIRE_FALSE(commands.empty());
    for (auto const &command : commands)
    {
        CHECK(command.contains("keywords"));
    }

    auto const set_velocity = std::find_if(
        commands.begin(), commands.end(), [](nlohmann::json const &command) {
            return command.at("path") == std::vector<std::string>{"set", "velocity"};
        });
    REQUIRE(set_velocity != commands.end());
    CHECK(set_velocity->at("keywords") ==
          std::vector<std::string>{"volume", "gain", "level", "loudness"});
    REQUIRE(set_velocity->at("arguments").size() == 1);
    CHECK(set_velocity->at("arguments")[0].at("kind") == "velocity | modulator");
    CHECK(set_velocity->at("arguments")[0]
              .at("constraints")[0]
              .at("minimum")
              .get<double>() == 0.0);
    CHECK(set_velocity->at("arguments")[0]
              .at("constraints")[0]
              .at("maximum")
              .get<double>() == 1.0);
}

TEST_CASE("Catalog docs are generated from catalog metadata",
          "[core][command][catalog]")
{
    auto const docs = catalog_docs();
    REQUIRE_FALSE(docs.empty());

    auto const again_doc =
        std::find_if(docs.begin(), docs.end(), [](Documentation const &doc) {
            return doc.signature.id == "again";
        });
    REQUIRE(again_doc != docs.end());
    CHECK(again_doc->signature.pattern_arg == false);

    auto const mirror_doc =
        std::find_if(docs.begin(), docs.end(), [](Documentation const &doc) {
            return doc.signature.id == "mirror";
        });
    REQUIRE(mirror_doc != docs.end());
    CHECK(mirror_doc->signature.pattern_arg == true);
    REQUIRE_FALSE(mirror_doc->signature.arguments.empty());
    CHECK(mirror_doc->signature.arguments[0] == "[pitch: centerPitch=0]");
}
