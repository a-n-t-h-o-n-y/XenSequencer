#pragma once

#include <optional>
#include <string>

#include <sequence/tuning.hpp>

#include <xen/scale.hpp>

namespace xen
{

struct NamedTuning
{
    std::string name{"12-TET"};
    sequence::Tuning definition{
        .intervals = {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100},
        .octave = 1200,
        .description = "",
    };

    auto operator==(NamedTuning const &) const -> bool = default;
};

struct ActiveScale
{
    std::optional<std::string> source_id{};
    Scale definition{};

    auto operator==(ActiveScale const &) const -> bool = default;
};

struct PitchSystem
{
    NamedTuning tuning{};
    std::optional<ActiveScale> scale{};
    int transposition{0};
    TranslateDirection translation_direction{TranslateDirection::Up};
    float base_frequency{440.f};

    auto operator==(PitchSystem const &) const -> bool = default;
};

} // namespace xen
