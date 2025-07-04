#include <xen/gui/tuning_reference.hpp>

#include <cassert>
#include <cmath>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/tuning.hpp>

#include <xen/gui/themes.hpp>
#include <xen/scale.hpp>
#include <xen/utility.hpp>

namespace
{

[[nodiscard]]
auto largest_element(std::set<float> const &s) -> float
{
    return s.empty() ? throw std::runtime_error("set is empty") : *std::prev(s.end());
}

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
                                sequence::Tuning const &tuning,
                                xen::TranslateDirection scale_translate_direction)
    -> std::set<int>
{
    auto result = std::set<int>{};
    if (scale.has_value())
    {
        auto valid_pitches = xen::generate_valid_pitches(*scale);
        for (auto pitch : pitches)
        {
            auto const norm =
                (int)xen::utility::normalize_pitch(pitch, tuning.intervals.size());
            result.insert(xen::map_pitch_to_scale(norm, valid_pitches,
                                                  tuning.intervals.size(),
                                                  scale_translate_direction));
        }
    }
    else
    {
        for (auto pitch : pitches)
        {
            result.insert(
                (int)xen::utility::normalize_pitch(pitch, tuning.intervals.size()));
        }
    }
    return result;
}

} // namespace

namespace xen::gui
{

TuningReference::TuningReference(sequence::Tuning const &tuning,
                                 std::optional<Scale> const &scale,
                                 std::set<int> const &highlight_pitches,
                                 TranslateDirection scale_translate_direction)
    : tuning_{tuning}, scale_{scale},
      pitches_{
          init_pitches(highlight_pitches, scale, tuning, scale_translate_direction)},
      reference_ratios_{init_reference_ratios()},
      tuning_ratios_{init_tuning_ratios(tuning)}
{
}

void TuningReference::paint(juce::Graphics &g)
{
    auto const mark_length = 10;
    auto const thickness = 2;

    g.fillAll(this->findColour(ColorID::BackgroundHigh));

    auto const bounds = this->getLocalBounds().reduced(0, 4);

    // The space between the last interval and the octave of the reference tuning, split
    // between the top and bottom of the display
    auto const gap_to_octave =
        (1.f - largest_element(reference_ratios_)) * bounds.getHeight();

    auto const mid_x = bounds.getX() + (int)std::round((float)bounds.getWidth() / 2.f);

    auto a = bounds.getY() + bounds.getHeight() - (gap_to_octave / 2);

    { // Paint Reference Marks
        g.setColour(this->findColour(ColorID::ForegroundHigh));
        for (auto const ratio : reference_ratios_)
        {
            auto y = (int)std::round(a - (ratio * bounds.getHeight()));
            assert(y >= bounds.getY());
            g.fillRect(mid_x, y - (int)(thickness / 2.f), mark_length, thickness);
        }
    }

    { // Paint Tuning Marks
        auto i = 0;
        for (auto ratio : tuning_ratios_)
        {
            auto const color_id =
                pitches_.contains(i) ? ColorID::ForegroundHigh : ColorID::ForegroundLow;
            g.setColour(this->findColour(color_id));

            auto y = (int)std::round(a - (ratio * bounds.getHeight()));

            while (y < bounds.getY())
            {
                y += bounds.getY() + bounds.getHeight();
            }
            g.fillRect(mid_x - mark_length, y - (int)std::round(thickness / 2.f),
                       mark_length, thickness);
            ++i;
        }
    }

    { // Paint Vertical Line
        g.setColour(this->findColour(ColorID::ForegroundHigh));
        g.fillRect(mid_x - (int)std::round((float)thickness / 2.f), bounds.getY(),
                   thickness, bounds.getHeight());
    }
}

} // namespace xen::gui