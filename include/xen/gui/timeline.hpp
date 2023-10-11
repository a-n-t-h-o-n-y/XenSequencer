#pragma once

#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/measure.hpp>

#include <xen/state.hpp>

namespace xen::gui
{

class TimelineMeasure : public juce::Component
{
  public:
    explicit TimelineMeasure(sequence::Measure const &measure, bool selected)
        : measure_{measure}, selected_{selected}
    {
    }

  public:
    [[nodiscard]] auto time_signature() const -> sequence::TimeSignature const &
    {
        return measure_.time_signature;
    }

  protected:
    void paint(juce::Graphics &g) override;

  private:
    sequence::Measure measure_;
    bool selected_;

    juce::Component measure_holder_;
    juce::Component selection_;
};

/**
 * High level visual overview of an entire Phrase.
 */
class Timeline : public juce::Component
{
  public:
    auto set(sequence::Phrase const &phrase, SelectedState const &selected) -> void;

  protected:
    void paint(juce::Graphics &g) override;

    void resized() override;

  public:
    std::vector<std::unique_ptr<TimelineMeasure>> measures_;
};

} // namespace xen::gui
