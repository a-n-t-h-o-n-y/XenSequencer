#include "command_catalog_spec_builder.hpp"
#include "command_catalog_specs_internal.hpp"

namespace xen::catalog_detail
{

void append_transform_specs(std::vector<CommandSpec> &specs)
{
    specs.push_back(make_spec(
        {"stretch"}, true, "Stretch selected pattern.",
        std::make_tuple(optional_arg<std::size_t>("Unsigned", "count", 2)),
        [](CommandInvocation const &invocation, std::size_t count) {
            return StretchAction{
                .pattern = invocation.input.pattern,
                .count = count,
            };
        }));

    specs.push_back(make_spec(
        {"compress"}, true, "Compress selected pattern.", std::make_tuple(),
        [](CommandInvocation const &invocation) {
            return CompressAction{.pattern = invocation.input.pattern};
        }));

    specs.push_back(make_spec(
        {"shuffle"}, false, "Shuffle selected content.", std::make_tuple(),
        [](CommandInvocation const &) { return ShuffleAction{}; }));

    specs.push_back(make_spec(
        {"rotate"}, false, "Rotate selected content.",
        std::make_tuple(optional_arg<int>("Int", "amount", 1)),
        [](CommandInvocation const &, int amount) {
            return RotateAction{.amount = amount};
        }));

    specs.push_back(make_spec(
        {"reverse"}, false, "Reverse selected content.", std::make_tuple(),
        [](CommandInvocation const &) { return ReverseAction{}; }));

    specs.push_back(make_spec(
        {"mirror"}, true, "Mirror selected notes around center pitch.",
        std::make_tuple(optional_arg<int>("Int", "centerPitch", 0)),
        [](CommandInvocation const &invocation, int centerPitch) {
            return MirrorAction{
                .pattern = invocation.input.pattern,
                .center_pitch = centerPitch,
            };
        }));

    specs.push_back(make_spec(
        {"step"}, true,
        "Apply incremental pitch/velocity offsets to selected sequence.",
        std::make_tuple(optional_arg<int>("Int", "pitchDistance", 1),
                        optional_arg<float>("Float", "velocityDistance", 0.f)),
        [](CommandInvocation const &invocation, int pitchDistance,
           float velocityDistance) {
            return StepAction{
                .pattern = invocation.input.pattern,
                .pitch_distance = pitchDistance,
                .velocity_distance = velocityDistance,
            };
        }));

    specs.push_back(make_spec(
        {"arp"}, true, "Apply chord arpeggiation to selection.",
        std::make_tuple(optional_arg<std::string>("String", "chord", "cycle",
                                                  std::string{"\"cycle\""}),
                        optional_arg<int>("Int", "inversion", -1)),
        [](CommandInvocation const &invocation, std::string chord, int inversion) {
            return ArpAction{
                .pattern = invocation.input.pattern,
                .chord = std::move(chord),
                .inversion = inversion,
            };
        }));

    specs.push_back(make_spec(
        {"drums"}, false, "Switch to drum-oriented tuning.",
        std::make_tuple(optional_arg<std::size_t>("Unsigned", "octaveSize", 16),
                        optional_arg<int>("Int", "offset", 1)),
        [](CommandInvocation const &, std::size_t octaveSize, int offset) {
            return DrumsAction{.octave_size = octaveSize, .offset = offset};
        }));
}

} // namespace xen::catalog_detail
