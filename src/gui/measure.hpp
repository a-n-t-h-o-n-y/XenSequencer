#pragma once

#include <functional>

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

    [[nodiscard]] auto get_time_signature() const -> sequence::TimeSignature
    {
        return time_sig_;
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

class Ruler : public juce::Component
{
  public:
    Ruler(sequence::TimeSignature const &time_sig = {4, 4}) : time_sig_{time_sig}
    {
    }

  public:
    auto set(sequence::TimeSignature const &time_sig) -> void
    {
        time_sig_ = time_sig;
        this->repaint();
    }

  protected:
    auto paint(juce::Graphics &g) -> void override
    {
        // set the current drawing color
        g.setColour(juce::Colours::white);
        g.fillAll(juce::Colours::darkgrey);

        // get the bounds of the component
        auto const bounds = getLocalBounds();

        auto const beat_width = (float)bounds.getWidth() / time_sig_.numerator;

        for (auto i = 2; i >= -3; --i)
        {
            auto const beats = std::pow(2, i);
            auto const px_length = beats * beat_width;
            auto const height =
                scaleValue((float)i, -3.f, 2.f, 0.1f, 1.f) * bounds.getHeight();

            for (auto j = 0; (float)j < ((float)time_sig_.numerator / beats); ++j)
            {
                auto const x = px_length * j;
                auto const y = bounds.getY();
                g.drawRect(x, y + bounds.getHeight() - height, 1.f, height, 1.f);
            }
        }
    }

  private:
    sequence::TimeSignature time_sig_;

  private:
    [[nodiscard]] static auto scaleValue(float input, float inputMin, float inputMax,
                                         float outputMin, float outputMax) -> float
    {
        if (input < inputMin || input > inputMax)
        {
            throw std::invalid_argument(
                "Input must be within the specified input range.");
        }
        // scale input to 0-1 range
        auto const t = (input - inputMin) / (inputMax - inputMin);

        // use lerp to scale this to the output range
        return std::lerp(outputMin, outputMax, t);
    }
};

class Measure : public juce::Component
{
  public:
    Measure(sequence::Measure const &measure)
    {
        this->addAndMakeVisible(time_sig_);
        this->addAndMakeVisible(sequence_);
        this->addAndMakeVisible(ruler_);

        this->set(measure);
    }

  public:
    auto set(sequence::Measure const &measure) -> void
    {
        // TODO if you allow the time signature to be edited, then connect to its
        // on_update signal.
        time_sig_.set(measure.time_signature);
        ruler_.set(measure.time_signature);

        sequence_.set(measure.sequence);
        sequence_.on_update = [this] {
            if (this->on_update)
            {
                this->on_update();
            }
        };

        if (this->on_update)
        {
            this->on_update();
        }
    }

    [[nodiscard]] auto get_measure() const -> sequence::Measure
    {
        return sequence::Measure{
            sequence_.get_sequence(),
            time_sig_.get_time_signature(),
        };
    }

  public:
    /**
     * @brief Called when the measure is updated.
     */
    std::function<void()> on_update;

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::column;

        flexbox.items.add(juce::FlexItem{time_sig_}.withHeight(20.f));
        flexbox.items.add(juce::FlexItem{sequence_}.withFlex(1.f));
        flexbox.items.add(juce::FlexItem{ruler_}.withHeight(20.f));

        flexbox.performLayout(getLocalBounds());
    }

  private:
    TimeSignature time_sig_;
    Sequence sequence_;
    Ruler ruler_;
};

} // namespace xen::gui