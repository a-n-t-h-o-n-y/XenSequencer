#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <sequence/pattern.hpp>

#include <xen/message_level.hpp>
#include <xen/modulator.hpp>
#include <xen/state.hpp>

namespace xen
{

enum class MoveDirection : std::uint8_t
{
    Left,
    Right,
    Up,
    Down,
};

/**
 * Typed action for selection movement commands.
 */
struct MoveSelectionAction
{
    MoveDirection direction{MoveDirection::Right};
    std::size_t amount{1};
};

/**
 * Typed action for `welcome`.
 */
struct WelcomeAction
{
};

/**
 * Typed action for `version`.
 */
struct VersionAction
{
};

/**
 * Typed action for `reset`.
 */
struct ResetAction
{
};

/**
 * Typed action for `again`.
 */
struct AgainAction
{
};

/**
 * Typed action for `set key`.
 */
struct SetKeyAction
{
    int key{0};
};

/**
 * Typed action for `set sequence name`.
 */
struct SetSequenceNameAction
{
    std::string name{};
    int index{-1};
};

/**
 * Typed action for `set sequence timeSignature`.
 */
struct SetSequenceTimeSignatureAction
{
    sequence::TimeSignature time_signature{4, 4};
    int index{-1};
};

/**
 * Typed action for `select sequence`.
 */
struct SelectSequenceAction
{
    int index{0};
};

/**
 * Typed action for `inputMode`.
 */
struct SetInputModeAction
{
    InputMode mode{InputMode::Pitch};
};

/**
 * Typed action for `load sequenceBank`.
 */
struct LoadSequenceBankAction
{
    std::string filename{};
};

/**
 * Typed action for `load tuning`.
 */
struct LoadTuningAction
{
    std::string filename{};
};

/**
 * Typed action for deprecated `load keys`.
 */
struct LoadKeysAction
{
};

/**
 * Typed action for `load scales`.
 */
struct LoadScalesAction
{
};

/**
 * Typed action for `load chords`.
 */
struct LoadChordsAction
{
};

/**
 * Typed action for `note`.
 */
struct CreateNoteAction
{
    int pitch{0};
    float velocity{100.f / 127.f};
    float delay{0.f};
    float gate{1.f};
};

/**
 * Typed action for `copy`.
 */
struct CopySelectionAction
{
};

/**
 * Typed action for `cut`.
 */
struct CutSelectionAction
{
};

/**
 * Typed action for `paste`.
 */
struct PasteSelectionAction
{
};

/**
 * Typed action for `duplicate`.
 */
struct DuplicateSelectionAction
{
};

/**
 * Typed action for `delete`.
 */
struct DeleteSelectionAction
{
};

/**
 * Typed action for `split`.
 */
struct SplitSelectionAction
{
    std::size_t count{2};
};

/**
 * Typed action for `lift`.
 */
struct LiftSelectionAction
{
};

/**
 * Typed action for `set baseFrequency`.
 */
struct SetBaseFrequencyAction
{
    float freq{440.f};
};

/**
 * Typed action for `save sequenceBank`.
 */
struct SaveSequenceBankAction
{
    std::string filename{};
};

/**
 * Typed action for `libraryDirectory`.
 */
struct LibraryDirectoryAction
{
};

/**
 * Typed action for `set pitch`.
 */
struct SetPitchAction
{
    sequence::Pattern pattern{0, {1}};
    std::variant<int, Modulator> pitch{0};
};

/**
 * Typed action for `set octave`.
 */
struct SetOctaveAction
{
    sequence::Pattern pattern{0, {1}};
    int octave{0};
};

/**
 * Typed action for `set velocity`.
 */
struct SetVelocityAction
{
    sequence::Pattern pattern{0, {1}};
    std::variant<float, Modulator> velocity{100.f / 127.f};
};

/**
 * Typed action for `set delay`.
 */
struct SetDelayAction
{
    sequence::Pattern pattern{0, {1}};
    std::variant<float, Modulator> delay{0.f};
};

/**
 * Typed action for `set gate`.
 */
struct SetGateAction
{
    sequence::Pattern pattern{0, {1}};
    std::variant<float, Modulator> gate{1.f};
};

/**
 * Typed action for `set weight`.
 */
struct SetWeightAction
{
    float value{0.f};
};

/**
 * Typed action for `set weights`.
 */
struct SetWeightsAction
{
    sequence::Pattern pattern{0, {1}};
    std::variant<float, Modulator> weight{0.f};
};

/**
 * Typed action for `commit`.
 */
struct CommitAction
{
};

/**
 * Typed action for `undo`.
 */
struct UndoAction
{
};

/**
 * Typed action for `redo`.
 */
struct RedoAction
{
};

/**
 * Typed action for `set scale`.
 */
struct SetScaleAction
{
    std::string name{};
};

/**
 * Typed action for `set mode`.
 */
struct SetScaleModeAction
{
    std::size_t mode_index{1};
};

/**
 * Typed action for `set translateDirection`.
 */
struct SetTranslateDirectionAction
{
    std::string direction{};
};

/**
 * Typed action for `shift selectedSequence`.
 */
struct ShiftSelectedSequenceAction
{
    int amount{0};
};

/**
 * Typed action for `shift scale`.
 */
struct ShiftScaleAction
{
    int amount{1};
};

/**
 * Typed action for `shift scaleMode`.
 */
struct ShiftScaleModeAction
{
    int amount{1};
};

/**
 * Typed action for `shift translateDirection`.
 */
struct ShiftTranslateDirectionAction
{
};

/**
 * Typed action for `shift entireScale`.
 */
struct ShiftEntireScaleAction
{
    int direction{1};
};

/**
 * Typed action for `double sequence timeSignature`.
 */
struct DoubleSequenceTimeSignatureAction
{
    int index{-1};
};

/**
 * Typed action for `halve sequence timeSignature`.
 */
struct HalveSequenceTimeSignatureAction
{
    int index{-1};
};

/**
 * Typed action for `shift pitch`.
 */
struct ShiftPitchAction
{
    sequence::Pattern pattern{0, {1}};
    int amount{1};
};

/**
 * Typed action for `shift octave`.
 */
struct ShiftOctaveAction
{
    sequence::Pattern pattern{0, {1}};
    int amount{1};
};

/**
 * Typed action for `shift velocity`.
 */
struct ShiftVelocityAction
{
    sequence::Pattern pattern{0, {1}};
    float amount{0.1f};
};

/**
 * Typed action for `shift delay`.
 */
struct ShiftDelayAction
{
    sequence::Pattern pattern{0, {1}};
    float amount{0.1f};
};

/**
 * Typed action for `shift gate`.
 */
struct ShiftGateAction
{
    sequence::Pattern pattern{0, {1}};
    float amount{0.1f};
};

/**
 * Typed action for `randomize pitch`.
 */
struct RandomizePitchAction
{
    sequence::Pattern pattern{0, {1}};
    int min{-12};
    int max{12};
};

/**
 * Typed action for `randomize velocity`.
 */
struct RandomizeVelocityAction
{
    sequence::Pattern pattern{0, {1}};
    float min{0.01f};
    float max{1.f};
};

/**
 * Typed action for `randomize delay`.
 */
struct RandomizeDelayAction
{
    sequence::Pattern pattern{0, {1}};
    float min{0.f};
    float max{0.95f};
};

/**
 * Typed action for `randomize gate`.
 */
struct RandomizeGateAction
{
    sequence::Pattern pattern{0, {1}};
    float min{0.f};
    float max{0.95f};
};

/**
 * Typed action for `stretch`.
 */
struct StretchAction
{
    sequence::Pattern pattern{0, {1}};
    std::size_t count{2};
};

/**
 * Typed action for `compress`.
 */
struct CompressAction
{
    sequence::Pattern pattern{0, {1}};
};

/**
 * Typed action for `shuffle`.
 */
struct ShuffleAction
{
};

/**
 * Typed action for `rotate`.
 */
struct RotateAction
{
    int amount{1};
};

/**
 * Typed action for `reverse`.
 */
struct ReverseAction
{
};

/**
 * Typed action for `mirror`.
 */
struct MirrorAction
{
    sequence::Pattern pattern{0, {1}};
    int center_pitch{0};
};

/**
 * Typed action for `step`.
 */
struct StepAction
{
    sequence::Pattern pattern{0, {1}};
    int pitch_distance{1};
    float velocity_distance{0.f};
};

/**
 * Typed action for `drums`.
 */
struct DrumsAction
{
    std::size_t octave_size{16};
    int offset{1};
};

/**
 * Typed action for `arp`.
 */
struct ArpAction
{
    sequence::Pattern pattern{0, {1}};
    std::string chord{"cycle"};
    int inversion{-1};
};

/**
 * Canonical command action variant.
 */
using CommandAction =
    std::variant<MoveSelectionAction, WelcomeAction, VersionAction, ResetAction,
                 AgainAction,
                 SetKeyAction, SetSequenceNameAction,
                 SetSequenceTimeSignatureAction, SelectSequenceAction,
                 SetInputModeAction, LoadSequenceBankAction, LoadTuningAction,
                 LoadKeysAction, LoadScalesAction, LoadChordsAction, CreateNoteAction,
                 CopySelectionAction, CutSelectionAction,
                 PasteSelectionAction, DuplicateSelectionAction,
                 DeleteSelectionAction, SplitSelectionAction,
                 LiftSelectionAction, SetBaseFrequencyAction,
                 SaveSequenceBankAction, LibraryDirectoryAction, SetPitchAction,
                 SetOctaveAction, SetVelocityAction, SetDelayAction,
                 SetGateAction, SetWeightAction, SetWeightsAction, CommitAction,
                 UndoAction, RedoAction, SetScaleAction, SetScaleModeAction,
                 SetTranslateDirectionAction, ShiftPitchAction,
                 ShiftOctaveAction, ShiftVelocityAction, ShiftDelayAction,
                 ShiftGateAction, ShiftSelectedSequenceAction, ShiftScaleAction,
                 ShiftScaleModeAction, ShiftTranslateDirectionAction,
                 ShiftEntireScaleAction, DoubleSequenceTimeSignatureAction,
                 HalveSequenceTimeSignatureAction, RandomizePitchAction,
                 RandomizeVelocityAction, RandomizeDelayAction,
                 RandomizeGateAction, StretchAction, CompressAction,
                 ShuffleAction, RotateAction, ReverseAction, MirrorAction,
                 StepAction, DrumsAction, ArpAction>;

/**
 * Result of applying one typed command action.
 */
struct CommandActionResult
{
    std::pair<MessageLevel, std::string> status{MessageLevel::Debug, ""};
    ExecutionContext context{};
    bool engine_mutated{false};
    CommitIntent commit_intent{CommitIntent::Auto};
};

/**
 * Check whether an action is a chain-level replay action.
 */
[[nodiscard]] auto is_again_action(CommandAction const &action) -> bool;

/**
 * Apply one command action to plugin state with explicit execution context.
 */
[[nodiscard]] auto execute_command_action(PluginState &ps,
                                          ExecutionContext context,
                                          CommandAction const &action)
    -> CommandActionResult;

} // namespace xen
