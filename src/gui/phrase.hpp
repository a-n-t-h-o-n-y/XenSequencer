#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/measure.hpp>

#include "homogenous_row.hpp"
#include "measure.hpp"

namespace xen::gui
{

// TODO possibly add the ability to have a count of visible measures and a starting
// index to display them you can use the juce::Component::setVisible(false/true) to
// hide/show the component.
class Phrase : public HomogenousRow<Measure>
{
  public:
    Phrase()
    {
        // TODO temporary test measures
        auto const measure1 = [] {
            namespace seq = sequence;
            auto m = seq::create_measure({4, 4}, 2);
            for (auto i = 0; i < (int)m.sequence.cells.size(); ++i)
            {
                // if even create a subsequence else create a note
                if (i % 2 == 0)
                {
                    m.sequence.cells[i] = seq::Sequence{{
                        seq::Note{i, 0.75f, 0.5f, 1.f},
                        seq::Rest{},
                        seq::Note{i + 2, 0.75f, 0.25f, 1.f},
                    }};
                }
                else
                {
                    m.sequence.cells[i] = seq::Note{i, 0.75f, 1.f, 1.f};
                }
            }
            return m;
        }();

        auto const measure2 = [] {
            namespace seq = sequence;
            auto m = seq::create_measure({2, 4}, 1);
            for (auto i = 0; i < (int)m.sequence.cells.size(); ++i)
            {
                m.sequence.cells[i] = seq::Sequence{{
                    seq::Note{i, 0.75f, 0.5f, 0.8f},
                    seq::Rest{},
                    seq::Note{i + 2, 0.75f, 0.25f, 0.1f},
                }};
            }
            return m;
        }();

        this->set({
            measure1,
            measure2,
        });
    }

  public:
    auto set(sequence::Phrase const &phrase) -> void
    {
        this->HomogenousRow<Measure>::clear();

        for (auto const &measure : phrase)
        {
            this->emplace_back(measure);
        }
    }

  protected:
    auto paint(juce::Graphics &g) -> void override
    {
        (void)g;
        // set the current drawing color
        // g.setColour(juce::Colours::white);

        // // draw an outline around the component
        // g.drawRect(getLocalBounds(), 1);
    }

  private:
    sequence::Phrase phrase_;
};

} // namespace xen::gui