#pragma once

#include <functional>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/measure.hpp>

#include "homogenous_row.hpp"
#include "measure.hpp"

namespace xen::gui
{

class Phrase : public HomogenousRow<Measure>
{
  public:
    Phrase() : HomogenousRow<Measure>{juce::FlexItem{}.withFlex(1.f), true}
    {
    }

  public:
    auto set(sequence::Phrase const &phrase, State const &state) -> void
    {
        this->HomogenousRow<Measure>::clear();

        for (auto const &measure : phrase)
        {
            this->emplace_back(measure, state);
        }
    }
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