#include <catch2/catch_test_macros.hpp>

#include <xen/command.hpp>
#include <xen/engine_state.hpp>

namespace
{
auto make_plugin_state() -> xen::PluginState
{
    return xen::PluginState{
        .timeline = xen::XenTimeline{xen::TrackedState{
            .sequencer = xen::SequencerState{},
            .aux = xen::AuxState{},
        }},
    };
}
} // namespace

TEST_CASE("Command executes typed arguments", "[unit][command]")
{
    auto ps = make_plugin_state();

    auto command = xen::cmd(xen::signature("add", xen::arg<int>("a"), xen::arg<int>("b")),
                            "add numbers",
                            [](xen::PluginState &, int a, int b) {
                                return std::pair{xen::MessageLevel::Debug,
                                                 std::to_string(a + b)};
                            });

    auto const result = command->execute(ps, xen::SplitInput{
                                                 .pattern = {0, {1}},
                                                 .words = {"2", "5"},
                                             });

    REQUIRE(result.first == xen::MessageLevel::Debug);
    REQUIRE(result.second == "7");
}

TEST_CASE("Patterned signature receives split-input pattern", "[unit][command]")
{
    auto ps = make_plugin_state();

    auto command = xen::cmd(
        xen::signature("show-pattern", xen::arg<sequence::Pattern>("pattern"),
                       xen::arg<int>("value")),
        "uses the parsed command prefix pattern",
        [](xen::PluginState &, sequence::Pattern const &pattern, int value) {
            auto const pattern_ok = pattern == sequence::Pattern{3, {2, 1}};
            return std::pair{xen::MessageLevel::Debug,
                             std::string{pattern_ok ? "ok" : "bad"} +
                                 ":" + std::to_string(value)};
        });

    auto input = xen::split_input("+3 2 1 show-pattern 9");
    REQUIRE_FALSE(input.words.empty());
    input.words.erase(std::begin(input.words)); // Drop command ID for direct command call.

    auto const result = command->execute(ps, std::move(input));

    REQUIRE(result.first == xen::MessageLevel::Debug);
    REQUIRE(result.second == "ok:9");
}

TEST_CASE("Command group dispatches nested commands and reports missing command",
          "[unit][command]")
{
    auto ps = make_plugin_state();

    auto root = xen::cmd_group("set");
    root->add(xen::cmd(xen::signature("add", xen::arg<int>("a"), xen::arg<int>("b")),
                       "add",
                       [](xen::PluginState &, int a, int b) {
                           return std::pair{xen::MessageLevel::Info,
                                            std::to_string(a + b)};
                       }));

    auto nested = xen::cmd_group("math");
    nested->add(xen::cmd(xen::signature("sub", xen::arg<int>("a"), xen::arg<int>("b")),
                         "subtract",
                         [](xen::PluginState &, int a, int b) {
                             return std::pair{xen::MessageLevel::Info,
                                              std::to_string(a - b)};
                         }));
    root->add(std::move(nested));

    auto const add_result = root->execute(ps, xen::SplitInput{
                                                  .pattern = {0, {1}},
                                                  .words = {"add", "4", "8"},
                                              });
    REQUIRE(add_result.second == "12");

    auto const sub_result = root->execute(ps, xen::SplitInput{
                                                  .pattern = {0, {1}},
                                                  .words = {"math", "sub", "9", "3"},
                                              });
    REQUIRE(sub_result.second == "6");

    auto const missing = root->execute(ps, xen::SplitInput{
                                               .pattern = {0, {1}},
                                               .words = {"unknown"},
                                           });
    REQUIRE(missing.first == xen::MessageLevel::Error);
    REQUIRE(missing.second == "Command not found: unknown");
}
