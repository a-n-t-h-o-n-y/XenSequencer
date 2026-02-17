#include <stdexcept>
#include <vector>
#include <variant>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <xen/actions.hpp>
#include <xen/command_action.hpp>
#include <xen/command.hpp>
#include <xen/message_level.hpp>
#include <xen/selection.hpp>
#include <xen/state.hpp>
#include <xen/utility.hpp>
#include <xen/xen_command_tree.hpp>
#include <sequence/sequence.hpp>
#include <sequence/tuning.hpp>

using namespace xen;

namespace
{

auto make_plugin_state() -> PluginState
{
    return PluginState{
        .timeline = XenTimeline{
            TimelineState{
                .sequencer = {},
                .aux = {},
            },
        },
    };
}

auto check_context_equal(ExecutionContext const &lhs, ExecutionContext const &rhs)
    -> void
{
    CHECK(lhs.selected == rhs.selected);
    CHECK(lhs.input_mode == rhs.input_mode);
    CHECK(lhs.arp_state.selected == rhs.arp_state.selected);
    CHECK(lhs.arp_state.previous_commit_id == rhs.arp_state.previous_commit_id);
    CHECK(lhs.arp_state.previous_chord_name == rhs.arp_state.previous_chord_name);
    CHECK(lhs.arp_state.previous_inversion == rhs.arp_state.previous_inversion);
}

auto execute_legacy_command(PluginState &ps, XenCommandTree const &tree,
                            ExecutionContext context,
                            std::string const &command)
    -> CommandActionResult
{
    auto const invocations = parse_command_chain(command);
    if (invocations.size() != 1)
    {
        throw std::runtime_error("Legacy helper expects a single command.");
    }

    auto staged_state = ps.timeline.get_state();
    staged_state.aux = context;
    ps.timeline.stage(std::move(staged_state));

    auto const engine_before = ps.timeline.get_state().sequencer;
    ps.commit_intent = CommitIntent::Auto;

    auto const status = tree.execute(ps, invocations.front().input);
    auto const state_after = ps.timeline.get_state();
    return CommandActionResult{
        .status = status,
        .context = state_after.aux,
        .engine_mutated = state_after.sequencer != engine_before,
        .commit_intent = ps.commit_intent,
    };
}

} // namespace

TEST_CASE("Command invocations are adapted to typed actions in order",
          "[core][command][action]")
{
    auto const invocations = parse_command_chain("set key 9; again; version");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 3);
    CHECK(std::holds_alternative<SetKeyAction>(actions[0]));
    CHECK(is_again_action(actions[1]));
    CHECK_FALSE(is_again_action(actions[2]));
    CHECK(std::holds_alternative<AgainAction>(actions[1]));
    CHECK(std::holds_alternative<VersionAction>(actions[2]));
}

TEST_CASE("Unknown command invocation fails typed-action conversion",
          "[core][command][action]")
{
    auto const invocations = parse_command_chain("notACommand 123");
    REQUIRE(invocations.size() == 1);

    CHECK_FALSE(try_to_command_action(invocations.front()).has_value());
    CHECK_THROWS_AS(to_command_actions(invocations), utility::ErrorNoMatch);
}

TEST_CASE("Official command catalog maps to typed actions",
          "[core][command][action]")
{
    auto const samples = std::vector<std::string>{
        "welcome",
        "version",
        "commit",
        "reset",
        "undo",
        "redo",
        "copy",
        "cut",
        "paste",
        "duplicate",
        "inputMode pitch",
        "focus statusBar",
        "show chordsPane",
        "load sequenceBank fixture",
        "load tuning fixture",
        "load keys",
        "load scales",
        "load chords",
        "save sequenceBank fixture",
        "libraryDirectory",
        "move left 1",
        "move right 1",
        "move up 1",
        "move down 1",
        "note",
        "rest",
        "delete",
        "split 2",
        "lift",
        "+0 flip",
        "+0 fill note",
        "+0 fill rest",
        "select sequence 0",
        "set pitch 0",
        "set octave 0",
        "set velocity 0.5",
        "set delay 0.1",
        "set gate 0.5",
        "set sequence name \"lead\"",
        "set sequence timeSignature 4/4",
        "set baseFrequency 440",
        "set theme default",
        "set scale chromatic",
        "set mode 1",
        "set translateDirection up",
        "set key 0",
        "set weight 1",
        "+0 set weights 0.5",
        "double sequence timeSignature",
        "halve sequence timeSignature",
        "+0 shift pitch 1",
        "+0 shift octave 1",
        "+0 shift velocity 0.1",
        "+0 shift delay 0.1",
        "+0 shift gate 0.1",
        "shift selectedSequence 1",
        "shift scale",
        "shift scaleMode",
        "shift translateDirection",
        "shift entireScale 1",
        "+0 randomize pitch",
        "+0 randomize velocity",
        "+0 randomize delay",
        "+0 randomize gate",
        "+0 stretch 2",
        "+1 compress",
        "shuffle",
        "rotate 1",
        "reverse",
        "+0 mirror 0",
        "+0 quantize",
        "swing 0.1",
        "+0 step 1 0.1",
        "drums",
        "+0 arp major 1",
    };

    for (auto const &command : samples)
    {
        auto const actions = to_command_actions(parse_command_chain(command));
        REQUIRE(actions.size() == 1);
        INFO("command=" << command);
        CHECK_FALSE(is_again_action(actions.front()));
    }
}

TEST_CASE("Command adapter maps migrated commands to typed action variants",
          "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "move down 2; set key 11; set sequence name \"pad\" 4; "
        "select sequence 3; inputMode gate; note 2 0.7 0.1 0.9; rest; "
        "set baseFrequency 880; commit; undo; redo; "
        "set scale major; set mode 2; set translateDirection down; version");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 15);
    CHECK(std::holds_alternative<MoveSelectionAction>(actions[0]));
    CHECK(std::holds_alternative<SetKeyAction>(actions[1]));
    CHECK(std::holds_alternative<SetSequenceNameAction>(actions[2]));
    CHECK(std::holds_alternative<SelectSequenceAction>(actions[3]));
    CHECK(std::holds_alternative<SetInputModeAction>(actions[4]));
    CHECK(std::holds_alternative<CreateNoteAction>(actions[5]));
    CHECK(std::holds_alternative<CreateRestAction>(actions[6]));
    CHECK(std::holds_alternative<SetBaseFrequencyAction>(actions[7]));
    CHECK(std::holds_alternative<CommitAction>(actions[8]));
    CHECK(std::holds_alternative<UndoAction>(actions[9]));
    CHECK(std::holds_alternative<RedoAction>(actions[10]));
    CHECK(std::holds_alternative<SetScaleAction>(actions[11]));
    CHECK(std::holds_alternative<SetScaleModeAction>(actions[12]));
    CHECK(std::holds_alternative<SetTranslateDirectionAction>(actions[13]));
    CHECK(std::holds_alternative<VersionAction>(actions[14]));

    auto const move = std::get<MoveSelectionAction>(actions[0]);
    CHECK(move.direction == MoveDirection::Down);
    CHECK(move.amount == 2);

    auto const set_key = std::get<SetKeyAction>(actions[1]);
    CHECK(set_key.key == 11);

    auto const set_name = std::get<SetSequenceNameAction>(actions[2]);
    CHECK(set_name.name == "pad");
    CHECK(set_name.index == 4);

    auto const select = std::get<SelectSequenceAction>(actions[3]);
    CHECK(select.index == 3);

    auto const input_mode = std::get<SetInputModeAction>(actions[4]);
    CHECK(input_mode.mode == InputMode::Gate);

    auto const note = std::get<CreateNoteAction>(actions[5]);
    CHECK(note.pitch == 2);
    CHECK(note.velocity == Catch::Approx(0.7f));
    CHECK(note.delay == Catch::Approx(0.1f));
    CHECK(note.gate == Catch::Approx(0.9f));

    auto const base = std::get<SetBaseFrequencyAction>(actions[7]);
    CHECK(base.freq == Catch::Approx(880.f));

    auto const mode = std::get<SetScaleModeAction>(actions[12]);
    CHECK(mode.mode_index == 2);

    auto const translate = std::get<SetTranslateDirectionAction>(actions[13]);
    CHECK(translate.direction == "down");
}

TEST_CASE("Command adapter preserves migrated command defaults",
          "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "set key; set sequence name \"lead\"; note; set baseFrequency");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 4);
    REQUIRE(std::holds_alternative<SetKeyAction>(actions[0]));
    REQUIRE(std::holds_alternative<SetSequenceNameAction>(actions[1]));
    REQUIRE(std::holds_alternative<CreateNoteAction>(actions[2]));
    REQUIRE(std::holds_alternative<SetBaseFrequencyAction>(actions[3]));

    CHECK(std::get<SetKeyAction>(actions[0]).key == 0);
    CHECK(std::get<SetSequenceNameAction>(actions[1]).index == -1);
    CHECK(std::get<CreateNoteAction>(actions[2]).pitch == 0);
    CHECK(std::get<CreateNoteAction>(actions[2]).velocity ==
          Catch::Approx(100.f / 127.f));
    CHECK(std::get<CreateNoteAction>(actions[2]).delay == Catch::Approx(0.f));
    CHECK(std::get<CreateNoteAction>(actions[2]).gate == Catch::Approx(1.f));
    CHECK(std::get<SetBaseFrequencyAction>(actions[3]).freq == Catch::Approx(440.f));
}

TEST_CASE("Command adapter maps shift commands to typed action variants",
          "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "shift selectedSequence -2; shift scale; shift scaleMode -1; "
        "shift translateDirection; shift entireScale -1");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 5);
    REQUIRE(std::holds_alternative<ShiftSelectedSequenceAction>(actions[0]));
    REQUIRE(std::holds_alternative<ShiftScaleAction>(actions[1]));
    REQUIRE(std::holds_alternative<ShiftScaleModeAction>(actions[2]));
    REQUIRE(std::holds_alternative<ShiftTranslateDirectionAction>(actions[3]));
    REQUIRE(std::holds_alternative<ShiftEntireScaleAction>(actions[4]));

    CHECK(std::get<ShiftSelectedSequenceAction>(actions[0]).amount == -2);
    CHECK(std::get<ShiftScaleAction>(actions[1]).amount == 1);
    CHECK(std::get<ShiftScaleModeAction>(actions[2]).amount == -1);
    CHECK(std::get<ShiftEntireScaleAction>(actions[4]).direction == -1);
}

TEST_CASE("Command adapter preserves shift defaults", "[core][command][action]")
{
    auto const invocations =
        parse_command_chain("shift scale; shift scaleMode; shift entireScale");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 3);
    CHECK(std::get<ShiftScaleAction>(actions[0]).amount == 1);
    CHECK(std::get<ShiftScaleModeAction>(actions[1]).amount == 1);
    CHECK(std::get<ShiftEntireScaleAction>(actions[2]).direction == 1);
}

TEST_CASE("Typed action execution applies explicit context and reports updated context",
          "[core][command][action]")
{
    auto ps = make_plugin_state();

    auto context = ExecutionContext{};
    context.selected.measure = 3;

    auto const actions =
        to_command_actions(parse_command_chain("set sequence name \"lead\""));
    REQUIRE(actions.size() == 1);

    auto const result = execute_command_action(ps, context, actions[0]);
    CHECK(result.status.first == MessageLevel::Info);
    CHECK(result.context.selected.measure == 3);
    CHECK(ps.timeline.get_state().sequencer.sequence_names[3] == "lead");
}

TEST_CASE("Typed action execution exposes commit intent and mutation metadata",
          "[core][command][action]")
{
    auto ps = make_plugin_state();
    auto context = ExecutionContext{ps.timeline.get_state().aux};

    auto const set_key_actions = to_command_actions(parse_command_chain("set key 7"));
    REQUIRE(set_key_actions.size() == 1);
    auto const set_key_result =
        execute_command_action(ps, context, set_key_actions[0]);
    CHECK(set_key_result.status.first == MessageLevel::Info);
    CHECK(set_key_result.engine_mutated);
    CHECK(set_key_result.commit_intent == CommitIntent::Auto);

    context = set_key_result.context;

    auto const commit_actions = to_command_actions(parse_command_chain("commit"));
    REQUIRE(commit_actions.size() == 1);
    auto const commit_result =
        execute_command_action(ps, context, commit_actions[0]);
    CHECK(commit_result.status.first == MessageLevel::Debug);
    CHECK_FALSE(commit_result.engine_mutated);
    CHECK(commit_result.commit_intent == CommitIntent::Force);

    auto const split_actions = to_command_actions(parse_command_chain("split 2"));
    REQUIRE(split_actions.size() == 1);
    auto const split_result =
        execute_command_action(ps, context, split_actions[0]);
    REQUIRE(split_result.status.first == MessageLevel::Info);

    auto const defer_actions = to_command_actions(parse_command_chain("set weights 0.5"));
    REQUIRE(defer_actions.size() == 1);
    auto const defer_result =
        execute_command_action(ps, split_result.context, defer_actions[0]);
    CHECK(defer_result.status.first == MessageLevel::Info);
    CHECK(defer_result.commit_intent == CommitIntent::Defer);
}

TEST_CASE("Typed move action updates context without mutating engine",
          "[core][command][action]")
{
    auto ps = make_plugin_state();
    auto context = ExecutionContext{ps.timeline.get_state().aux};

    auto const action = CommandAction{
        MoveSelectionAction{.direction = MoveDirection::Right, .amount = 1}};
    auto const result = execute_command_action(ps, context, action);

    CHECK(result.status.first == MessageLevel::Debug);
    CHECK_FALSE(result.engine_mutated);
    CHECK(result.context.selected == action::move_right(ps.timeline.get_state().sequencer,
                                                        context, 1)
                                 .selected);
}

TEST_CASE("Typed select sequence and input mode update context-only state",
          "[core][command][action]")
{
    auto ps = make_plugin_state();

    auto context = ExecutionContext{};
    context.selected.measure = 5;
    context.selected.cell = {0};
    context.input_mode = InputMode::Pitch;

    auto select_result = execute_command_action(
        ps, context, CommandAction{SelectSequenceAction{.index = 5}});
    CHECK(select_result.status.first == MessageLevel::Debug);
    CHECK(select_result.status.second == "Already Selected");
    CHECK_FALSE(select_result.engine_mutated);
    CHECK(select_result.context.selected.measure == 5);
    CHECK(select_result.context.selected.cell == std::vector<std::size_t>{0});

    select_result = execute_command_action(
        ps, context, CommandAction{SelectSequenceAction{.index = 3}});
    CHECK(select_result.status.first == MessageLevel::Debug);
    CHECK(select_result.status.second == "Sequence 3 Selected");
    CHECK_FALSE(select_result.engine_mutated);
    CHECK(select_result.context.selected.measure == 3);
    CHECK(select_result.context.selected.cell.empty());

    auto const mode_result = execute_command_action(
        ps, select_result.context, CommandAction{SetInputModeAction{.mode = InputMode::Gate}});
    CHECK(mode_result.status.first == MessageLevel::Info);
    CHECK(mode_result.status.second == "Input Mode Set to 'gate'");
    CHECK_FALSE(mode_result.engine_mutated);
    CHECK(mode_result.context.input_mode == InputMode::Gate);
}

TEST_CASE("Typed note/rest actions match invoke-command behavior",
          "[core][command][action]")
{
    auto tree = create_command_tree();
    auto typed_ps = make_plugin_state();
    auto invoke_ps = make_plugin_state();

    auto seed = [](PluginState &ps) {
        auto state = ps.timeline.get_state();
        auto &selected = get_selected_cell(state.sequencer.sequence_bank, state.aux.selected);
        selected.weight = 0.37f;
        ps.timeline.stage(std::move(state));
    };
    seed(typed_ps);
    seed(invoke_ps);

    auto const context = ExecutionContext{typed_ps.timeline.get_state().aux};

    auto const typed_note = execute_command_action(
        typed_ps, context, CommandAction{CreateNoteAction{
            .pitch = 12, .velocity = 0.5f, .delay = 0.25f, .gate = 0.75f}});
    auto const invoke_note =
        execute_legacy_command(invoke_ps, tree, context, "note 12 0.5 0.25 0.75");

    CHECK(typed_note.status == invoke_note.status);
    CHECK(typed_note.engine_mutated == invoke_note.engine_mutated);
    check_context_equal(typed_note.context, invoke_note.context);
    CHECK(typed_ps.timeline.get_state().sequencer ==
          invoke_ps.timeline.get_state().sequencer);

    auto const typed_rest = execute_command_action(
        typed_ps, typed_note.context, CommandAction{CreateRestAction{}});
    auto const invoke_rest =
        execute_legacy_command(invoke_ps, tree, invoke_note.context, "rest");

    CHECK(typed_rest.status == invoke_rest.status);
    CHECK(typed_rest.engine_mutated == invoke_rest.engine_mutated);
    check_context_equal(typed_rest.context, invoke_rest.context);
    CHECK(typed_ps.timeline.get_state().sequencer ==
          invoke_ps.timeline.get_state().sequencer);
}

TEST_CASE("Typed baseFrequency action clamps value", "[core][command][action]")
{
    auto ps = make_plugin_state();
    auto context = ExecutionContext{ps.timeline.get_state().aux};

    auto result = execute_command_action(
        ps, context, CommandAction{SetBaseFrequencyAction{.freq = -10.f}});
    CHECK(result.status.first == MessageLevel::Info);
    CHECK(result.status.second == "Base Frequency Set");
    CHECK(result.engine_mutated);
    CHECK(ps.timeline.get_state().sequencer.base_frequency == Catch::Approx(20.f));

    result = execute_command_action(
        ps, context, CommandAction{SetBaseFrequencyAction{.freq = 50'000.f}});
    CHECK(result.status.first == MessageLevel::Info);
    CHECK(result.status.second == "Base Frequency Set");
    CHECK(result.engine_mutated);
    CHECK(ps.timeline.get_state().sequencer.base_frequency ==
          Catch::Approx(20'000.f));
}

TEST_CASE("Typed commit undo redo actions match invoke-command behavior",
          "[core][command][action]")
{
    auto tree = create_command_tree();
    auto typed_ps = make_plugin_state();
    auto invoke_ps = make_plugin_state();

    auto seed_history = [](PluginState &ps) {
        auto state = ps.timeline.get_state();
        state.sequencer.key = 1;
        ps.timeline.stage(std::move(state));
        ps.timeline.commit();

        state = ps.timeline.get_state();
        state.sequencer.key = 2;
        ps.timeline.stage(std::move(state));
        ps.timeline.commit();
    };
    seed_history(typed_ps);
    seed_history(invoke_ps);

    auto context = ExecutionContext{};
    context.selected.measure = 7;
    context.input_mode = InputMode::Gate;

    auto const typed_undo_action = to_command_actions(parse_command_chain("undo"))[0];

    auto typed_undo =
        execute_command_action(typed_ps, context, typed_undo_action);
    auto invoke_undo =
        execute_legacy_command(invoke_ps, tree, context, "undo");
    CHECK(typed_undo.status == invoke_undo.status);
    CHECK(typed_undo.engine_mutated == invoke_undo.engine_mutated);
    check_context_equal(typed_undo.context, invoke_undo.context);
    CHECK(typed_ps.timeline.get_state().sequencer ==
          invoke_ps.timeline.get_state().sequencer);

    auto const typed_redo_action = to_command_actions(parse_command_chain("redo"))[0];
    auto typed_redo = execute_command_action(typed_ps, typed_undo.context,
                                             typed_redo_action);
    auto invoke_redo =
        execute_legacy_command(invoke_ps, tree, invoke_undo.context, "redo");
    CHECK(typed_redo.status == invoke_redo.status);
    CHECK(typed_redo.engine_mutated == invoke_redo.engine_mutated);
    check_context_equal(typed_redo.context, invoke_redo.context);
    CHECK(typed_ps.timeline.get_state().sequencer ==
          invoke_ps.timeline.get_state().sequencer);

    auto commit_typed_ps = make_plugin_state();
    auto commit_invoke_ps = make_plugin_state();
    auto const typed_commit_action = to_command_actions(parse_command_chain("commit"))[0];
    auto const typed_commit =
        execute_command_action(commit_typed_ps, ExecutionContext{}, typed_commit_action);
    auto const invoke_commit =
        execute_legacy_command(commit_invoke_ps, tree, ExecutionContext{}, "commit");
    CHECK(typed_commit.status == invoke_commit.status);
    CHECK(typed_commit.commit_intent == CommitIntent::Force);
    CHECK(invoke_commit.commit_intent == CommitIntent::Force);
    CHECK_FALSE(typed_commit.engine_mutated);
    CHECK_FALSE(invoke_commit.engine_mutated);
}

TEST_CASE("Typed scale actions match invoke-command behavior",
          "[core][command][action]")
{
    auto tree = create_command_tree();
    auto typed_ps = make_plugin_state();
    auto invoke_ps = make_plugin_state();
    auto const scale = Scale{
        .name = "major",
        .tuning_length = 12,
        .intervals = {2, 2, 1, 2, 2, 2, 1},
        .mode = 1,
    };
    typed_ps.library.scales = {scale};
    invoke_ps.library.scales = {scale};

    auto context = ExecutionContext{};

    auto run_parity = [&](std::string const &command) {
        auto const typed_action = to_command_actions(parse_command_chain(command))[0];

        auto const typed_result =
            execute_command_action(typed_ps, context, typed_action);
        auto const invoke_result =
            execute_legacy_command(invoke_ps, tree, context, command);

        CHECK(typed_result.status == invoke_result.status);
        CHECK(typed_result.engine_mutated == invoke_result.engine_mutated);
        check_context_equal(typed_result.context, invoke_result.context);
        CHECK(typed_ps.timeline.get_state().sequencer ==
              invoke_ps.timeline.get_state().sequencer);

        context = typed_result.context;
    };

    run_parity("set scale major");
    run_parity("set mode 2");
    run_parity("set translateDirection down");
    run_parity("set scale does_not_exist");
}

TEST_CASE("Typed shift actions match invoke-command behavior",
          "[core][command][action]")
{
    auto tree = create_command_tree();
    auto typed_ps = make_plugin_state();
    auto invoke_ps = make_plugin_state();
    auto const major = Scale{
        .name = "major",
        .tuning_length = 12,
        .intervals = {2, 2, 1, 2, 2, 2, 1},
        .mode = 1,
    };
    auto const minor = Scale{
        .name = "minor",
        .tuning_length = 12,
        .intervals = {2, 1, 2, 2, 1, 2, 2},
        .mode = 1,
    };
    typed_ps.library.scales = {major, minor};
    invoke_ps.library.scales = {major, minor};

    auto context = ExecutionContext{};
    context.selected.measure = 0;
    context.selected.cell = {0};

    auto run_parity = [&](std::string const &command) {
        auto const typed_action = to_command_actions(parse_command_chain(command))[0];

        auto const typed_result =
            execute_command_action(typed_ps, context, typed_action);
        auto const invoke_result =
            execute_legacy_command(invoke_ps, tree, context, command);

        CHECK(typed_result.status == invoke_result.status);
        CHECK(typed_result.engine_mutated == invoke_result.engine_mutated);
        check_context_equal(typed_result.context, invoke_result.context);
        CHECK(typed_ps.timeline.get_state().sequencer ==
              invoke_ps.timeline.get_state().sequencer);
        CHECK(typed_ps.library.scale_shift_index ==
              invoke_ps.library.scale_shift_index);

        context = typed_result.context;
    };

    run_parity("shift selectedSequence 3");
    run_parity("shift selectedSequence -1");
    run_parity("shift scale");
    run_parity("shift scale");
    run_parity("shift scaleMode -1");
    run_parity("shift translateDirection");
    run_parity("shift entireScale -1");
    run_parity("shift entireScale 0");
}

TEST_CASE(
    "Command adapter maps edit, set, shift, step, and drums commands to typed actions",
    "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "delete; split 3; lift; +1 flip; +2 fill note 4 0.8 0.1 0.9; +3 fill rest; "
        "set pitch 9; set octave 1; set velocity 0.75; set delay 0.2; set gate 0.8; "
        "set weight 0.6; +4 set weights 0.5; +5 shift pitch -2; +6 shift octave 2; "
        "+7 shift velocity -0.1; +8 shift delay 0.25; +9 shift gate -0.2; "
        "+10 step 3 0.4; drums 24 -2");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 20);
    REQUIRE(std::holds_alternative<DeleteSelectionAction>(actions[0]));
    REQUIRE(std::holds_alternative<SplitSelectionAction>(actions[1]));
    REQUIRE(std::holds_alternative<LiftSelectionAction>(actions[2]));
    REQUIRE(std::holds_alternative<FlipSelectionAction>(actions[3]));
    REQUIRE(std::holds_alternative<FillNoteAction>(actions[4]));
    REQUIRE(std::holds_alternative<FillRestAction>(actions[5]));
    REQUIRE(std::holds_alternative<SetPitchAction>(actions[6]));
    REQUIRE(std::holds_alternative<SetOctaveAction>(actions[7]));
    REQUIRE(std::holds_alternative<SetVelocityAction>(actions[8]));
    REQUIRE(std::holds_alternative<SetDelayAction>(actions[9]));
    REQUIRE(std::holds_alternative<SetGateAction>(actions[10]));
    REQUIRE(std::holds_alternative<SetWeightAction>(actions[11]));
    REQUIRE(std::holds_alternative<SetWeightsAction>(actions[12]));
    REQUIRE(std::holds_alternative<ShiftPitchAction>(actions[13]));
    REQUIRE(std::holds_alternative<ShiftOctaveAction>(actions[14]));
    REQUIRE(std::holds_alternative<ShiftVelocityAction>(actions[15]));
    REQUIRE(std::holds_alternative<ShiftDelayAction>(actions[16]));
    REQUIRE(std::holds_alternative<ShiftGateAction>(actions[17]));
    REQUIRE(std::holds_alternative<StepAction>(actions[18]));
    REQUIRE(std::holds_alternative<DrumsAction>(actions[19]));

    CHECK(std::get<SplitSelectionAction>(actions[1]).count == 3);
    CHECK(std::get<FlipSelectionAction>(actions[3]).pattern ==
          sequence::Pattern{1, {1}});
    CHECK(std::get<FillNoteAction>(actions[4]).pattern == sequence::Pattern{2, {1}});
    CHECK(std::get<FillNoteAction>(actions[4]).pitch == 4);
    CHECK(std::get<FillRestAction>(actions[5]).pattern == sequence::Pattern{3, {1}});
    CHECK(std::get<SetWeightAction>(actions[11]).value == Catch::Approx(0.6f));
    CHECK(std::get<SetWeightsAction>(actions[12]).pattern ==
          sequence::Pattern{4, {1}});
    CHECK(std::get<ShiftGateAction>(actions[17]).pattern == sequence::Pattern{9, {1}});
    CHECK(std::get<StepAction>(actions[18]).pattern == sequence::Pattern{10, {1}});
    CHECK(std::get<StepAction>(actions[18]).pitch_distance == 3);
    CHECK(std::get<StepAction>(actions[18]).velocity_distance ==
          Catch::Approx(0.4f));
    CHECK(std::get<DrumsAction>(actions[19]).octave_size == 24);
    CHECK(std::get<DrumsAction>(actions[19]).offset == -2);
}

TEST_CASE("Command adapter preserves edit/set/shift/step/drums defaults",
          "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "split; flip; fill note; fill rest; set pitch; set octave; set velocity; "
        "set delay; set gate; shift pitch; shift octave; shift velocity; shift delay; "
        "shift gate; step; drums");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 16);
    CHECK(std::get<SplitSelectionAction>(actions[0]).count == 2);
    CHECK(std::get<FlipSelectionAction>(actions[1]).pattern ==
          sequence::Pattern{0, {1}});
    CHECK(std::get<FillNoteAction>(actions[2]).pitch == 0);
    CHECK(std::get<FillNoteAction>(actions[2]).velocity ==
          Catch::Approx(100.f / 127.f));
    CHECK(std::get<FillNoteAction>(actions[2]).delay == Catch::Approx(0.f));
    CHECK(std::get<FillNoteAction>(actions[2]).gate == Catch::Approx(1.f));
    CHECK(std::get<FillRestAction>(actions[3]).pattern == sequence::Pattern{0, {1}});
    CHECK(std::holds_alternative<int>(std::get<SetPitchAction>(actions[4]).pitch));
    CHECK(std::get<int>(std::get<SetPitchAction>(actions[4]).pitch) == 0);
    CHECK(std::get<SetOctaveAction>(actions[5]).octave == 0);
    CHECK(std::holds_alternative<float>(
        std::get<SetVelocityAction>(actions[6]).velocity));
    CHECK(std::get<float>(std::get<SetVelocityAction>(actions[6]).velocity) ==
          Catch::Approx(100.f / 127.f));
    CHECK(std::holds_alternative<float>(std::get<SetDelayAction>(actions[7]).delay));
    CHECK(std::get<float>(std::get<SetDelayAction>(actions[7]).delay) ==
          Catch::Approx(0.f));
    CHECK(std::holds_alternative<float>(std::get<SetGateAction>(actions[8]).gate));
    CHECK(std::get<float>(std::get<SetGateAction>(actions[8]).gate) ==
          Catch::Approx(1.f));
    CHECK(std::get<ShiftPitchAction>(actions[9]).amount == 1);
    CHECK(std::get<ShiftOctaveAction>(actions[10]).amount == 1);
    CHECK(std::get<ShiftVelocityAction>(actions[11]).amount == Catch::Approx(0.1f));
    CHECK(std::get<ShiftDelayAction>(actions[12]).amount == Catch::Approx(0.1f));
    CHECK(std::get<ShiftGateAction>(actions[13]).amount == Catch::Approx(0.1f));
    CHECK(std::get<StepAction>(actions[14]).pitch_distance == 1);
    CHECK(std::get<StepAction>(actions[14]).velocity_distance == Catch::Approx(0.f));
    CHECK(std::get<DrumsAction>(actions[15]).octave_size == 16);
    CHECK(std::get<DrumsAction>(actions[15]).offset == 1);
}

TEST_CASE("Typed edit/set/shift/step/drums actions match invoke-command behavior",
          "[core][command][action]")
{
    auto tree = create_command_tree();

    auto seeded_state = []() {
        auto ps = make_plugin_state();
        auto state = ps.timeline.get_state();
        state.aux.selected.measure = 0;
        state.aux.selected.cell = {0};
        state.sequencer.sequence_bank[0].cell = {
            .element = sequence::Sequence{
                .cells = {
                    sequence::Cell{
                        .element = sequence::Note{5, 0.5f, 0.2f, 0.8f},
                        .weight = 0.7f,
                    },
                    sequence::Cell{
                        .element = sequence::Rest{},
                        .weight = 0.4f,
                    },
                },
            },
            .weight = 1.f,
        };
        ps.timeline.stage(std::move(state));
        return ps;
    };

    auto const mod_json =
        to_json(Modulator{modulator::Constant{0.35f}}).dump();

    auto run_parity = [&](std::string const &command) {
        auto typed_ps = seeded_state();
        auto invoke_ps = seeded_state();
        auto const context = ExecutionContext{typed_ps.timeline.get_state().aux};

        auto const typed_action = to_command_actions(parse_command_chain(command))[0];

        auto const typed_result =
            execute_command_action(typed_ps, context, typed_action);
        auto const invoke_result =
            execute_legacy_command(invoke_ps, tree, context, command);

        CHECK(typed_result.status == invoke_result.status);
        CHECK(typed_result.engine_mutated == invoke_result.engine_mutated);
        CHECK(typed_result.commit_intent == invoke_result.commit_intent);
        check_context_equal(typed_result.context, invoke_result.context);
        CHECK(typed_ps.timeline.get_state().sequencer ==
              invoke_ps.timeline.get_state().sequencer);
        CHECK(typed_ps.library.scale_shift_index ==
              invoke_ps.library.scale_shift_index);
    };

    run_parity("delete");
    run_parity("split 3");
    run_parity("lift");
    run_parity("+0 flip");
    run_parity("+0 fill note 2 0.6 0.1 0.8");
    run_parity("+0 fill rest");
    run_parity("set pitch 11");
    run_parity("set pitch " + mod_json);
    run_parity("set octave 2");
    run_parity("set velocity 0.3");
    run_parity("set velocity " + mod_json);
    run_parity("set delay 0.2");
    run_parity("set delay " + mod_json);
    run_parity("set gate 0.7");
    run_parity("set gate " + mod_json);
    run_parity("set weight 0.9");
    run_parity("+0 set weights 0.4");
    run_parity("+0 set weights " + mod_json);
    run_parity("+0 shift pitch -1");
    run_parity("+0 shift octave 1");
    run_parity("+0 shift velocity 0.2");
    run_parity("+0 shift delay -0.1");
    run_parity("+0 shift gate 0.15");
    run_parity("+0 step 2 0.1");
    run_parity("drums 20 3");
}

TEST_CASE(
    "Command adapter maps clipboard and sequence time-signature commands to typed actions",
    "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "copy; cut; paste; duplicate; set sequence timeSignature 7/8 3; "
        "double sequence timeSignature 2; halve sequence timeSignature 5");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 7);
    REQUIRE(std::holds_alternative<CopySelectionAction>(actions[0]));
    REQUIRE(std::holds_alternative<CutSelectionAction>(actions[1]));
    REQUIRE(std::holds_alternative<PasteSelectionAction>(actions[2]));
    REQUIRE(std::holds_alternative<DuplicateSelectionAction>(actions[3]));
    REQUIRE(std::holds_alternative<SetSequenceTimeSignatureAction>(actions[4]));
    REQUIRE(std::holds_alternative<DoubleSequenceTimeSignatureAction>(actions[5]));
    REQUIRE(std::holds_alternative<HalveSequenceTimeSignatureAction>(actions[6]));

    auto const set_ts = std::get<SetSequenceTimeSignatureAction>(actions[4]);
    CHECK(set_ts.time_signature.numerator == 7);
    CHECK(set_ts.time_signature.denominator == 8);
    CHECK(set_ts.index == 3);
    CHECK(std::get<DoubleSequenceTimeSignatureAction>(actions[5]).index == 2);
    CHECK(std::get<HalveSequenceTimeSignatureAction>(actions[6]).index == 5);
}

TEST_CASE(
    "Command adapter preserves clipboard and sequence time-signature defaults",
    "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "set sequence timeSignature; double sequence timeSignature; "
        "halve sequence timeSignature");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 3);
    auto const set_ts = std::get<SetSequenceTimeSignatureAction>(actions[0]);
    CHECK(set_ts.time_signature.numerator == 4);
    CHECK(set_ts.time_signature.denominator == 4);
    CHECK(set_ts.index == -1);
    CHECK(std::get<DoubleSequenceTimeSignatureAction>(actions[1]).index == -1);
    CHECK(std::get<HalveSequenceTimeSignatureAction>(actions[2]).index == -1);
}

TEST_CASE(
    "Typed duplicate and sequence time-signature actions match invoke-command behavior",
    "[core][command][action]")
{
    auto tree = create_command_tree();

    auto seeded_state = []() {
        auto ps = make_plugin_state();
        auto state = ps.timeline.get_state();
        state.aux.selected.measure = 2;
        state.aux.selected.cell = {0};
        state.sequencer.sequence_bank[2].cell = {
            .element = sequence::Sequence{
                .cells = {
                    sequence::Cell{
                        .element = sequence::Note{7, 0.6f, 0.1f, 0.9f},
                        .weight = 0.8f,
                    },
                    sequence::Cell{
                        .element = sequence::Rest{},
                        .weight = 0.5f,
                    },
                },
            },
            .weight = 1.f,
        };
        ps.timeline.stage(std::move(state));
        return ps;
    };

    auto run_parity = [&](std::string const &command) {
        auto typed_ps = seeded_state();
        auto invoke_ps = seeded_state();
        auto const context = ExecutionContext{typed_ps.timeline.get_state().aux};

        auto const typed_action = to_command_actions(parse_command_chain(command))[0];

        auto const typed_result =
            execute_command_action(typed_ps, context, typed_action);
        auto const invoke_result =
            execute_legacy_command(invoke_ps, tree, context, command);

        CHECK(typed_result.status == invoke_result.status);
        CHECK(typed_result.engine_mutated == invoke_result.engine_mutated);
        CHECK(typed_result.commit_intent == invoke_result.commit_intent);
        check_context_equal(typed_result.context, invoke_result.context);
        CHECK(typed_ps.timeline.get_state().sequencer ==
              invoke_ps.timeline.get_state().sequencer);
    };

    SECTION("duplicate")
    {
        run_parity("duplicate");
    }
    SECTION("set sequence timeSignature explicit")
    {
        run_parity("set sequence timeSignature 7/8 2");
    }
    SECTION("set sequence timeSignature default")
    {
        run_parity("set sequence timeSignature");
    }
    SECTION("set sequence timeSignature invalid")
    {
        run_parity("set sequence timeSignature 0/4");
    }
    SECTION("double sequence timeSignature")
    {
        run_parity("double sequence timeSignature 2");
    }
    SECTION("halve sequence timeSignature")
    {
        run_parity("halve sequence timeSignature 2");
    }
}

TEST_CASE("Command adapter maps misc and arp commands to typed actions",
          "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "welcome; version; reset; focus statusBar; show chordsPane; "
        "set theme neon; +1 2 arp major 1");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 7);
    REQUIRE(std::holds_alternative<WelcomeAction>(actions[0]));
    REQUIRE(std::holds_alternative<VersionAction>(actions[1]));
    REQUIRE(std::holds_alternative<ResetAction>(actions[2]));
    REQUIRE(std::holds_alternative<DeprecatedFocusAction>(actions[3]));
    REQUIRE(std::holds_alternative<DeprecatedShowAction>(actions[4]));
    REQUIRE(std::holds_alternative<SetThemeAction>(actions[5]));
    REQUIRE(std::holds_alternative<ArpAction>(actions[6]));

    CHECK(std::get<DeprecatedFocusAction>(actions[3]).component_id == "statusBar");
    CHECK(std::get<DeprecatedShowAction>(actions[4]).component_id == "chordsPane");
    CHECK(std::get<SetThemeAction>(actions[5]).name == "neon");
    CHECK(std::get<ArpAction>(actions[6]).pattern == sequence::Pattern{1, {2}});
    CHECK(std::get<ArpAction>(actions[6]).chord == "major");
    CHECK(std::get<ArpAction>(actions[6]).inversion == 1);
}

TEST_CASE("Command adapter preserves arp defaults", "[core][command][action]")
{
    auto const invocations = parse_command_chain("arp; +2 arp cycle; +3 arp major");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 3);
    CHECK(std::get<ArpAction>(actions[0]).pattern == sequence::Pattern{0, {1}});
    CHECK(std::get<ArpAction>(actions[0]).chord == "cycle");
    CHECK(std::get<ArpAction>(actions[0]).inversion == -1);
    CHECK(std::get<ArpAction>(actions[1]).pattern == sequence::Pattern{2, {1}});
    CHECK(std::get<ArpAction>(actions[1]).chord == "cycle");
    CHECK(std::get<ArpAction>(actions[1]).inversion == -1);
    CHECK(std::get<ArpAction>(actions[2]).pattern == sequence::Pattern{3, {1}});
    CHECK(std::get<ArpAction>(actions[2]).chord == "major");
    CHECK(std::get<ArpAction>(actions[2]).inversion == -1);
}

TEST_CASE("Typed misc and arp actions match invoke-command behavior",
          "[core][command][action]")
{
    auto tree = create_command_tree();

    auto seeded_state = []() {
        auto ps = make_plugin_state();
        auto state = ps.timeline.get_state();
        state.aux.selected.measure = 0;
        state.aux.selected.cell.clear();
        state.sequencer.sequence_bank[0].cell = {
            .element = sequence::Sequence{
                .cells = {
                    sequence::Cell{
                        .element = sequence::Note{0, 0.6f, 0.1f, 0.8f},
                        .weight = 0.5f,
                    },
                    sequence::Cell{
                        .element = sequence::Note{2, 0.6f, 0.1f, 0.8f},
                        .weight = 0.5f,
                    },
                    sequence::Cell{
                        .element = sequence::Note{4, 0.6f, 0.1f, 0.8f},
                        .weight = 0.5f,
                    },
                },
            },
            .weight = 1.f,
        };
        ps.library.chords = {
            Chord{.name = "major", .intervals = {0, 4, 7}},
            Chord{.name = "minor", .intervals = {0, 3, 7}},
        };
        ps.timeline.stage(std::move(state));
        return ps;
    };

    auto run_parity = [&](std::string const &command) {
        auto typed_ps = seeded_state();
        auto invoke_ps = seeded_state();
        auto const context = ExecutionContext{typed_ps.timeline.get_state().aux};

        auto const typed_action = to_command_actions(parse_command_chain(command))[0];

        auto const typed_result =
            execute_command_action(typed_ps, context, typed_action);
        auto const invoke_result =
            execute_legacy_command(invoke_ps, tree, context, command);

        CHECK(typed_result.status == invoke_result.status);
        CHECK(typed_result.engine_mutated == invoke_result.engine_mutated);
        CHECK(typed_result.commit_intent == invoke_result.commit_intent);
        check_context_equal(typed_result.context, invoke_result.context);
        CHECK(typed_ps.timeline.get_state().sequencer ==
              invoke_ps.timeline.get_state().sequencer);
        CHECK(typed_ps.library.scale_shift_index ==
              invoke_ps.library.scale_shift_index);
    };

    run_parity("welcome");
    run_parity("version");
    run_parity("reset");
    run_parity("focus statusBar");
    run_parity("show chordsPane");
    run_parity("set theme neon");
    run_parity("+0 arp major 1");
    run_parity("+0 arp cycle");
}

TEST_CASE("Command adapter maps load/save/libraryDirectory commands to typed actions",
          "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "load sequenceBank demo; load tuning edo12; load keys; load scales; "
        "load chords; save sequenceBank backup; libraryDirectory");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 7);
    REQUIRE(std::holds_alternative<LoadSequenceBankAction>(actions[0]));
    REQUIRE(std::holds_alternative<LoadTuningAction>(actions[1]));
    REQUIRE(std::holds_alternative<LoadKeysAction>(actions[2]));
    REQUIRE(std::holds_alternative<LoadScalesAction>(actions[3]));
    REQUIRE(std::holds_alternative<LoadChordsAction>(actions[4]));
    REQUIRE(std::holds_alternative<SaveSequenceBankAction>(actions[5]));
    REQUIRE(std::holds_alternative<LibraryDirectoryAction>(actions[6]));

    CHECK(std::get<LoadSequenceBankAction>(actions[0]).filename == "demo");
    CHECK(std::get<LoadTuningAction>(actions[1]).filename == "edo12");
    CHECK(std::get<SaveSequenceBankAction>(actions[5]).filename == "backup");
}

TEST_CASE("Typed load/save/libraryDirectory actions match invoke-command behavior",
          "[core][command][action]")
{
    auto tree = create_command_tree();

    auto const temp_root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getChildFile("xen_command_action_io_" +
                                             juce::Uuid{}.toString());
    temp_root.createDirectory();
    auto const seq_dir = temp_root.getChildFile("sequences");
    auto const tuning_dir = temp_root.getChildFile("tunings");
    seq_dir.createDirectory();
    tuning_dir.createDirectory();

    auto fixture_bank = SequenceBank{};
    fixture_bank[0].cell = {
        .element = sequence::Note{11, 0.8f, 0.1f, 0.9f},
        .weight = 1.f,
    };
    auto fixture_names = std::array<std::string, 16>{};
    fixture_names[0] = "fixture";
    action::save_sequence_bank(fixture_bank, fixture_names,
                               seq_dir.getChildFile("fixture.xss"));

    auto const tuning_file = tuning_dir.getChildFile("fixture.scl");
    tuning_file.replaceWithText(
        "! fixture.scl\n"
        "fixture\n"
        "3\n"
        "!\n"
        "100.000000\n"
        "200.000000\n"
        "2/1\n");

    auto seeded_state = [&]() {
        auto ps = make_plugin_state();
        ps.config.current_sequence_directory = seq_dir;
        ps.config.current_tuning_directory = tuning_dir;

        auto state = ps.timeline.get_state();
        state.sequencer.sequence_bank[0].cell = {
            .element = sequence::Rest{},
            .weight = 1.f,
        };
        state.sequencer.sequence_names[0] = "seed";
        ps.timeline.stage(std::move(state));
        return ps;
    };

    auto run_parity = [&](std::string const &command) {
        auto typed_ps = seeded_state();
        auto invoke_ps = seeded_state();
        auto const context = ExecutionContext{typed_ps.timeline.get_state().aux};

        auto const typed_action = to_command_actions(parse_command_chain(command))[0];
        auto const typed_result =
            execute_command_action(typed_ps, context, typed_action);
        auto const invoke_result =
            execute_legacy_command(invoke_ps, tree, context, command);

        CHECK(typed_result.status == invoke_result.status);
        CHECK(typed_result.engine_mutated == invoke_result.engine_mutated);
        CHECK(typed_result.commit_intent == invoke_result.commit_intent);
        check_context_equal(typed_result.context, invoke_result.context);
        CHECK(typed_ps.timeline.get_state().sequencer ==
              invoke_ps.timeline.get_state().sequencer);
    };

    run_parity("save sequenceBank saved");
    run_parity("load sequenceBank fixture");
    run_parity("load tuning fixture");
    run_parity("load keys");
    run_parity("libraryDirectory");

    temp_root.deleteRecursively();
}

TEST_CASE(
    "Command adapter maps randomize and transform commands to typed action variants",
    "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "+1 2 randomize pitch -3 3; +2 randomize velocity 0.2 0.2; "
        "+3 randomize delay 0.1 0.1; +4 randomize gate 0.9 0.9; "
        "+5 2 stretch 3; compress; shuffle; rotate -2; reverse; "
        "+7 mirror 12; +8 quantize; swing 0.2");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 12);
    REQUIRE(std::holds_alternative<RandomizePitchAction>(actions[0]));
    REQUIRE(std::holds_alternative<RandomizeVelocityAction>(actions[1]));
    REQUIRE(std::holds_alternative<RandomizeDelayAction>(actions[2]));
    REQUIRE(std::holds_alternative<RandomizeGateAction>(actions[3]));
    REQUIRE(std::holds_alternative<StretchAction>(actions[4]));
    REQUIRE(std::holds_alternative<CompressAction>(actions[5]));
    REQUIRE(std::holds_alternative<ShuffleAction>(actions[6]));
    REQUIRE(std::holds_alternative<RotateAction>(actions[7]));
    REQUIRE(std::holds_alternative<ReverseAction>(actions[8]));
    REQUIRE(std::holds_alternative<MirrorAction>(actions[9]));
    REQUIRE(std::holds_alternative<QuantizeAction>(actions[10]));
    REQUIRE(std::holds_alternative<SwingAction>(actions[11]));

    CHECK(std::get<RandomizePitchAction>(actions[0]).pattern ==
          sequence::Pattern{1, {2}});
    CHECK(std::get<RandomizePitchAction>(actions[0]).min == -3);
    CHECK(std::get<RandomizePitchAction>(actions[0]).max == 3);
    CHECK(std::get<StretchAction>(actions[4]).pattern == sequence::Pattern{5, {2}});
    CHECK(std::get<StretchAction>(actions[4]).count == 3);
    CHECK(std::get<RotateAction>(actions[7]).amount == -2);
    CHECK(std::get<MirrorAction>(actions[9]).pattern == sequence::Pattern{7, {1}});
    CHECK(std::get<MirrorAction>(actions[9]).center_pitch == 12);
    CHECK(std::get<QuantizeAction>(actions[10]).pattern ==
          sequence::Pattern{8, {1}});
    CHECK(std::get<SwingAction>(actions[11]).amount == Catch::Approx(0.2f));
}

TEST_CASE("Command adapter preserves randomize and transform defaults",
          "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "randomize pitch; randomize velocity; randomize delay; randomize gate; "
        "stretch; rotate; mirror; swing");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 8);
    CHECK(std::get<RandomizePitchAction>(actions[0]).min == -12);
    CHECK(std::get<RandomizePitchAction>(actions[0]).max == 12);
    CHECK(std::get<RandomizeVelocityAction>(actions[1]).min == Catch::Approx(0.01f));
    CHECK(std::get<RandomizeVelocityAction>(actions[1]).max == Catch::Approx(1.f));
    CHECK(std::get<RandomizeDelayAction>(actions[2]).min == Catch::Approx(0.f));
    CHECK(std::get<RandomizeDelayAction>(actions[2]).max == Catch::Approx(0.95f));
    CHECK(std::get<RandomizeGateAction>(actions[3]).min == Catch::Approx(0.f));
    CHECK(std::get<RandomizeGateAction>(actions[3]).max == Catch::Approx(0.95f));
    CHECK(std::get<StretchAction>(actions[4]).count == 2);
    CHECK(std::get<RotateAction>(actions[5]).amount == 1);
    CHECK(std::get<MirrorAction>(actions[6]).center_pitch == 0);
    CHECK(std::get<SwingAction>(actions[7]).amount == Catch::Approx(0.1f));
}

TEST_CASE("Typed randomize and transform actions match invoke-command behavior",
          "[core][command][action]")
{
    auto tree = create_command_tree();
    auto typed_ps = make_plugin_state();
    auto invoke_ps = make_plugin_state();

    auto seed = [](PluginState &ps) {
        auto state = ps.timeline.get_state();
        auto &selected = get_selected_cell(state.sequencer.sequence_bank, state.aux.selected);
        selected.element = sequence::Note{5, 0.5f, 0.2f, 0.8f};
        selected.weight = 0.7f;
        ps.timeline.stage(std::move(state));
    };
    seed(typed_ps);
    seed(invoke_ps);

    auto context = ExecutionContext{};
    auto run_parity = [&](std::string const &command) {
        auto const typed_action = to_command_actions(parse_command_chain(command))[0];

        auto const typed_result =
            execute_command_action(typed_ps, context, typed_action);
        auto const invoke_result =
            execute_legacy_command(invoke_ps, tree, context, command);

        CHECK(typed_result.status == invoke_result.status);
        CHECK(typed_result.engine_mutated == invoke_result.engine_mutated);
        check_context_equal(typed_result.context, invoke_result.context);
        CHECK(typed_ps.timeline.get_state().sequencer ==
              invoke_ps.timeline.get_state().sequencer);

        context = typed_result.context;
    };

    run_parity("+0 randomize pitch 4 4");
    run_parity("+0 randomize velocity 0.3 0.3");
    run_parity("+0 randomize delay 0.4 0.4");
    run_parity("+0 randomize gate 0.5 0.5");
    run_parity("+0 stretch 2");
    run_parity("+1 compress");
    run_parity("compress");
    run_parity("shuffle");
    run_parity("rotate -1");
    run_parity("reverse");
    run_parity("+0 mirror 10");
    run_parity("+0 quantize");
    run_parity("swing 0.25");
}
