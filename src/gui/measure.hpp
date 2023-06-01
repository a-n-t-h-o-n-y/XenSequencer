#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/measure.hpp>
#include <sequence/time_signature.hpp>

#include "sequence.hpp"

namespace xen::gui
{

class TimeSignature : public juce::Label
{
  public:
    TimeSignature(sequence::TimeSignature const &time_sig = {4, 4})
        : time_sig_{time_sig}
    {
        this->set(time_sig_);
        this->setFont(juce::Font{"Arial", "Bold", 14.f});
        this->setColour(juce::Label::ColourIds::textColourId, juce::Colours::white);
    }

  public:
    auto set(sequence::TimeSignature const &time_sig) -> void
    {
        time_sig_ = time_sig;
        auto const text = juce::String{time_sig_.numerator} + "/" +
                          juce::String{time_sig_.denominator};
        this->setText(text, juce::dontSendNotification);
    }

  protected:
    auto paint(juce::Graphics &g) -> void override
    {
        this->juce::Label::paint(g);

        // set the current drawing color
        g.setColour(juce::Colours::white);

        // draw an outline around the component
        g.drawRect(getLocalBounds(), 1);
    }

  private:
    sequence::TimeSignature time_sig_;
};

// TODO - a sequence and time signature.
class Measure : public juce::Component
{
  public:
    Measure(sequence::Measure const &measure) : measure_{measure}
    {
        this->addAndMakeVisible(time_sig_);
        this->addAndMakeVisible(sequence_);

        this->set(measure_);
    }

  public:
    auto set(sequence::Measure const &measure) -> void
    {
        time_sig_.set(measure.time_signature);
        sequence_.set(measure.sequence);
    }

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::column;

        flexbox.items.add(juce::FlexItem{time_sig_}.withHeight(20.f));
        flexbox.items.add(juce::FlexItem{sequence_}.withFlex(1.f));

        flexbox.performLayout(getLocalBounds());
    }

  private:
    sequence::Measure measure_;

  private:
    TimeSignature time_sig_;
    Sequence sequence_;
};

} // namespace xen::gui