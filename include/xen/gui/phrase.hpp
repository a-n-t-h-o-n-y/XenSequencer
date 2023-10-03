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
    auto set(State const &state, SelectedState const &selected) -> void
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

// TODO possibly add the ability to have a count of visible measures and a starting
// index to display them you can use the juce::Component::setVisible(false/true) to
// hide/show the component.
// class Phrase : public HomogenousRow<Measure>
// {
//   public:
//     auto set(sequence::Phrase const &phrase) -> void
//     {
//         this->HomogenousRow<Measure>::clear();

//         for (auto const &measure : phrase)
//         {
//             auto &measure_display = this->emplace_back(measure);

//             measure_display.on_update = [this] {
//                 if (this->on_update)
//                 {
//                     this->on_update();
//                 }
//             };
//         }
//     }

//     [[nodiscard]] auto get_phrase() const -> sequence::Phrase
//     {
//         auto phrase = sequence::Phrase{};

//         for (auto const &measure : *this)
//         {
//             phrase.push_back(measure.get_measure());
//         }

//         return phrase;
//     }

//     auto set_tuning_length(std::size_t length) -> void
//     {
//         for (auto &measure : *this)
//         {
//             measure.set_tuning_length(length);
//         }
//     }

//   public:
//     /**
//      * @brief Called when the phrase is updated.
//      */
//     std::function<void()> on_update;

//   protected:
//     auto paintOverChildren(juce::Graphics &g) -> void override
//     {
//         g.setColour(juce::Colours::white);

//         auto const bounds = getLocalBounds();
//         auto const right_x = (float)bounds.getRight();

//         g.drawLine(right_x, (float)bounds.getY(), right_x, (float)bounds.getBottom(),
//                    1);
//     }
// };

} // namespace xen::gui