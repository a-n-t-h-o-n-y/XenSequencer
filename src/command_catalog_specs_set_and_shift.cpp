#include "command_catalog_spec_builder.hpp"
#include "command_catalog_specs_internal.hpp"

namespace xen::catalog_detail
{

void append_set_and_shift_specs(std::vector<CommandSpec> &specs)
{
    specs.push_back(make_spec(
        {"set", "pitch"}, true, "Set selected note pitches.",
        std::make_tuple(optional_arg<std::variant<int, Modulator>>(
            "Int|Modulator", "pitch", std::variant<int, Modulator>{0})),
        [](CommandInvocation const &invocation,
           std::variant<int, Modulator> pitch) {
            return SetPitchAction{
                .pattern = invocation.input.pattern,
                .pitch = std::move(pitch),
            };
        }));

    specs.push_back(make_spec(
        {"set", "octave"}, true, "Set selected note octaves.",
        std::make_tuple(optional_arg<int>("Int", "octave", 0)),
        [](CommandInvocation const &invocation, int octave) {
            return SetOctaveAction{
                .pattern = invocation.input.pattern,
                .octave = octave,
            };
        }));

    specs.push_back(make_spec(
        {"set", "velocity"}, true, "Set selected note velocities.",
        std::make_tuple(optional_arg<std::variant<float, Modulator>>(
            "Float|Modulator", "velocity",
            std::variant<float, Modulator>{100.f / 127.f})),
        [](CommandInvocation const &invocation,
           std::variant<float, Modulator> velocity) {
            return SetVelocityAction{
                .pattern = invocation.input.pattern,
                .velocity = std::move(velocity),
            };
        }));

    specs.push_back(make_spec(
        {"set", "delay"}, true, "Set selected note delays.",
        std::make_tuple(optional_arg<std::variant<float, Modulator>>(
            "Float|Modulator", "delay", std::variant<float, Modulator>{0.f})),
        [](CommandInvocation const &invocation,
           std::variant<float, Modulator> delay) {
            return SetDelayAction{
                .pattern = invocation.input.pattern,
                .delay = std::move(delay),
            };
        }));

    specs.push_back(make_spec(
        {"set", "gate"}, true, "Set selected note gates.",
        std::make_tuple(optional_arg<std::variant<float, Modulator>>(
            "Float|Modulator", "gate", std::variant<float, Modulator>{1.f})),
        [](CommandInvocation const &invocation,
           std::variant<float, Modulator> gate) {
            return SetGateAction{
                .pattern = invocation.input.pattern,
                .gate = std::move(gate),
            };
        }));

    specs.push_back(make_spec(
        {"set", "measure", "timeSignature"}, false,
        "Set measure time signature.",
        std::make_tuple(optional_arg<sequence::TimeSignature>(
            "TimeSignature", "timesignature", sequence::TimeSignature{4, 4})),
        [](CommandInvocation const &, sequence::TimeSignature timesignature) {
            return SetMeasureTimeSignatureAction{
                .time_signature = timesignature,
            };
        }));

    specs.push_back(make_spec(
        {"set", "baseFrequency"}, false, "Set base frequency in Hz.",
        std::make_tuple(optional_arg<float>("Float", "freq", 440.f)),
        [](CommandInvocation const &, float freq) {
            return SetBaseFrequencyAction{.freq = freq};
        }));

    specs.push_back(make_spec(
        {"set", "scale"}, false, "Set the active scale by name.",
        std::make_tuple(required_arg<std::string>("String", "name")),
        [](CommandInvocation const &, std::string name) {
            return SetScaleAction{.name = std::move(name)};
        }));

    specs.push_back(make_spec(
        {"set", "mode"}, false, "Set the active scale mode index.",
        std::make_tuple(required_arg<std::size_t>("Unsigned", "mode_index")),
        [](CommandInvocation const &, std::size_t mode_index) {
            return SetScaleModeAction{.mode_index = mode_index};
        }));

    specs.push_back(make_spec(
        {"set", "translateDirection"}, false, "Set scale translate direction.",
        std::make_tuple(required_arg<std::string>("String", "direction")),
        [](CommandInvocation const &, std::string direction) {
            return SetTranslateDirectionAction{.direction = std::move(direction)};
        }));

    specs.push_back(make_spec(
        {"set", "key"}, false, "Set transposition key.",
        std::make_tuple(optional_arg<int>("Int", "key", 0)),
        [](CommandInvocation const &, int key) { return SetKeyAction{.key = key}; }));

    specs.push_back(make_spec(
        {"set", "weight"}, false, "Set selected cell weight.",
        std::make_tuple(required_arg<float>("Float", "value")),
        [](CommandInvocation const &, float value) {
            return SetWeightAction{.value = value};
        }));

    specs.push_back(make_spec(
        {"set", "weights"}, true, "Set child weights in selected cell.",
        std::make_tuple(required_arg<std::variant<float, Modulator>>(
            "Float|Modulator", "weight")),
        [](CommandInvocation const &invocation,
           std::variant<float, Modulator> weight) {
            return SetWeightsAction{
                .pattern = invocation.input.pattern,
                .weight = std::move(weight),
            };
        }));

    specs.push_back(make_spec(
        {"double", "measure", "timeSignature"}, false,
        "Double measure time signature.",
        std::make_tuple(), [](CommandInvocation const &) {
            return DoubleMeasureTimeSignatureAction{};
        }));

    specs.push_back(make_spec(
        {"halve", "measure", "timeSignature"}, false,
        "Halve measure time signature.",
        std::make_tuple(), [](CommandInvocation const &) {
            return HalveMeasureTimeSignatureAction{};
        }));

    specs.push_back(make_spec(
        {"shift", "pitch"}, true, "Shift selected note pitches.",
        std::make_tuple(optional_arg<int>("Int", "amount", 1)),
        [](CommandInvocation const &invocation, int amount) {
            return ShiftPitchAction{
                .pattern = invocation.input.pattern,
                .amount = amount,
            };
        }));

    specs.push_back(make_spec(
        {"shift", "octave"}, true, "Shift selected note octaves.",
        std::make_tuple(optional_arg<int>("Int", "amount", 1)),
        [](CommandInvocation const &invocation, int amount) {
            return ShiftOctaveAction{
                .pattern = invocation.input.pattern,
                .amount = amount,
            };
        }));

    specs.push_back(make_spec(
        {"shift", "velocity"}, true, "Shift selected note velocities.",
        std::make_tuple(optional_arg<float>("Float", "amount", 0.1f)),
        [](CommandInvocation const &invocation, float amount) {
            return ShiftVelocityAction{
                .pattern = invocation.input.pattern,
                .amount = amount,
            };
        }));

    specs.push_back(make_spec(
        {"shift", "delay"}, true, "Shift selected note delays.",
        std::make_tuple(optional_arg<float>("Float", "amount", 0.1f)),
        [](CommandInvocation const &invocation, float amount) {
            return ShiftDelayAction{
                .pattern = invocation.input.pattern,
                .amount = amount,
            };
        }));

    specs.push_back(make_spec(
        {"shift", "gate"}, true, "Shift selected note gates.",
        std::make_tuple(optional_arg<float>("Float", "amount", 0.1f)),
        [](CommandInvocation const &invocation, float amount) {
            return ShiftGateAction{
                .pattern = invocation.input.pattern,
                .amount = amount,
            };
        }));

    specs.push_back(make_spec(
        {"shift", "scale"}, false, "Shift loaded scale index.",
        std::make_tuple(optional_arg<int>("Int", "amount", 1)),
        [](CommandInvocation const &, int amount) {
            return ShiftScaleAction{.amount = amount};
        }));

    specs.push_back(make_spec(
        {"shift", "scaleMode"}, false, "Shift scale mode.",
        std::make_tuple(optional_arg<int>("Int", "amount", 1)),
        [](CommandInvocation const &, int amount) {
            return ShiftScaleModeAction{.amount = amount};
        }));

    specs.push_back(make_spec(
        {"shift", "translateDirection"}, false, "Flip translate direction.",
        std::make_tuple(),
        [](CommandInvocation const &) { return ShiftTranslateDirectionAction{}; }));

    specs.push_back(make_spec(
        {"shift", "entireScale"}, false,
        "Shift direction, mode, and scale together.",
        std::make_tuple(optional_arg<int>("Int", "direction", 1)),
        [](CommandInvocation const &, int direction) {
            return ShiftEntireScaleAction{.direction = direction};
        }));

    specs.push_back(make_spec(
        {"randomize", "pitch"}, true, "Randomize note pitches.",
        std::make_tuple(optional_arg<int>("Int", "min", -12),
                        optional_arg<int>("Int", "max", 12)),
        [](CommandInvocation const &invocation, int min, int max) {
            return RandomizePitchAction{
                .pattern = invocation.input.pattern,
                .min = min,
                .max = max,
            };
        }));

    specs.push_back(make_spec(
        {"randomize", "velocity"}, true, "Randomize note velocities.",
        std::make_tuple(optional_arg<float>("Float", "min", 0.01f),
                        optional_arg<float>("Float", "max", 1.f)),
        [](CommandInvocation const &invocation, float min, float max) {
            return RandomizeVelocityAction{
                .pattern = invocation.input.pattern,
                .min = min,
                .max = max,
            };
        }));

    specs.push_back(make_spec(
        {"randomize", "delay"}, true, "Randomize note delays.",
        std::make_tuple(optional_arg<float>("Float", "min", 0.f),
                        optional_arg<float>("Float", "max", 0.95f)),
        [](CommandInvocation const &invocation, float min, float max) {
            return RandomizeDelayAction{
                .pattern = invocation.input.pattern,
                .min = min,
                .max = max,
            };
        }));

    specs.push_back(make_spec(
        {"randomize", "gate"}, true, "Randomize note gates.",
        std::make_tuple(optional_arg<float>("Float", "min", 0.f),
                        optional_arg<float>("Float", "max", 0.95f)),
        [](CommandInvocation const &invocation, float min, float max) {
            return RandomizeGateAction{
                .pattern = invocation.input.pattern,
                .min = min,
                .max = max,
            };
        }));
}

} // namespace xen::catalog_detail
