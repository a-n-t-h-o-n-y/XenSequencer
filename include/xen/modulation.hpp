#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <sequence/pattern.hpp>

#include <xen/message_level.hpp>
#include <xen/state.hpp>

namespace xen
{

inline constexpr auto MAX_MODULATION_WAVEFORMS = std::size_t{64};

enum class WaveformShape : std::uint8_t
{
    Sine,
    Triangle,
    SawtoothUp,
    SawtoothDown,
    Square,
};

struct ModulationWaveform
{
    bool enabled{true};
    WaveformShape shape{WaveformShape::Sine};
    float frequency{1.f};
    float phase{0.f};
    float amplitude{1.f};
    float amplitude_offset{0.f};

    auto operator==(ModulationWaveform const &) const -> bool = default;
};

enum class ModulationOperation : std::uint8_t
{
    Average,
    Sum,
    Product,
    AmplitudeModulation,
    RingModulation,
    FrequencyModulation,
    PhaseModulation,
};

struct ModulationDefinition
{
    ModulationOperation operation{ModulationOperation::Average};
    std::vector<ModulationWaveform> waveforms{};

    auto operator==(ModulationDefinition const &) const -> bool = default;
};

enum class ModulationDestination : std::uint8_t
{
    Pitch,
    Velocity,
    Delay,
    Gate,
    Weight,
};

struct ModulationOutputRange
{
    double minimum{};
    double maximum{1.0};

    auto operator==(ModulationOutputRange const &) const -> bool = default;
};

struct ModulationTarget
{
    CompositionCursor cursor{};
    SelectionPath selection{};
    sequence::Pattern pattern{0, {1}};

    auto operator==(ModulationTarget const &) const -> bool = default;
};

struct ModulationPreviewUpdate
{
    PreviewId preview_id{};
    std::uint64_t update_sequence{};
    ProjectRevision expected_project_revision{};
    ModulationDestination destination{ModulationDestination::Velocity};
    ModulationOutputRange output_range{};
    ModulationDefinition modulation{};
};

struct ModulationPreviewUpdateResult
{
    std::pair<MessageLevel, std::string> status{MessageLevel::Debug, ""};
    PreviewId preview_id{};
    std::uint64_t accepted_update_sequence{};
    bool accepted{false};
    bool project_changed{false};
    ProjectRevision project_revision{};
    StateRevision state_revision{};
};

void validate(ModulationDefinition const &modulation);
void validate(ModulationDestination destination, ModulationOutputRange const &range);

/** Evaluate normalized modulation amounts at x = index / sample_count. */
[[nodiscard]] auto evaluate(ModulationDefinition const &modulation,
                            std::size_t sample_count) -> std::vector<float>;

} // namespace xen
