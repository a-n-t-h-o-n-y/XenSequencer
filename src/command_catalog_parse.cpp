#include "command_catalog_parse.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <xen/parse_args.hpp>
#include <xen/string_manip.hpp>

namespace xen
{
namespace
{

auto parse_move_direction(std::string const &value) -> std::optional<MoveDirection>
{
    auto const lower = to_lower(value);
    if (lower == "left")
    {
        return MoveDirection::Left;
    }
    if (lower == "right")
    {
        return MoveDirection::Right;
    }
    if (lower == "up")
    {
        return MoveDirection::Up;
    }
    if (lower == "down")
    {
        return MoveDirection::Down;
    }

    return std::nullopt;
}

auto try_convert_to_typed_action(CommandInvocation const &invocation)
    -> std::optional<CommandAction>
{
    auto const &words = invocation.input.words;
    if (words.empty())
    {
        return std::nullopt;
    }

    auto const command_id = to_lower(words[0]);

    if (command_id == "welcome")
    {
        return CommandAction{WelcomeAction{}};
    }

    if (command_id == "version")
    {
        return CommandAction{VersionAction{}};
    }

    if (command_id == "reset")
    {
        return CommandAction{ResetAction{}};
    }

    if (command_id == "again")
    {
        return CommandAction{AgainAction{}};
    }

    if (command_id == "commit")
    {
        return CommandAction{CommitAction{}};
    }

    if (command_id == "undo")
    {
        return CommandAction{UndoAction{}};
    }

    if (command_id == "redo")
    {
        return CommandAction{RedoAction{}};
    }

    if (command_id == "move")
    {
        if (words.size() < 2)
        {
            return std::nullopt;
        }

        auto const direction = parse_move_direction(words[1]);
        if (!direction.has_value())
        {
            return std::nullopt;
        }

        auto amount = std::size_t{1};
        if (words.size() >= 3)
        {
            amount = parse<std::size_t>(words[2]);
        }

        return CommandAction{
            MoveSelectionAction{.direction = *direction, .amount = amount}};
    }

    if (command_id == "select" && words.size() >= 2 &&
        to_lower(words[1]) == "sequence")
    {
        if (words.size() < 3)
        {
            throw std::invalid_argument("Missing argument and no default value");
        }

        return CommandAction{SelectSequenceAction{.index = parse<int>(words[2])}};
    }

    if (command_id == "inputmode")
    {
        if (words.size() < 2)
        {
            throw std::invalid_argument("Missing argument and no default value");
        }

        return CommandAction{
            SetInputModeAction{.mode = parse<InputMode>(words[1])}};
    }

    if (command_id == "load" && words.size() >= 2)
    {
        auto const subcommand = to_lower(words[1]);

        if (subcommand == "sequencebank")
        {
            if (words.size() < 3)
            {
                throw std::invalid_argument("Missing argument and no default value");
            }
            return CommandAction{LoadSequenceBankAction{.filename = words[2]}};
        }

        if (subcommand == "tuning")
        {
            if (words.size() < 3)
            {
                throw std::invalid_argument("Missing argument and no default value");
            }
            return CommandAction{LoadTuningAction{.filename = words[2]}};
        }

        if (subcommand == "keys")
        {
            return CommandAction{LoadKeysAction{}};
        }

        if (subcommand == "scales")
        {
            return CommandAction{LoadScalesAction{}};
        }

        if (subcommand == "chords")
        {
            return CommandAction{LoadChordsAction{}};
        }
    }

    if (command_id == "focus")
    {
        if (words.size() < 2)
        {
            throw std::invalid_argument("Missing argument and no default value");
        }
        return CommandAction{DeprecatedFocusAction{.component_id = words[1]}};
    }

    if (command_id == "show")
    {
        if (words.size() < 2)
        {
            throw std::invalid_argument("Missing argument and no default value");
        }
        return CommandAction{DeprecatedShowAction{.component_id = words[1]}};
    }

    if (command_id == "save" && words.size() >= 2)
    {
        auto const subcommand = to_lower(words[1]);
        if (subcommand == "sequencebank")
        {
            if (words.size() < 3)
            {
                throw std::invalid_argument("Missing argument and no default value");
            }
            return CommandAction{SaveSequenceBankAction{.filename = words[2]}};
        }
    }

    if (command_id == "librarydirectory")
    {
        return CommandAction{LibraryDirectoryAction{}};
    }

    if (command_id == "note")
    {
        auto pitch = 0;
        auto velocity = 100.f / 127.f;
        auto delay = 0.f;
        auto gate = 1.f;

        if (words.size() >= 2)
        {
            pitch = parse<int>(words[1]);
        }
        if (words.size() >= 3)
        {
            velocity = parse<float>(words[2]);
        }
        if (words.size() >= 4)
        {
            delay = parse<float>(words[3]);
        }
        if (words.size() >= 5)
        {
            gate = parse<float>(words[4]);
        }

        return CommandAction{
            CreateNoteAction{
                .pitch = pitch,
                .velocity = velocity,
                .delay = delay,
                .gate = gate,
            }};
    }

    if (command_id == "rest")
    {
        return CommandAction{CreateRestAction{}};
    }

    if (command_id == "copy")
    {
        return CommandAction{CopySelectionAction{}};
    }

    if (command_id == "cut")
    {
        return CommandAction{CutSelectionAction{}};
    }

    if (command_id == "paste")
    {
        return CommandAction{PasteSelectionAction{}};
    }

    if (command_id == "duplicate")
    {
        return CommandAction{DuplicateSelectionAction{}};
    }

    if (command_id == "delete")
    {
        return CommandAction{DeleteSelectionAction{}};
    }

    if (command_id == "split")
    {
        auto count = std::size_t{2};
        if (words.size() >= 2)
        {
            count = parse<std::size_t>(words[1]);
        }

        return CommandAction{SplitSelectionAction{.count = count}};
    }

    if (command_id == "lift")
    {
        return CommandAction{LiftSelectionAction{}};
    }

    if (command_id == "flip")
    {
        return CommandAction{
            FlipSelectionAction{.pattern = invocation.input.pattern}};
    }

    if (command_id == "fill" && words.size() >= 2)
    {
        auto const subcommand = to_lower(words[1]);
        auto const pattern = invocation.input.pattern;

        if (subcommand == "note")
        {
            auto pitch = 0;
            auto velocity = 100.f / 127.f;
            auto delay = 0.f;
            auto gate = 1.f;

            if (words.size() >= 3)
            {
                pitch = parse<int>(words[2]);
            }
            if (words.size() >= 4)
            {
                velocity = parse<float>(words[3]);
            }
            if (words.size() >= 5)
            {
                delay = parse<float>(words[4]);
            }
            if (words.size() >= 6)
            {
                gate = parse<float>(words[5]);
            }

            return CommandAction{FillNoteAction{
                .pattern = pattern,
                .pitch = pitch,
                .velocity = velocity,
                .delay = delay,
                .gate = gate,
            }};
        }

        if (subcommand == "rest")
        {
            return CommandAction{FillRestAction{.pattern = pattern}};
        }
    }

    if (command_id == "set" && words.size() >= 2)
    {
        auto const subcommand = to_lower(words[1]);

        if (subcommand == "key")
        {
            auto key = 0;
            if (words.size() >= 3)
            {
                key = parse<int>(words[2]);
            }

            return CommandAction{SetKeyAction{.key = key}};
        }

        if (subcommand == "sequence" && words.size() >= 3 &&
            to_lower(words[2]) == "name")
        {
            if (words.size() < 4)
            {
                throw std::invalid_argument("Missing argument and no default value");
            }

            auto const name = words[3];
            auto index = -1;
            if (words.size() >= 5)
            {
                index = parse<int>(words[4]);
            }

            return CommandAction{
                SetSequenceNameAction{.name = name, .index = index}};
        }

        if (subcommand == "sequence" && words.size() >= 3 &&
            to_lower(words[2]) == "timesignature")
        {
            auto ts = sequence::TimeSignature{4, 4};
            auto index = -1;
            if (words.size() >= 4)
            {
                ts = parse<sequence::TimeSignature>(words[3]);
            }
            if (words.size() >= 5)
            {
                index = parse<int>(words[4]);
            }

            return CommandAction{SetSequenceTimeSignatureAction{
                .time_signature = ts,
                .index = index,
            }};
        }

        if (subcommand == "basefrequency")
        {
            auto freq = 440.f;
            if (words.size() >= 3)
            {
                freq = parse<float>(words[2]);
            }

            return CommandAction{SetBaseFrequencyAction{.freq = freq}};
        }

        if (subcommand == "theme")
        {
            if (words.size() < 3)
            {
                throw std::invalid_argument("Missing argument and no default value");
            }

            return CommandAction{SetThemeAction{.name = words[2]}};
        }

        if (subcommand == "pitch")
        {
            auto pitch = std::variant<int, Modulator>{0};
            if (words.size() >= 3)
            {
                pitch = parse<std::variant<int, Modulator>>(words[2]);
            }

            return CommandAction{
                SetPitchAction{.pattern = invocation.input.pattern, .pitch = pitch}};
        }

        if (subcommand == "octave")
        {
            auto octave = 0;
            if (words.size() >= 3)
            {
                octave = parse<int>(words[2]);
            }

            return CommandAction{SetOctaveAction{
                .pattern = invocation.input.pattern, .octave = octave}};
        }

        if (subcommand == "velocity")
        {
            auto velocity = std::variant<float, Modulator>{100.f / 127.f};
            if (words.size() >= 3)
            {
                velocity = parse<std::variant<float, Modulator>>(words[2]);
            }

            return CommandAction{SetVelocityAction{
                .pattern = invocation.input.pattern, .velocity = velocity}};
        }

        if (subcommand == "delay")
        {
            auto delay = std::variant<float, Modulator>{0.f};
            if (words.size() >= 3)
            {
                delay = parse<std::variant<float, Modulator>>(words[2]);
            }

            return CommandAction{
                SetDelayAction{.pattern = invocation.input.pattern, .delay = delay}};
        }

        if (subcommand == "gate")
        {
            auto gate = std::variant<float, Modulator>{1.f};
            if (words.size() >= 3)
            {
                gate = parse<std::variant<float, Modulator>>(words[2]);
            }

            return CommandAction{
                SetGateAction{.pattern = invocation.input.pattern, .gate = gate}};
        }

        if (subcommand == "weight")
        {
            if (words.size() < 3)
            {
                throw std::invalid_argument("Missing argument and no default value");
            }

            return CommandAction{
                SetWeightAction{.value = parse<float>(words[2])}};
        }

        if (subcommand == "weights")
        {
            if (words.size() < 3)
            {
                throw std::invalid_argument("Missing argument and no default value");
            }

            return CommandAction{SetWeightsAction{
                .pattern = invocation.input.pattern,
                .weight = parse<std::variant<float, Modulator>>(words[2]),
            }};
        }

        if (subcommand == "scale")
        {
            if (words.size() < 3)
            {
                throw std::invalid_argument("Missing argument and no default value");
            }

            return CommandAction{SetScaleAction{.name = words[2]}};
        }

        if (subcommand == "mode")
        {
            if (words.size() < 3)
            {
                throw std::invalid_argument("Missing argument and no default value");
            }

            return CommandAction{
                SetScaleModeAction{.mode_index = parse<std::size_t>(words[2])}};
        }

        if (subcommand == "translatedirection")
        {
            if (words.size() < 3)
            {
                throw std::invalid_argument("Missing argument and no default value");
            }

            return CommandAction{
                SetTranslateDirectionAction{.direction = words[2]}};
        }
    }

    if (command_id == "shift" && words.size() >= 2)
    {
        auto const subcommand = to_lower(words[1]);

        if (subcommand == "pitch")
        {
            auto amount = 1;
            if (words.size() >= 3)
            {
                amount = parse<int>(words[2]);
            }
            return CommandAction{ShiftPitchAction{
                .pattern = invocation.input.pattern, .amount = amount}};
        }

        if (subcommand == "octave")
        {
            auto amount = 1;
            if (words.size() >= 3)
            {
                amount = parse<int>(words[2]);
            }
            return CommandAction{ShiftOctaveAction{
                .pattern = invocation.input.pattern, .amount = amount}};
        }

        if (subcommand == "velocity")
        {
            auto amount = 0.1f;
            if (words.size() >= 3)
            {
                amount = parse<float>(words[2]);
            }
            return CommandAction{ShiftVelocityAction{
                .pattern = invocation.input.pattern, .amount = amount}};
        }

        if (subcommand == "delay")
        {
            auto amount = 0.1f;
            if (words.size() >= 3)
            {
                amount = parse<float>(words[2]);
            }
            return CommandAction{ShiftDelayAction{
                .pattern = invocation.input.pattern, .amount = amount}};
        }

        if (subcommand == "gate")
        {
            auto amount = 0.1f;
            if (words.size() >= 3)
            {
                amount = parse<float>(words[2]);
            }
            return CommandAction{ShiftGateAction{
                .pattern = invocation.input.pattern, .amount = amount}};
        }

        if (subcommand == "selectedsequence")
        {
            if (words.size() < 3)
            {
                throw std::invalid_argument("Missing argument and no default value");
            }

            return CommandAction{
                ShiftSelectedSequenceAction{.amount = parse<int>(words[2])}};
        }

        if (subcommand == "scale")
        {
            auto amount = 1;
            if (words.size() >= 3)
            {
                amount = parse<int>(words[2]);
            }

            return CommandAction{ShiftScaleAction{.amount = amount}};
        }

        if (subcommand == "scalemode")
        {
            auto amount = 1;
            if (words.size() >= 3)
            {
                amount = parse<int>(words[2]);
            }

            return CommandAction{ShiftScaleModeAction{.amount = amount}};
        }

        if (subcommand == "translatedirection")
        {
            return CommandAction{ShiftTranslateDirectionAction{}};
        }

        if (subcommand == "entirescale")
        {
            auto direction = 1;
            if (words.size() >= 3)
            {
                direction = parse<int>(words[2]);
            }

            return CommandAction{ShiftEntireScaleAction{.direction = direction}};
        }
    }

    if (command_id == "randomize" && words.size() >= 2)
    {
        auto const subcommand = to_lower(words[1]);
        auto const pattern = invocation.input.pattern;

        if (subcommand == "pitch")
        {
            auto min = -12;
            auto max = 12;
            if (words.size() >= 3)
            {
                min = parse<int>(words[2]);
            }
            if (words.size() >= 4)
            {
                max = parse<int>(words[3]);
            }
            return CommandAction{
                RandomizePitchAction{.pattern = pattern, .min = min, .max = max}};
        }

        if (subcommand == "velocity")
        {
            auto min = 0.01f;
            auto max = 1.f;
            if (words.size() >= 3)
            {
                min = parse<float>(words[2]);
            }
            if (words.size() >= 4)
            {
                max = parse<float>(words[3]);
            }
            return CommandAction{
                RandomizeVelocityAction{.pattern = pattern, .min = min, .max = max}};
        }

        if (subcommand == "delay")
        {
            auto min = 0.f;
            auto max = 0.95f;
            if (words.size() >= 3)
            {
                min = parse<float>(words[2]);
            }
            if (words.size() >= 4)
            {
                max = parse<float>(words[3]);
            }
            return CommandAction{
                RandomizeDelayAction{.pattern = pattern, .min = min, .max = max}};
        }

        if (subcommand == "gate")
        {
            auto min = 0.f;
            auto max = 0.95f;
            if (words.size() >= 3)
            {
                min = parse<float>(words[2]);
            }
            if (words.size() >= 4)
            {
                max = parse<float>(words[3]);
            }
            return CommandAction{
                RandomizeGateAction{.pattern = pattern, .min = min, .max = max}};
        }
    }

    if (command_id == "stretch")
    {
        auto count = std::size_t{2};
        if (words.size() >= 2)
        {
            count = parse<std::size_t>(words[1]);
        }
        return CommandAction{
            StretchAction{.pattern = invocation.input.pattern, .count = count}};
    }

    if (command_id == "compress")
    {
        return CommandAction{CompressAction{.pattern = invocation.input.pattern}};
    }

    if (command_id == "shuffle")
    {
        return CommandAction{ShuffleAction{}};
    }

    if (command_id == "rotate")
    {
        auto amount = 1;
        if (words.size() >= 2)
        {
            amount = parse<int>(words[1]);
        }
        return CommandAction{RotateAction{.amount = amount}};
    }

    if (command_id == "reverse")
    {
        return CommandAction{ReverseAction{}};
    }

    if (command_id == "mirror")
    {
        auto center_pitch = 0;
        if (words.size() >= 2)
        {
            center_pitch = parse<int>(words[1]);
        }
        return CommandAction{MirrorAction{
            .pattern = invocation.input.pattern, .center_pitch = center_pitch}};
    }

    if (command_id == "quantize")
    {
        return CommandAction{QuantizeAction{.pattern = invocation.input.pattern}};
    }

    if (command_id == "swing")
    {
        auto amount = 0.1f;
        if (words.size() >= 2)
        {
            amount = parse<float>(words[1]);
        }
        return CommandAction{SwingAction{.amount = amount}};
    }

    if (command_id == "step")
    {
        auto pitch_distance = 1;
        auto velocity_distance = 0.f;
        if (words.size() >= 2)
        {
            pitch_distance = parse<int>(words[1]);
        }
        if (words.size() >= 3)
        {
            velocity_distance = parse<float>(words[2]);
        }
        return CommandAction{StepAction{
            .pattern = invocation.input.pattern,
            .pitch_distance = pitch_distance,
            .velocity_distance = velocity_distance,
        }};
    }

    if (command_id == "drums")
    {
        auto octave_size = std::size_t{16};
        auto offset = 1;
        if (words.size() >= 2)
        {
            octave_size = parse<std::size_t>(words[1]);
        }
        if (words.size() >= 3)
        {
            offset = parse<int>(words[2]);
        }
        return CommandAction{
            DrumsAction{.octave_size = octave_size, .offset = offset}};
    }

    if (command_id == "arp")
    {
        auto chord = std::string{"cycle"};
        auto inversion = -1;
        if (words.size() >= 2)
        {
            chord = words[1];
        }
        if (words.size() >= 3)
        {
            inversion = parse<int>(words[2]);
        }

        return CommandAction{ArpAction{
            .pattern = invocation.input.pattern,
            .chord = chord,
            .inversion = inversion,
        }};
    }

    if (command_id == "double" && words.size() >= 3 &&
        to_lower(words[1]) == "sequence" &&
        to_lower(words[2]) == "timesignature")
    {
        auto index = -1;
        if (words.size() >= 4)
        {
            index = parse<int>(words[3]);
        }

        return CommandAction{
            DoubleSequenceTimeSignatureAction{.index = index}};
    }

    if (command_id == "halve" && words.size() >= 3 &&
        to_lower(words[1]) == "sequence" &&
        to_lower(words[2]) == "timesignature")
    {
        auto index = -1;
        if (words.size() >= 4)
        {
            index = parse<int>(words[3]);
        }

        return CommandAction{HalveSequenceTimeSignatureAction{.index = index}};
    }

    return std::nullopt;
}

} // namespace

auto try_to_command_action(CommandInvocation const &invocation)
    -> std::optional<CommandAction>
{
    return try_convert_to_typed_action(invocation);
}

} // namespace xen
