#pragma once

#include <functional>
#include <memory>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/measure.hpp>

#include "homogenous_row.hpp"
#include "measure.hpp"

namespace xen::gui
{

class Phrase : public juce::Component
{
  public:
    auto set(SequencerState const &state, SelectedState const &selected) -> void
    {
        measure_ptr_.reset();
        auto const &phrase = state.phrase;
        if (!phrase.empty())
        {
            auto const &selected_measure = phrase[selected.measure];
            measure_ptr_ = std::make_unique<Measure>(selected_measure, state);
            this->addAndMakeVisible(*measure_ptr_);
            this->resized();
        }
    }

    auto select(SelectedState const &selected) -> void
    {
        if (measure_ptr_)
        {
            measure_ptr_->select(selected.cell);
        }
    }

  protected:
    auto resized() -> void override
    {
        if (measure_ptr_)
        {
            measure_ptr_->setBounds(this->getLocalBounds());
        }
    }

  private:
    std::unique_ptr<Measure> measure_ptr_{nullptr};
};

} // namespace xen::gui