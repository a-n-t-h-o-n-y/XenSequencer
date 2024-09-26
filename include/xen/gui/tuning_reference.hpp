#pragma once

#include <optional>
#include <set>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/tuning.hpp>

#include <xen/scale.hpp>

namespace xen::gui
{

/**
 * Displays Just Intonation ratios on right and passed in ratios on the left, and
 * highlights any selected pitches.
 */
class TuningReference : public juce::Component
{
  public:
    /**
     * \p highlight_pitches do not have to be normalized.
     */
    TuningReference(sequence::Tuning const &tuning, std::optional<Scale> const &scale,
                    std::set<int> const &highlight_pitches,
                    TranslateDirection scale_translate_direction);

  public:
    void paint(juce::Graphics &g) override;

  private:
    sequence::Tuning tuning_;
    std::optional<Scale> scale_;
    std::set<int> pitches_;

    std::set<float> reference_ratios_;
    std::set<float> tuning_ratios_;
};

} // namespace xen::gui