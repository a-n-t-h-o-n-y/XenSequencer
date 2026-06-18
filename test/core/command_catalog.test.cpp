#include <algorithm>
#include <string>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <xen/command.hpp>
#include <xen/command_action.hpp>
#include <xen/command_catalog.hpp>
#include <xen/command_dsl.hpp>

using namespace xen;

TEST_CASE("Catalog binds command chain to typed actions", "[core][command][catalog]")
{
    auto const chain = parse_command_chain("version; again; set key 7; move left 3");
    auto const result = bind_chain(chain);

    REQUIRE(std::holds_alternative<std::vector<BoundCommand>>(result));
    auto const &bound = std::get<std::vector<BoundCommand>>(result);
    REQUIRE(bound.size() == 4);

    REQUIRE(bound[0].action.has_value());
    REQUIRE(bound[2].action.has_value());
    REQUIRE(bound[3].action.has_value());
    CHECK(std::holds_alternative<VersionAction>(*bound[0].action));
    CHECK(bound[1].control == BoundCommandControl::ReplayPrevious);
    CHECK_FALSE(bound[1].action.has_value());
    CHECK(std::holds_alternative<SetKeyAction>(*bound[2].action));
    CHECK(std::holds_alternative<MoveSelectionAction>(*bound[3].action));

    CHECK(bound[0].canonical == "version");
    CHECK(std::get<SetKeyAction>(*bound[2].action).key == 7);
    CHECK(std::get<MoveSelectionAction>(*bound[3].action).direction ==
          MoveDirection::Left);
    CHECK(std::get<MoveSelectionAction>(*bound[3].action).amount == 3);
}

TEST_CASE("Catalog binder applies defaults for commands", "[core][command][catalog]")
{
    auto const set_key_invocation = parse_command_chain("set key")[0];
    auto const set_key_result = bind_invocation(set_key_invocation);
    REQUIRE(std::holds_alternative<BoundCommand>(set_key_result));
    auto const &set_key_command = std::get<BoundCommand>(set_key_result);
    REQUIRE(set_key_command.action.has_value());
    CHECK(std::get<SetKeyAction>(*set_key_command.action).key == 0);

    auto const move_invocation = parse_command_chain("move right")[0];
    auto const move_result = bind_invocation(move_invocation);
    REQUIRE(std::holds_alternative<BoundCommand>(move_result));
    auto const &move_command = std::get<BoundCommand>(move_result);
    REQUIRE(move_command.action.has_value());
    auto const move_action = std::get<MoveSelectionAction>(*move_command.action);
    CHECK(move_action.direction == MoveDirection::Right);
    CHECK(move_action.amount == 1);
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

    auto const missing_invocation = parse_command_chain("load measure")[0];
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
    CHECK(std::holds_alternative<BoundCommand>(accepted));
}

TEST_CASE("Catalog supports runtime typed command registration",
          "[core][command][catalog]")
{
    auto catalog = CommandCatalog{};
    catalog.add(command_dsl::command(
        {"inspect", "value"}, false, "Inspect a typed value.",
        std::make_tuple(command_dsl::constrained(
            command_dsl::optional_arg<int>("value", 7),
            [](int value) { return value >= 0 && value <= 100; },
            "value must be in range [0, 100]")),
        [](PluginState &, ExecutionContext context, CommandInvocation const &,
           int value) {
            static_cast<void>(context);
            return std::pair{MessageLevel::Info, std::to_string(value)};
        }));

    auto const result =
        catalog.bind_invocation(parse_command_chain("inspect value 42")[0]);
    REQUIRE(std::holds_alternative<BoundCommand>(result));
    auto const &command = std::get<BoundCommand>(result);
    REQUIRE(command.execute);
    CHECK_FALSE(command.action.has_value());

    auto state = PluginState{
        .timeline =
            XenTimeline{
                TimelineState{
                    .sequencer = {},
                    .aux = {},
                },
            },
    };
    auto const execution = command.execute(state, ExecutionContext{});
    CHECK(execution.status.first == MessageLevel::Info);
    CHECK(execution.status.second == "42");

    REQUIRE(catalog.metadata().size() == 1);
    CHECK(catalog.metadata()[0].arguments[0].type == "Int");

    auto const invalid =
        catalog.bind_invocation(parse_command_chain("inspect value 101")[0]);
    REQUIRE(std::holds_alternative<CatalogBindError>(invalid));
    CHECK(std::get<CatalogBindError>(invalid).kind ==
          CatalogBindErrorKind::InvalidArgument);
}

TEST_CASE("Catalog binds non-bootstrap commands without adapter gaps",
          "[core][command][catalog]")
{
    auto const chain =
        parse_command_chain("set baseFrequency 333; load scales; save measure foo");
    auto const result = bind_chain(chain);

    REQUIRE(std::holds_alternative<std::vector<BoundCommand>>(result));
    auto const &bound = std::get<std::vector<BoundCommand>>(result);
    REQUIRE(bound.size() == 3);
    REQUIRE(bound[0].action.has_value());
    REQUIRE(bound[1].action.has_value());
    REQUIRE(bound[2].action.has_value());
    CHECK(std::holds_alternative<SetBaseFrequencyAction>(*bound[0].action));
    CHECK(std::holds_alternative<LoadScalesAction>(*bound[1].action));
    CHECK(std::holds_alternative<SaveMeasureAction>(*bound[2].action));
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

TEST_CASE("Catalog metadata exposes path, args, and docs", "[core][command][catalog]")
{
    auto const &metadata = command_metadata();
    REQUIRE_FALSE(metadata.empty());

    auto const set_key = std::find_if(
        metadata.begin(), metadata.end(), [](CatalogCommandMetadata const &entry) {
            return entry.path == std::vector<std::string>{"set", "key"};
        });
    REQUIRE(set_key != metadata.end());
    REQUIRE(set_key->arguments.size() == 1);
    CHECK(set_key->arguments[0].type == "Int");
    CHECK(set_key->arguments[0].name == "key");
    REQUIRE(set_key->arguments[0].default_value.has_value());
    CHECK(*set_key->arguments[0].default_value == "0");
    CHECK_FALSE(set_key->description.empty());

    auto const entire_scale = std::find_if(
        metadata.begin(), metadata.end(), [](CatalogCommandMetadata const &entry) {
            return entry.path == std::vector<std::string>{"shift", "entireScale"};
        });
    REQUIRE(entire_scale != metadata.end());
    CHECK(entire_scale->accepts_pattern_prefix == false);
}

TEST_CASE("Catalog completion is driven from catalog metadata",
          "[core][command][catalog]")
{
    auto const catalog = create_command_catalog();

    CHECK(catalog.complete_text("") == "");
    CHECK(catalog.complete_text("   ") == "");
    CHECK(catalog.complete_text("set ba") == "seFrequency");
    CHECK(catalog.complete_id("set ba") == "seFrequency");

    CHECK(catalog.complete_text("set baseFrequency") == "[Float: freq=440]");
    CHECK(catalog.complete_text("set baseFrequency ") == "[Float: freq=440]");
    CHECK(catalog.complete_id("set baseFrequency ") == "");
}

TEST_CASE("Catalog structured completion returns all matching command tokens",
          "[core][command][catalog]")
{
    auto const catalog = create_command_catalog();
    auto const result = catalog.complete("set ");

    auto displays = std::vector<std::string>{};
    for (auto const &candidate : result.candidates)
    {
        displays.push_back(candidate.display);
    }

    CHECK(std::find(displays.begin(), displays.end(), "pitch") != displays.end());
    CHECK(std::find(displays.begin(), displays.end(), "velocity") != displays.end());
    CHECK(std::find(displays.begin(), displays.end(), "key") != displays.end());
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
    CHECK(mirror_doc->signature.arguments[0] == "[Int: centerPitch=0]");
}
