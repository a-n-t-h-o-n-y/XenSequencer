#include <stdexcept>
#include <variant>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <sequence/sequence.hpp>
#include <sequence/tuning.hpp>
#include <xen/actions.hpp>
#include <xen/command.hpp>
#include <xen/command_action.hpp>
#include <xen/command_catalog.hpp>
#include <xen/message_level.hpp>
#include <xen/selection.hpp>
#include <xen/state.hpp>
#include <xen/utility.hpp>

using namespace xen;

namespace
{

auto make_plugin_state() -> PluginState
{
    return PluginState{
        .timeline =
            XenTimeline{
                TimelineState{
                    .sequencer = {},
                    .aux = {},
                },
            },
    };
}

auto to_command_actions(std::vector<CommandInvocation> const &invocations)
    -> std::vector<CommandAction>
{
    auto const result = bind_chain(invocations);
    if (std::holds_alternative<CatalogBindError>(result))
    {
        auto const &error = std::get<CatalogBindError>(result);
        if (error.kind == CatalogBindErrorKind::UnknownCommand)
        {
            throw utility::ErrorNoMatch{};
        }
        throw std::invalid_argument(error.message);
    }

    auto actions = std::vector<CommandAction>{};
    auto const &bound_commands = std::get<std::vector<BoundCommand>>(result);
    actions.reserve(bound_commands.size());
    for (auto const &bound : bound_commands)
    {
        if (bound.control == BoundCommandControl::ReplayPrevious)
        {
            actions.push_back(AgainAction{});
            continue;
        }
        REQUIRE(bound.action.has_value());
        actions.push_back(*bound.action);
    }
    return actions;
}

auto singleton_sequence_cell_selection(std::vector<std::size_t> const &indices)
    -> SelectedState
{
    auto selected = SelectedState{};
    for (auto const index : indices)
    {
        selected.path.push_back({.kind = SelectionStepKind::Element, .index = 0});
        selected.path.push_back(
            {.kind = SelectionStepKind::SequenceCell, .index = index});
    }
    return selected;
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

    auto const bind_result = bind_invocation(invocations.front());
    REQUIRE(std::holds_alternative<CatalogBindError>(bind_result));
    CHECK(std::get<CatalogBindError>(bind_result).kind ==
          CatalogBindErrorKind::UnknownCommand);
    CHECK_THROWS_AS(to_command_actions(invocations), utility::ErrorNoMatch);
}

TEST_CASE("Official command catalog maps to typed actions", "[core][command][action]")
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
        "load measure fixture",
        "load tuning fixture",
        "load keys",
        "load scales",
        "load chords",
        "save measure fixture",
        "libraryDirectory",
        "move left 1",
        "move right 1",
        "move up 1",
        "move down 1",
        "note",
        "delete",
        "split 2",
        "lift",
        "set pitch 0",
        "set octave 0",
        "set velocity 0.5",
        "set delay 0.1",
        "set gate 0.5",
        "set measure timeSignature 4/4",
        "set baseFrequency 440",
        "set scale chromatic",
        "set mode 1",
        "set translateDirection up",
        "set key 0",
        "set weight 1",
        "+0 set weights 0.5",
        "double measure timeSignature",
        "halve measure timeSignature",
        "+0 shift pitch 1",
        "+0 shift octave 1",
        "+0 shift velocity 0.1",
        "+0 shift delay 0.1",
        "+0 shift gate 0.1",
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
        "+0 step 1 0.1",
        "drums",
        "+0 arp major 1",
        "chord major 1",
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
        "move down 2; set key 11; inputMode gate; note 2 0.7 0.1 0.9; "
        "set baseFrequency 880; commit; undo; redo; "
        "set scale major; set mode 2; set translateDirection down; version");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 12);
    CHECK(std::holds_alternative<MoveSelectionAction>(actions[0]));
    CHECK(std::holds_alternative<SetKeyAction>(actions[1]));
    CHECK(std::holds_alternative<SetInputModeAction>(actions[2]));
    CHECK(std::holds_alternative<CreateNoteAction>(actions[3]));
    CHECK(std::holds_alternative<SetBaseFrequencyAction>(actions[4]));
    CHECK(std::holds_alternative<CommitAction>(actions[5]));
    CHECK(std::holds_alternative<UndoAction>(actions[6]));
    CHECK(std::holds_alternative<RedoAction>(actions[7]));
    CHECK(std::holds_alternative<SetScaleAction>(actions[8]));
    CHECK(std::holds_alternative<SetScaleModeAction>(actions[9]));
    CHECK(std::holds_alternative<SetTranslateDirectionAction>(actions[10]));
    CHECK(std::holds_alternative<VersionAction>(actions[11]));

    auto const move = std::get<MoveSelectionAction>(actions[0]);
    CHECK(move.direction == MoveDirection::Down);
    CHECK(move.amount == 2);

    auto const set_key = std::get<SetKeyAction>(actions[1]);
    CHECK(set_key.key == 11);

    auto const input_mode = std::get<SetInputModeAction>(actions[2]);
    CHECK(input_mode.mode == InputMode::Gate);

    auto const note = std::get<CreateNoteAction>(actions[3]);
    CHECK(note.pitch == 2);
    CHECK(note.velocity == Catch::Approx(0.7f));
    CHECK(note.delay == Catch::Approx(0.1f));
    CHECK(note.gate == Catch::Approx(0.9f));

    auto const base = std::get<SetBaseFrequencyAction>(actions[4]);
    CHECK(base.freq == Catch::Approx(880.f));

    auto const mode = std::get<SetScaleModeAction>(actions[9]);
    CHECK(mode.mode_index == 2);

    auto const translate = std::get<SetTranslateDirectionAction>(actions[10]);
    CHECK(translate.direction == "down");
}

TEST_CASE("Command adapter preserves migrated command defaults",
          "[core][command][action]")
{
    auto const invocations = parse_command_chain("set key; note; set baseFrequency");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 3);
    REQUIRE(std::holds_alternative<SetKeyAction>(actions[0]));
    REQUIRE(std::holds_alternative<CreateNoteAction>(actions[1]));
    REQUIRE(std::holds_alternative<SetBaseFrequencyAction>(actions[2]));

    CHECK(std::get<SetKeyAction>(actions[0]).key == 0);
    CHECK(std::get<CreateNoteAction>(actions[1]).pitch == 0);
    CHECK(std::get<CreateNoteAction>(actions[1]).velocity ==
          Catch::Approx(100.f / 127.f));
    CHECK(std::get<CreateNoteAction>(actions[1]).delay == Catch::Approx(0.f));
    CHECK(std::get<CreateNoteAction>(actions[1]).gate == Catch::Approx(1.f));
    CHECK(std::get<SetBaseFrequencyAction>(actions[2]).freq == Catch::Approx(440.f));
}

TEST_CASE("Command adapter maps shift commands to typed action variants",
          "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "shift scale; shift scaleMode -1; shift translateDirection; "
        "shift entireScale -1");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 4);
    REQUIRE(std::holds_alternative<ShiftScaleAction>(actions[0]));
    REQUIRE(std::holds_alternative<ShiftScaleModeAction>(actions[1]));
    REQUIRE(std::holds_alternative<ShiftTranslateDirectionAction>(actions[2]));
    REQUIRE(std::holds_alternative<ShiftEntireScaleAction>(actions[3]));

    CHECK(std::get<ShiftScaleAction>(actions[0]).amount == 1);
    CHECK(std::get<ShiftScaleModeAction>(actions[1]).amount == -1);
    CHECK(std::get<ShiftEntireScaleAction>(actions[3]).direction == -1);
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
    context.selected = singleton_sequence_cell_selection({0});

    auto const actions = to_command_actions(parse_command_chain("set key 12"));
    REQUIRE(actions.size() == 1);

    auto const result = execute_command_action(ps, context, actions[0]);
    CHECK(result.status.first == MessageLevel::Info);
    CHECK(result.context.selected == singleton_sequence_cell_selection({0}));
    CHECK(ps.timeline.get_state().sequencer.key == 12);
}

TEST_CASE("Typed action execution exposes commit intent and mutation metadata",
          "[core][command][action]")
{
    auto ps = make_plugin_state();
    auto context = ExecutionContext{ps.timeline.get_state().aux};

    auto const set_key_actions = to_command_actions(parse_command_chain("set key 7"));
    REQUIRE(set_key_actions.size() == 1);
    auto const set_key_result = execute_command_action(ps, context, set_key_actions[0]);
    CHECK(set_key_result.status.first == MessageLevel::Info);
    CHECK(set_key_result.engine_mutated);
    CHECK(set_key_result.commit_intent == CommitIntent::Auto);

    context = set_key_result.context;

    auto const commit_actions = to_command_actions(parse_command_chain("commit"));
    REQUIRE(commit_actions.size() == 1);
    auto const commit_result = execute_command_action(ps, context, commit_actions[0]);
    CHECK(commit_result.status.first == MessageLevel::Debug);
    CHECK_FALSE(commit_result.engine_mutated);
    CHECK(commit_result.commit_intent == CommitIntent::Force);

    auto const split_actions = to_command_actions(parse_command_chain("split 2"));
    REQUIRE(split_actions.size() == 1);
    auto const split_result = execute_command_action(ps, context, split_actions[0]);
    REQUIRE(split_result.status.first == MessageLevel::Info);

    auto const defer_actions =
        to_command_actions(parse_command_chain("set weights 0.5"));
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
    CHECK(result.context.selected ==
          action::move_right(ps.timeline.get_state().sequencer, context, 1).selected);
}

TEST_CASE("Typed input mode update context-only state", "[core][command][action]")
{
    auto ps = make_plugin_state();

    auto context = ExecutionContext{};
    context.selected = singleton_sequence_cell_selection({0});
    context.input_mode = InputMode::Pitch;

    auto const mode_result = execute_command_action(
        ps, context, CommandAction{SetInputModeAction{.mode = InputMode::Gate}});
    CHECK(mode_result.status.first == MessageLevel::Info);
    CHECK(mode_result.status.second == "Input Mode Set to 'gate'");
    CHECK_FALSE(mode_result.engine_mutated);
    CHECK(mode_result.context.input_mode == InputMode::Gate);
    CHECK(mode_result.context.selected == singleton_sequence_cell_selection({0}));
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
    CHECK(ps.timeline.get_state().sequencer.base_frequency == Catch::Approx(20'000.f));
}

TEST_CASE(
    "Command adapter maps edit, set, shift, step, and drums commands to typed actions",
    "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "delete; split 3; lift; set pitch 9; set octave 1; set velocity 0.75; "
        "set delay 0.2; set gate 0.8; set weight 0.6; +4 set weights 0.5; "
        "+5 shift pitch -2; +6 shift octave 2; +7 shift velocity -0.1; "
        "+8 shift delay 0.25; +9 shift gate -0.2; +10 step 3 0.4; drums 24 -2");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 17);
    REQUIRE(std::holds_alternative<DeleteSelectionAction>(actions[0]));
    REQUIRE(std::holds_alternative<SplitSelectionAction>(actions[1]));
    REQUIRE(std::holds_alternative<LiftSelectionAction>(actions[2]));
    REQUIRE(std::holds_alternative<SetPitchAction>(actions[3]));
    REQUIRE(std::holds_alternative<SetOctaveAction>(actions[4]));
    REQUIRE(std::holds_alternative<SetVelocityAction>(actions[5]));
    REQUIRE(std::holds_alternative<SetDelayAction>(actions[6]));
    REQUIRE(std::holds_alternative<SetGateAction>(actions[7]));
    REQUIRE(std::holds_alternative<SetWeightAction>(actions[8]));
    REQUIRE(std::holds_alternative<SetWeightsAction>(actions[9]));
    REQUIRE(std::holds_alternative<ShiftPitchAction>(actions[10]));
    REQUIRE(std::holds_alternative<ShiftOctaveAction>(actions[11]));
    REQUIRE(std::holds_alternative<ShiftVelocityAction>(actions[12]));
    REQUIRE(std::holds_alternative<ShiftDelayAction>(actions[13]));
    REQUIRE(std::holds_alternative<ShiftGateAction>(actions[14]));
    REQUIRE(std::holds_alternative<StepAction>(actions[15]));
    REQUIRE(std::holds_alternative<DrumsAction>(actions[16]));

    CHECK(std::get<SplitSelectionAction>(actions[1]).count == 3);
    CHECK(std::get<SetWeightAction>(actions[8]).value == Catch::Approx(0.6f));
    CHECK(std::get<SetWeightsAction>(actions[9]).pattern == sequence::Pattern{4, {1}});
    CHECK(std::get<ShiftGateAction>(actions[14]).pattern == sequence::Pattern{9, {1}});
    CHECK(std::get<StepAction>(actions[15]).pattern == sequence::Pattern{10, {1}});
    CHECK(std::get<StepAction>(actions[15]).pitch_distance == 3);
    CHECK(std::get<StepAction>(actions[15]).velocity_distance == Catch::Approx(0.4f));
    CHECK(std::get<DrumsAction>(actions[16]).octave_size == 24);
    CHECK(std::get<DrumsAction>(actions[16]).offset == -2);
}

TEST_CASE("Command adapter preserves edit/set/shift/step/drums defaults",
          "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "split; set pitch; set octave; set velocity; set delay; set gate; "
        "shift pitch; shift octave; shift velocity; shift delay; shift gate; "
        "step; drums");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 13);
    CHECK(std::get<SplitSelectionAction>(actions[0]).count == 2);
    CHECK(std::holds_alternative<int>(std::get<SetPitchAction>(actions[1]).pitch));
    CHECK(std::get<int>(std::get<SetPitchAction>(actions[1]).pitch) == 0);
    CHECK(std::get<SetOctaveAction>(actions[2]).octave == 0);
    CHECK(std::holds_alternative<float>(
        std::get<SetVelocityAction>(actions[3]).velocity));
    CHECK(std::get<float>(std::get<SetVelocityAction>(actions[3]).velocity) ==
          Catch::Approx(100.f / 127.f));
    CHECK(std::holds_alternative<float>(std::get<SetDelayAction>(actions[4]).delay));
    CHECK(std::get<float>(std::get<SetDelayAction>(actions[4]).delay) ==
          Catch::Approx(0.f));
    CHECK(std::holds_alternative<float>(std::get<SetGateAction>(actions[5]).gate));
    CHECK(std::get<float>(std::get<SetGateAction>(actions[5]).gate) ==
          Catch::Approx(1.f));
    CHECK(std::get<ShiftPitchAction>(actions[6]).amount == 1);
    CHECK(std::get<ShiftOctaveAction>(actions[7]).amount == 1);
    CHECK(std::get<ShiftVelocityAction>(actions[8]).amount == Catch::Approx(0.1f));
    CHECK(std::get<ShiftDelayAction>(actions[9]).amount == Catch::Approx(0.1f));
    CHECK(std::get<ShiftGateAction>(actions[10]).amount == Catch::Approx(0.1f));
    CHECK(std::get<StepAction>(actions[11]).pitch_distance == 1);
    CHECK(std::get<StepAction>(actions[11]).velocity_distance == Catch::Approx(0.f));
    CHECK(std::get<DrumsAction>(actions[12]).octave_size == 16);
    CHECK(std::get<DrumsAction>(actions[12]).offset == 1);
}

TEST_CASE("Command adapter maps clipboard and measure time-signature commands to typed "
          "actions",
          "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "copy; cut; paste; duplicate; set measure timeSignature 7/8; "
        "double measure timeSignature; halve measure timeSignature");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 7);
    REQUIRE(std::holds_alternative<CopySelectionAction>(actions[0]));
    REQUIRE(std::holds_alternative<CutSelectionAction>(actions[1]));
    REQUIRE(std::holds_alternative<PasteSelectionAction>(actions[2]));
    REQUIRE(std::holds_alternative<DuplicateSelectionAction>(actions[3]));
    REQUIRE(std::holds_alternative<SetMeasureTimeSignatureAction>(actions[4]));
    REQUIRE(std::holds_alternative<DoubleMeasureTimeSignatureAction>(actions[5]));
    REQUIRE(std::holds_alternative<HalveMeasureTimeSignatureAction>(actions[6]));

    auto const set_ts = std::get<SetMeasureTimeSignatureAction>(actions[4]);
    CHECK(set_ts.time_signature.numerator == 7);
    CHECK(set_ts.time_signature.denominator == 8);
}

TEST_CASE("Command adapter preserves clipboard and measure time-signature defaults",
          "[core][command][action]")
{
    auto const invocations =
        parse_command_chain("set measure timeSignature; double measure timeSignature; "
                            "halve measure timeSignature");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 3);
    auto const set_ts = std::get<SetMeasureTimeSignatureAction>(actions[0]);
    CHECK(set_ts.time_signature.numerator == 4);
    CHECK(set_ts.time_signature.denominator == 4);
}

TEST_CASE("Command adapter maps misc, arp, and chord commands to typed actions",
          "[core][command][action]")
{
    auto const invocations =
        parse_command_chain("welcome; version; reset; +1 2 arp major 1; chord minor 2");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 5);
    REQUIRE(std::holds_alternative<WelcomeAction>(actions[0]));
    REQUIRE(std::holds_alternative<VersionAction>(actions[1]));
    REQUIRE(std::holds_alternative<ResetAction>(actions[2]));
    REQUIRE(std::holds_alternative<ArpAction>(actions[3]));
    CHECK(std::get<ArpAction>(actions[3]).pattern == sequence::Pattern{1, {2}});
    CHECK(std::get<ArpAction>(actions[3]).chord == "major");
    CHECK(std::get<ArpAction>(actions[3]).inversion == 1);
    REQUIRE(std::holds_alternative<ChordAction>(actions[4]));
    CHECK(std::get<ChordAction>(actions[4]).chord == "minor");
    CHECK(std::get<ChordAction>(actions[4]).inversion == 2);
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

TEST_CASE("Command adapter preserves chord defaults", "[core][command][action]")
{
    auto const invocations = parse_command_chain("chord; chord cycle; chord major");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 3);
    CHECK(std::get<ChordAction>(actions[0]).chord == "cycle");
    CHECK(std::get<ChordAction>(actions[0]).inversion == -1);
    CHECK(std::get<ChordAction>(actions[1]).chord == "cycle");
    CHECK(std::get<ChordAction>(actions[1]).inversion == -1);
    CHECK(std::get<ChordAction>(actions[2]).chord == "major");
    CHECK(std::get<ChordAction>(actions[2]).inversion == -1);
}

TEST_CASE("Command adapter maps load/save/libraryDirectory commands to typed actions",
          "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "load measure demo; load tuning edo12; load keys; load scales; "
        "load chords; save measure backup; libraryDirectory");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 7);
    REQUIRE(std::holds_alternative<LoadMeasureAction>(actions[0]));
    REQUIRE(std::holds_alternative<LoadTuningAction>(actions[1]));
    REQUIRE(std::holds_alternative<LoadKeysAction>(actions[2]));
    REQUIRE(std::holds_alternative<LoadScalesAction>(actions[3]));
    REQUIRE(std::holds_alternative<LoadChordsAction>(actions[4]));
    REQUIRE(std::holds_alternative<SaveMeasureAction>(actions[5]));
    REQUIRE(std::holds_alternative<LibraryDirectoryAction>(actions[6]));

    CHECK(std::get<LoadMeasureAction>(actions[0]).filename == "demo");
    CHECK(std::get<LoadTuningAction>(actions[1]).filename == "edo12");
    CHECK(std::get<SaveMeasureAction>(actions[5]).filename == "backup");
}

TEST_CASE(
    "Command adapter maps randomize and transform commands to typed action variants",
    "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "+1 2 randomize pitch -3 3; +2 randomize velocity 0.2 0.2; "
        "+3 randomize delay 0.1 0.1; +4 randomize gate 0.9 0.9; "
        "+5 2 stretch 3; compress; shuffle; rotate -2; reverse; +7 mirror 12");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 10);
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

    CHECK(std::get<RandomizePitchAction>(actions[0]).pattern ==
          sequence::Pattern{1, {2}});
    CHECK(std::get<RandomizePitchAction>(actions[0]).min == -3);
    CHECK(std::get<RandomizePitchAction>(actions[0]).max == 3);
    CHECK(std::get<StretchAction>(actions[4]).pattern == sequence::Pattern{5, {2}});
    CHECK(std::get<StretchAction>(actions[4]).count == 3);
    CHECK(std::get<RotateAction>(actions[7]).amount == -2);
    CHECK(std::get<MirrorAction>(actions[9]).pattern == sequence::Pattern{7, {1}});
    CHECK(std::get<MirrorAction>(actions[9]).center_pitch == 12);
}

TEST_CASE("Command adapter preserves randomize and transform defaults",
          "[core][command][action]")
{
    auto const invocations = parse_command_chain(
        "randomize pitch; randomize velocity; randomize delay; randomize gate; "
        "stretch; rotate; mirror");
    auto const actions = to_command_actions(invocations);

    REQUIRE(actions.size() == 7);
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
}

TEST_CASE("Typed randomize and transform actions execute deterministically",
          "[core][command][action]")
{
    auto ps = make_plugin_state();
    auto state = ps.timeline.get_state();
    auto &selected = get_selected_cell(state.sequencer.measure, state.aux.selected);
    selected.elements = {sequence::Note{5, 0.5f, 0.2f, 0.8f}};
    selected.weight = 0.7f;
    ps.timeline.stage(std::move(state));

    auto context = ExecutionContext{};
    auto run_action = [&](std::string const &command) -> CommandActionResult {
        auto const actions = to_command_actions(parse_command_chain(command));
        REQUIRE(actions.size() == 1);
        auto const result = execute_command_action(ps, context, actions.front());
        context = result.context;
        return result;
    };

    CHECK(run_action("+0 randomize pitch 4 4").status.first == MessageLevel::Info);
    CHECK(run_action("+0 randomize velocity 0.3 0.3").status.first ==
          MessageLevel::Info);
    CHECK(run_action("+0 randomize delay 0.4 0.4").status.first == MessageLevel::Info);
    CHECK(run_action("+0 randomize gate 0.5 0.5").status.first == MessageLevel::Info);
    CHECK(run_action("+0 stretch 2").status.first == MessageLevel::Info);
    CHECK(run_action("+1 compress").status.first == MessageLevel::Info);
    CHECK(run_action("shuffle").status.first == MessageLevel::Info);
    CHECK(run_action("rotate -1").status.first == MessageLevel::Info);
    CHECK(run_action("reverse").status.first == MessageLevel::Info);
    CHECK(run_action("+0 mirror 10").status.first == MessageLevel::Info);
}

TEST_CASE("Typed chord action applies chord intervals across cell elements",
          "[core][command][action]")
{
    auto ps = make_plugin_state();
    ps.library.chords = {
        Chord{.name = "major", .intervals = {0, 4, 7}},
        Chord{.name = "minor", .intervals = {0, 3, 7}},
    };

    auto state = ps.timeline.get_state();
    state.sequencer.measure.cell.elements = {
        sequence::Note{10, 0.5f, 0.1f, 0.8f},
        sequence::Sequence{.cells = {sequence::Cell{
                               .elements = {sequence::Note{10, 0.5f, 0.1f, 0.8f}},
                               .weight = 1.f,
                           }}},
        sequence::Note{10, 0.5f, 0.1f, 0.8f},
        sequence::Note{10, 0.5f, 0.1f, 0.8f},
    };
    state.aux.arp_state.previous_chord_name = "sentinel";
    state.aux.arp_state.previous_inversion = 7;
    ps.timeline.stage(std::move(state));

    auto context = ExecutionContext{ps.timeline.get_state().aux};
    auto run_action = [&](std::string const &command) -> CommandActionResult {
        auto const actions = to_command_actions(parse_command_chain(command));
        REQUIRE(actions.size() == 1);
        auto const result = execute_command_action(ps, context, actions.front());
        context = result.context;
        return result;
    };

    auto const first_result = run_action("chord major 0");
    CHECK(first_result.status.first == MessageLevel::Info);
    CHECK(first_result.status.second == "Chorded with major inversion: 0");
    CHECK(first_result.engine_mutated);

    auto const &after_major = ps.timeline.get_state().sequencer.measure.cell.elements;
    REQUIRE(after_major.size() == 4);
    REQUIRE(std::holds_alternative<sequence::Note>(after_major[0]));
    CHECK(std::get<sequence::Note>(after_major[0]).pitch == 10);
    REQUIRE(std::holds_alternative<sequence::Sequence>(after_major[1]));
    auto const &sequence = std::get<sequence::Sequence>(after_major[1]);
    REQUIRE(sequence.cells.size() == 1);
    REQUIRE(sequence.cells[0].elements.size() == 1);
    REQUIRE(std::holds_alternative<sequence::Note>(sequence.cells[0].elements[0]));
    CHECK(std::get<sequence::Note>(sequence.cells[0].elements[0]).pitch == 14);
    REQUIRE(std::holds_alternative<sequence::Note>(after_major[2]));
    CHECK(std::get<sequence::Note>(after_major[2]).pitch == 17);
    REQUIRE(std::holds_alternative<sequence::Note>(after_major[3]));
    CHECK(std::get<sequence::Note>(after_major[3]).pitch == 22);

    auto const repeat_result = run_action("chord");
    CHECK(repeat_result.status.first == MessageLevel::Info);
    CHECK(repeat_result.status.second == "Chorded with major inversion: 1");

    auto const &after_repeat = ps.timeline.get_state().sequencer.measure.cell.elements;
    REQUIRE(std::holds_alternative<sequence::Note>(after_repeat[0]));
    CHECK(std::get<sequence::Note>(after_repeat[0]).pitch == 14);
    REQUIRE(std::holds_alternative<sequence::Sequence>(after_repeat[1]));
    auto const &repeated_sequence = std::get<sequence::Sequence>(after_repeat[1]);
    REQUIRE(repeated_sequence.cells[0].elements.size() == 1);
    CHECK(std::get<sequence::Note>(repeated_sequence.cells[0].elements[0]).pitch == 17);
    REQUIRE(std::holds_alternative<sequence::Note>(after_repeat[2]));
    CHECK(std::get<sequence::Note>(after_repeat[2]).pitch == 22);
    REQUIRE(std::holds_alternative<sequence::Note>(after_repeat[3]));
    CHECK(std::get<sequence::Note>(after_repeat[3]).pitch == 26);

    auto const cycle_result = run_action("chord cycle 0");
    CHECK(cycle_result.status.first == MessageLevel::Info);
    CHECK(cycle_result.status.second == "Chorded with minor inversion: 0");

    auto const &after_cycle = ps.timeline.get_state().sequencer.measure.cell.elements;
    REQUIRE(std::holds_alternative<sequence::Note>(after_cycle[0]));
    CHECK(std::get<sequence::Note>(after_cycle[0]).pitch == 10);
    REQUIRE(std::holds_alternative<sequence::Sequence>(after_cycle[1]));
    auto const &cycled_sequence = std::get<sequence::Sequence>(after_cycle[1]);
    REQUIRE(cycled_sequence.cells[0].elements.size() == 1);
    CHECK(std::get<sequence::Note>(cycled_sequence.cells[0].elements[0]).pitch == 13);
    REQUIRE(std::holds_alternative<sequence::Note>(after_cycle[2]));
    CHECK(std::get<sequence::Note>(after_cycle[2]).pitch == 17);
    REQUIRE(std::holds_alternative<sequence::Note>(after_cycle[3]));
    CHECK(std::get<sequence::Note>(after_cycle[3]).pitch == 22);

    auto const &aux = ps.timeline.get_state().aux;
    CHECK(aux.arp_state.previous_chord_name == "sentinel");
    CHECK(aux.arp_state.previous_inversion == 7);
    CHECK(aux.chord_state.previous_chord_name == "minor");
    CHECK(aux.chord_state.previous_inversion == 0);
}

TEST_CASE("Typed chord action requires a whole-cell selection",
          "[core][command][action]")
{
    auto ps = make_plugin_state();
    ps.library.chords = {
        Chord{.name = "major", .intervals = {0, 4, 7}},
    };

    auto state = ps.timeline.get_state();
    state.sequencer.measure.cell.elements = {
        sequence::Note{10, 0.5f, 0.1f, 0.8f},
        sequence::Note{10, 0.5f, 0.1f, 0.8f},
    };
    state.aux.selected = select_element_in_cell({}, 0);
    ps.timeline.stage(std::move(state));

    auto const context = ExecutionContext{ps.timeline.get_state().aux};
    auto const action = to_command_actions(parse_command_chain("chord major 0"))[0];
    CHECK_THROWS_AS(execute_command_action(ps, context, action), std::runtime_error);
}

TEST_CASE("Typed note and delete actions update selected cell",
          "[core][command][action]")
{
    auto ps = make_plugin_state();
    auto state = ps.timeline.get_state();
    state.aux.selected = singleton_sequence_cell_selection({0});
    state.sequencer.measure.cell = {
        .elements = {sequence::Sequence{
            .cells =
                {
                    sequence::Cell{
                        .elements = {sequence::Note{3, 0.2f, 0.1f, 0.4f}},
                        .weight = 0.37f,
                    },
                },
        }},
        .weight = 1.f,
    };
    ps.timeline.stage(std::move(state));

    auto context = ExecutionContext{ps.timeline.get_state().aux};
    auto const note_target = context.selected;

    auto note_action =
        to_command_actions(parse_command_chain("note 12 0.5 0.25 0.75"))[0];
    auto note_result = execute_command_action(ps, context, note_action);
    CHECK(note_result.status.first == MessageLevel::Info);
    CHECK(note_result.status.second == "Note Created");
    CHECK(note_result.engine_mutated);

    auto const state_after_note = ps.timeline.get_state();
    auto const &note_cell =
        get_selected_cell_const(state_after_note.sequencer.measure, note_target);
    REQUIRE(note_cell.elements.size() == 2);
    REQUIRE(std::holds_alternative<sequence::Note>(note_cell.elements.back()));
    auto const &note = std::get<sequence::Note>(note_cell.elements.back());
    CHECK(note.pitch == 12);
    CHECK(note.velocity == Catch::Approx(0.5f));
    CHECK(note.delay == Catch::Approx(0.25f));
    CHECK(note.gate == Catch::Approx(0.75f));
    CHECK(note_cell.weight == Catch::Approx(0.37f));

    auto delete_action = to_command_actions(parse_command_chain("delete"))[0];
    auto delete_result = execute_command_action(ps, note_result.context, delete_action);
    CHECK(delete_result.status.first == MessageLevel::Info);
    CHECK(delete_result.status.second == "Deleted Selection");
    CHECK(delete_result.engine_mutated);

    auto const state_after_delete = ps.timeline.get_state();
    auto const &deleted_cell = get_selected_cell_const(
        state_after_delete.sequencer.measure, note_result.context.selected);
    CHECK(deleted_cell.elements.empty());
    CHECK(deleted_cell.weight == Catch::Approx(0.37f));
}

TEST_CASE("Typed delete action on the last selected element returns to the parent cell",
          "[core][command][action]")
{
    auto ps = make_plugin_state();
    auto state = ps.timeline.get_state();
    state.sequencer.measure.cell.elements = {
        sequence::Note{1, 0.2f, 0.1f, 0.3f},
    };
    state.aux.selected = select_element_in_cell({}, 0);
    ps.timeline.stage(std::move(state));

    auto const context = ExecutionContext{ps.timeline.get_state().aux};
    auto delete_action = to_command_actions(parse_command_chain("delete"))[0];
    auto delete_result = execute_command_action(ps, context, delete_action);
    CHECK(delete_result.status.first == MessageLevel::Info);
    CHECK(delete_result.status.second == "Deleted Selection");
    CHECK(delete_result.engine_mutated);
    CHECK(delete_result.context.selected == SelectedState{});

    auto const &root_cell = ps.timeline.get_state().sequencer.measure.cell;
    CHECK(root_cell.elements.empty());
}

TEST_CASE("Typed note action replaces selected element only", "[core][command][action]")
{
    auto ps = make_plugin_state();
    auto state = ps.timeline.get_state();
    state.sequencer.measure.cell.elements = {
        sequence::Note{1, 0.2f, 0.1f, 0.3f},
        sequence::Sequence{.cells = {sequence::Cell{.elements = {}, .weight = 1.f}}},
    };
    state.aux.selected = select_element_in_cell({}, 1);
    ps.timeline.stage(std::move(state));

    auto const context = ExecutionContext{ps.timeline.get_state().aux};
    auto note_action = to_command_actions(parse_command_chain("note 9 0.6 0.2 0.8"))[0];
    auto note_result = execute_command_action(ps, context, note_action);
    CHECK(note_result.status.first == MessageLevel::Info);
    CHECK(note_result.status.second == "Note Created");
    CHECK(note_result.engine_mutated);

    auto const &root_cell = ps.timeline.get_state().sequencer.measure.cell;
    REQUIRE(root_cell.elements.size() == 2);
    REQUIRE(std::holds_alternative<sequence::Note>(root_cell.elements.front()));
    CHECK(std::get<sequence::Note>(root_cell.elements.front()).pitch == 1);
    REQUIRE(std::holds_alternative<sequence::Note>(root_cell.elements.at(1)));
    auto const &note = std::get<sequence::Note>(root_cell.elements.at(1));
    CHECK(note.pitch == 9);
    CHECK(note.velocity == Catch::Approx(0.6f));
    CHECK(note.delay == Catch::Approx(0.2f));
    CHECK(note.gate == Catch::Approx(0.8f));
    CHECK(note_result.context.selected == select_element_in_cell({}, 1));
}

TEST_CASE("Typed undo and redo actions restore committed history",
          "[core][command][action]")
{
    auto ps = make_plugin_state();

    auto state = ps.timeline.get_state();
    state.sequencer.key = 1;
    ps.timeline.stage(std::move(state));
    ps.timeline.commit();

    state = ps.timeline.get_state();
    state.sequencer.key = 2;
    ps.timeline.stage(std::move(state));
    ps.timeline.commit();

    auto context = ExecutionContext{};
    context.selected = singleton_sequence_cell_selection({0});
    context.input_mode = InputMode::Gate;

    auto undo_action = to_command_actions(parse_command_chain("undo"))[0];
    auto undo_result = execute_command_action(ps, context, undo_action);
    CHECK(undo_result.status.first == MessageLevel::Info);
    CHECK(undo_result.status.second == "Undone");
    CHECK(undo_result.engine_mutated);
    CHECK(ps.timeline.get_state().sequencer.key == 1);

    auto redo_action = to_command_actions(parse_command_chain("redo"))[0];
    auto redo_result = execute_command_action(ps, undo_result.context, redo_action);
    CHECK(redo_result.status.first == MessageLevel::Info);
    CHECK(redo_result.status.second == "Redone");
    CHECK(redo_result.engine_mutated);
    CHECK(ps.timeline.get_state().sequencer.key == 2);
}

TEST_CASE("Typed scale and deprecated/library commands execute via catalog",
          "[core][command][action]")
{
    auto ps = make_plugin_state();
    ps.library.scales = {
        Scale{.name = "major",
              .tuning_length = 12,
              .intervals = {2, 2, 1, 2, 2, 2, 1},
              .mode = 1},
        Scale{.name = "minor",
              .tuning_length = 12,
              .intervals = {2, 1, 2, 2, 1, 2, 2},
              .mode = 1},
    };

    auto context = ExecutionContext{};
    auto run_action = [&](std::string const &command) -> CommandActionResult {
        auto const actions = to_command_actions(parse_command_chain(command));
        REQUIRE(actions.size() == 1);
        auto const result = execute_command_action(ps, context, actions.front());
        context = result.context;
        return result;
    };

    CHECK(run_action("set scale major").status.first == MessageLevel::Info);
    CHECK(run_action("set mode 2").status.first == MessageLevel::Info);
    CHECK(run_action("shift scale").status.first == MessageLevel::Info);
    CHECK(run_action("shift scaleMode -1").status.first == MessageLevel::Info);
    CHECK(run_action("set translateDirection down").status.first == MessageLevel::Info);
    CHECK(run_action("shift translateDirection").status.first == MessageLevel::Info);
    CHECK(run_action("shift entireScale -1").status.first == MessageLevel::Info);
    CHECK(run_action("load keys").status.first == MessageLevel::Warning);
    CHECK(run_action("libraryDirectory").status.first == MessageLevel::Info);
}
