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
    REQUIRE(result.has_value());
    auto const &step = result.value();
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

    REQUIRE(result.has_value());
    auto const &bound = result.value();
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
    REQUIRE(set_key_result.has_value());
    auto const &set_key_step = set_key_result.value();
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

    REQUIRE_FALSE(result.has_value());
    auto const &error = result.error();
    CHECK(error.kind == CatalogBindErrorKind::UnknownCommand);
    CHECK(error.token == "notACommand");
}

TEST_CASE("Removed project and arrangement commands are absent from the catalog",
          "[core][command][catalog]")
{
    for (auto const &text : {"reset", "composition cell clear 0 0",
                             "load composition example", "save composition example"})
    {
        auto const result = bind_invocation(parse_command_chain(text).front());
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().kind == CatalogBindErrorKind::UnknownCommand);
    }

    auto const &metadata = default_command_catalog().metadata();
    for (auto const &removed :
         {std::vector<std::string>{"reset"},
          std::vector<std::string>{"composition", "cell", "clear"},
          std::vector<std::string>{"load", "composition"},
          std::vector<std::string>{"save", "composition"}})
    {
        CHECK(std::ranges::none_of(metadata, [&](CatalogCommandMetadata const &entry) {
            return entry.path == removed;
        }));
    }
}

TEST_CASE("Catalog binder reports invalid and missing arguments",
          "[core][command][catalog]")
{
    auto const invalid_invocation = parse_command_chain("set key nope")[0];
    auto const invalid_result = bind_invocation(invalid_invocation);
    REQUIRE_FALSE(invalid_result.has_value());
    auto const &invalid_error = invalid_result.error();
    CHECK(invalid_error.kind == CatalogBindErrorKind::InvalidArgument);
    CHECK(invalid_error.message == "Invalid argument 'key': Invalid integer: nope");

    auto const out_of_range_result =
        bind_invocation(parse_command_chain("set key 128")[0]);
    REQUIRE_FALSE(out_of_range_result.has_value());
    auto const &out_of_range_error = out_of_range_result.error();
    CHECK(out_of_range_error.kind == CatalogBindErrorKind::InvalidArgument);
    CHECK(out_of_range_error.message ==
          "Invalid argument 'key': Must be in range [-127, 127].");

    auto const invalid_velocity =
        bind_invocation(parse_command_chain("set velocity 1.5")[0]);
    REQUIRE_FALSE(invalid_velocity.has_value());
    auto const &velocity_error = invalid_velocity.error();
    CHECK(velocity_error.kind == CatalogBindErrorKind::InvalidArgument);
    CHECK(velocity_error.message ==
          "Invalid argument 'velocity': Must be in range [0, 1].");

    auto const invalid_direction =
        bind_invocation(parse_command_chain("set translateDirection sideways")[0]);
    REQUIRE_FALSE(invalid_direction.has_value());
    auto const &direction_error = invalid_direction.error();
    CHECK(direction_error.kind == CatalogBindErrorKind::InvalidArgument);
    CHECK(direction_error.message ==
          "Invalid argument 'direction': Must be up or down.");

    for (auto const *direction : {"up", "down"})
    {
        auto const valid_direction = bind_invocation(
            parse_command_chain(std::string{"set translateDirection "} + direction)[0]);
        CHECK(valid_direction.has_value());
    }

    auto const noncanonical_direction =
        bind_invocation(parse_command_chain("set translateDirection UP")[0]);
    REQUIRE_FALSE(noncanonical_direction.has_value());
    CHECK(noncanonical_direction.error().kind == CatalogBindErrorKind::InvalidArgument);

    auto const missing_invocation = parse_command_chain("load cell")[0];
    auto const missing_result = bind_invocation(missing_invocation);
    REQUIRE_FALSE(missing_result.has_value());
    auto const &missing_error = missing_result.error();
    CHECK(missing_error.kind == CatalogBindErrorKind::MissingArgument);
    CHECK(missing_error.message == "Missing argument: path");
}

TEST_CASE("Catalog binder rejects trailing arguments and unsupported patterns",
          "[core][command][catalog]")
{
    auto const trailing = bind_invocation(parse_command_chain("set key 3 extra")[0]);
    REQUIRE_FALSE(trailing.has_value());
    CHECK(trailing.error().kind == CatalogBindErrorKind::UnexpectedArgument);

    auto const pattern = bind_invocation(parse_command_chain("+2 set key 3")[0]);
    REQUIRE_FALSE(pattern.has_value());
    CHECK(pattern.error().kind == CatalogBindErrorKind::PatternPrefixNotAllowed);

    auto const accepted =
        bind_invocation(parse_command_chain("+2 set velocity 0.5")[0]);
    REQUIRE(accepted.has_value());
    CHECK(std::holds_alternative<ExecutableCommand>(accepted.value()));
}

TEST_CASE("Catalog binds non-bootstrap commands to executors",
          "[core][command][catalog]")
{
    auto const chain =
        parse_command_chain("set baseFrequency 333; load scales; save cell foo");
    auto const result = bind_chain(chain);

    REQUIRE(result.has_value());
    auto const &bound = result.value();
    REQUIRE(bound.size() == 3);
    CHECK(std::holds_alternative<ExecutableCommand>(bound[0]));
    CHECK(std::holds_alternative<ExecutableCommand>(bound[1]));
    CHECK(std::holds_alternative<ExecutableCommand>(bound[2]));
}

TEST_CASE("Catalog bind_chain stops at first bind error", "[core][command][catalog]")
{
    auto const chain = parse_command_chain("version; notACommand; set key 4");
    auto const result = bind_chain(chain);

    REQUIRE_FALSE(result.has_value());
    auto const &error = result.error();
    CHECK(error.kind == CatalogBindErrorKind::UnknownCommand);
    CHECK(error.token == "notACommand");
}

TEST_CASE("Catalog rejects incoherent command policies",
          "[core][command][catalog][policy]")
{
    auto const none = CommandPolicy{ProjectOperation::None, LibraryAccess::None,
                                    WorkspaceAccess::None,  FileAccess::None,
                                    CopyBufferAccess::None, TargetRequirement::None,
                                    RepeatPolicy::Never,    HistoryPolicy::None};

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
                        WorkspaceAccess::None, FileAccess::None, CopyBufferAccess::None,
                        TargetRequirement::None, RepeatPolicy::Never,
                        HistoryPolicy::None});
    CHECK(policy_for("duplicate").target == TargetRequirement::CellOrElement);
    CHECK(policy_for("set key 1").history == HistoryPolicy::Commit);
    CHECK(policy_for("copy").files == FileAccess::None);
    CHECK(policy_for("copy").copy_buffer == CopyBufferAccess::Write);
    CHECK(policy_for("cut").files == FileAccess::None);
    CHECK(policy_for("cut").copy_buffer == CopyBufferAccess::Write);
    CHECK(policy_for("paste").files == FileAccess::None);
    CHECK(policy_for("paste").copy_buffer == CopyBufferAccess::Read);
    CHECK(policy_for("load cell example").workspace == WorkspaceAccess::Read);
    CHECK(policy_for("save cell example").project == ProjectOperation::Read);
    CHECK(policy_for("project new").project == ProjectOperation::ReplaceHistory);
    CHECK(policy_for("project new").history == HistoryPolicy::None);
    CHECK(policy_for("project open example").project ==
          ProjectOperation::ReplaceHistory);
    CHECK(policy_for("project save").project == ProjectOperation::Read);
    CHECK(policy_for("project save as example.xenproj").project ==
          ProjectOperation::Read);
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
    CHECK(policy_for("composition cell unassign 0 0").history == HistoryPolicy::Commit);
    CHECK(policy_for("sequence clear").history == HistoryPolicy::Commit);
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

TEST_CASE("Catalog metadata exposes paths, arguments, and descriptions",
          "[core][command][catalog]")
{
    auto const &metadata = default_command_catalog().metadata();
    REQUIRE_FALSE(metadata.empty());

    auto const set_pitch = std::find_if(
        metadata.begin(), metadata.end(), [](CatalogCommandMetadata const &entry) {
            return entry.path == std::vector<std::string>{"set", "pitch"};
        });
    REQUIRE(set_pitch != metadata.end());
    REQUIRE(set_pitch->arguments.size() == 1);
    CHECK(set_pitch->arguments[0].kind == "pitch");

    auto const set_velocity = std::find_if(
        metadata.begin(), metadata.end(), [](CatalogCommandMetadata const &entry) {
            return entry.path == std::vector<std::string>{"set", "velocity"};
        });
    REQUIRE(set_velocity != metadata.end());
    REQUIRE(set_velocity->arguments.size() == 1);
    CHECK(set_velocity->arguments[0].kind == "velocity");
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

    auto const translate_direction = std::find_if(
        metadata.begin(), metadata.end(), [](CatalogCommandMetadata const &entry) {
            return entry.path == std::vector<std::string>{"set", "translateDirection"};
        });
    REQUIRE(translate_direction != metadata.end());
    REQUIRE(translate_direction->arguments.size() == 1);
    auto const &translate_argument = translate_direction->arguments[0];
    CHECK(translate_argument.kind == "translate_direction");
    CHECK(translate_argument.display_name == "direction");
    CHECK(translate_argument.required);
    REQUIRE(translate_argument.constraints.size() == 1);
    CHECK(translate_argument.constraints[0].kind == "one_of");
    CHECK(translate_argument.constraints[0].values ==
          std::vector<std::string>{"up", "down"});

    auto const entire_scale = std::find_if(
        metadata.begin(), metadata.end(), [](CatalogCommandMetadata const &entry) {
            return entry.path == std::vector<std::string>{"shift", "entireScale"};
        });
    REQUIRE(entire_scale != metadata.end());
    CHECK(entire_scale->accepts_pattern_prefix == false);
    REQUIRE(entire_scale->arguments.size() == 1);
    REQUIRE(entire_scale->arguments[0].constraints.size() == 1);
    CHECK(entire_scale->arguments[0].constraints[0].kind == "one_of");
    CHECK(entire_scale->arguments[0].constraints[0].values ==
          std::vector<std::string>{"-1", "1"});

    auto const load_keys = std::find_if(
        metadata.begin(), metadata.end(), [](CatalogCommandMetadata const &entry) {
            return entry.path == std::vector<std::string>{"load", "keys"};
        });
    CHECK(load_keys == metadata.end());

    auto const load_keys_result =
        bind_invocation(parse_command_chain("load keys").front());
    REQUIRE_FALSE(load_keys_result.has_value());
    CHECK(load_keys_result.error().kind == CatalogBindErrorKind::UnknownCommand);
}

TEST_CASE("Catalog bridge payload serializes schema version and keywords",
          "[core][command][catalog][bridge]")
{
    auto const payload =
        bridge::make_catalog_payload(default_command_catalog().metadata());
    CHECK(payload.at("schema_version") == bridge::catalog_schema_version);

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
    CHECK(set_velocity->at("arguments")[0].at("kind") == "velocity");
    CHECK(set_velocity->at("arguments")[0]
              .at("constraints")[0]
              .at("minimum")
              .get<double>() == 0.0);
    CHECK(set_velocity->at("arguments")[0]
              .at("constraints")[0]
              .at("maximum")
              .get<double>() == 1.0);

    auto const translate_direction = std::find_if(
        commands.begin(), commands.end(), [](nlohmann::json const &command) {
            return command.at("path") ==
                   std::vector<std::string>{"set", "translateDirection"};
        });
    REQUIRE(translate_direction != commands.end());
    auto const &translate_argument = translate_direction->at("arguments")[0];
    CHECK(translate_argument.at("kind") == "translate_direction");
    REQUIRE(translate_argument.at("constraints").size() == 1);
    CHECK(translate_argument.at("constraints")[0].at("kind") == "one_of");
    CHECK(translate_argument.at("constraints")[0].at("values") ==
          std::vector<std::string>{"up", "down"});

    auto const set_midi_cc = std::find_if(
        commands.begin(), commands.end(), [](nlohmann::json const &command) {
            return command.at("path") == std::vector<std::string>{"set", "midiCC"};
        });
    REQUIRE(set_midi_cc != commands.end());
    CHECK(set_midi_cc->at("accepts_pattern_prefix"));
    CHECK(set_midi_cc->at("target_requirement") == "cell_or_element");
    REQUIRE(set_midi_cc->at("arguments").size() == 2);
    CHECK(set_midi_cc->at("arguments")[0].at("kind") == "midi_controller");
    CHECK(set_midi_cc->at("arguments")[0].at("required"));
    CHECK(set_midi_cc->at("arguments")[0].at("constraints")[0].at("maximum") == 127);
    CHECK(set_midi_cc->at("arguments")[1].at("kind") == "normalized_value");

    auto const set_label = std::find_if(
        commands.begin(), commands.end(), [](nlohmann::json const &command) {
            return command.at("path") == std::vector<std::string>{"set", "midiCCLabel"};
        });
    REQUIRE(set_label != commands.end());
    CHECK_FALSE(set_label->at("accepts_pattern_prefix"));
    CHECK(set_label->at("target_requirement") == "none");
    CHECK(set_label->at("arguments")[1].at("constraints")[0].at("kind") ==
          "byte_length");
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
