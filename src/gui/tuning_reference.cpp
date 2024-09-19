#include <xen/gui/tuning_reference.hpp>

#include <cmath>
#include <optional>
#include <set>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/tuning.hpp>

#include <xen/gui/themes.hpp>
#include <xen/scale.hpp>
#include <xen/utility.hpp>

namespace
{

/**
 * Returns a list of just intonation ratios spread logarithmically. These act as
 * percentages for a visual display.
 */
[[nodiscard]] auto init_reference_ratios() -> std::set<float>
{
    return {
        std::log2(1.f / 1.f),  std::log2(3.f / 2.f),   std::log2(4.f / 3.f),
        std::log2(5.f / 4.f),  std::log2(6.f / 5.f),   std::log2(5.f / 3.f),
        std::log2(8.f / 5.f),  std::log2(9.f / 8.f),   std::log2(16.f / 9.f),
        std::log2(15.f / 8.f), std::log2(16.f / 15.f), std::log2(45.f / 32.f),
    };
}

[[nodiscard]] auto init_tuning_ratios(sequence::Tuning const &tuning) -> std::set<float>
{
    auto result = std::set<float>{};
    for (auto const cents : tuning.intervals)
    {
        result.insert(cents / 1'200.f);
    }
    return result;
}

[[nodiscard]] auto init_pitches(std::set<int> const &pitches,
                                std::optional<xen::Scale> const &scale,
                                sequence::Tuning const &tuning) -> std::set<int>
{
    auto result = std::set<int>{};
    if (scale.has_value())
    {
        auto valid_pitches = xen::generate_valid_pitches(*scale);
        for (auto pitch : pitches)
        {
            auto const norm = (int)xen::normalize_pitch(pitch, tuning.intervals.size());
            result.insert(xen::map_pitch_to_scale(norm, valid_pitches,
                                                  tuning.intervals.size(),
                                                  xen::TranslateDirection::Up));
        }
    }
    else
    {
        for (auto pitch : pitches)
        {
            result.insert((int)xen::normalize_pitch(pitch, tuning.intervals.size()));
        }
    }
    return result;
}

} // namespace

namespace xen::gui
{

TuningReference::TuningReference(sequence::Tuning const &tuning,
                                 std::optional<Scale> const &scale,
                                 std::set<int> const &highlight_pitches)
    : tuning_{tuning}, scale_{scale},
      pitches_{init_pitches(highlight_pitches, scale, tuning)},
      reference_ratios_{init_reference_ratios()},
      tuning_ratios_{init_tuning_ratios(tuning)}
{
    // noramlise and map to scale pitches
}

void TuningReference::paint(juce::Graphics &g)
{
    g.fillAll(this->findColour(ColorID::BackgroundMedium));

    auto const bounds = this->getLocalBounds().reduced(0, 4).toFloat();
    auto const vertical_offset =
        bounds.getHeight() / ((float)tuning_.intervals.size() * 2.f);

    // Draw Vertical Line
    auto const midpoint_x = bounds.getX() + (bounds.getWidth() / 2.f);
    g.setColour(this->findColour(ColorID::ForegroundHigh));
    g.drawLine(midpoint_x, bounds.getY() + vertical_offset, midpoint_x,
               bounds.getY() + bounds.getHeight() - vertical_offset, 1.5f);

    // Paint Reference Marks
    {
        g.setColour(this->findColour(ColorID::ForegroundHigh));
        auto const tick_margin_x = 10.f;
        auto const thickness = 1.5f;
        for (auto const ratio : reference_ratios_)
        {
            auto y = bounds.getY() + bounds.getHeight() - (ratio * bounds.getHeight()) -
                     vertical_offset - (thickness / 2.f);
            if (y <= 0.f)
            {
                y += bounds.getHeight() + bounds.getY();
            }
            g.drawLine(midpoint_x, y, midpoint_x + tick_margin_x, y, thickness);
        }
    }

    // Paint Tuning Marks
    {
        auto const tick_margin_x = -10.f;
        auto i = 0;
        for (auto ratio : tuning_ratios_)
        {
            auto const color_id =
                pitches_.contains(i) ? ColorID::ForegroundHigh : ColorID::ForegroundLow;
            auto const thickness = 1.5f;
            g.setColour(this->findColour(color_id));
            auto y = bounds.getY() + bounds.getHeight() - (ratio * bounds.getHeight()) -
                     vertical_offset - (thickness / 2.f);
            if (y <= 0.f)
            {
                y += bounds.getHeight() + bounds.getY();
            }
            g.drawLine(midpoint_x, y, midpoint_x + tick_margin_x, y, thickness);
            ++i;
        }
    }
}

} // namespace xen::gui