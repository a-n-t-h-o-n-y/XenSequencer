#pragma once

#include <functional>
#include <variant>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/measure.hpp>
#include <sequence/time_signature.hpp>
#include <sequence/utility.hpp>

#include <xen/gui/color_ids.hpp>

#include "sequence.hpp"

namespace xen::gui
{

class TimeSignature : public juce::Label
{
  public:
    explicit TimeSignature(sequence::TimeSignature const &time_sig)
    {
        this->set(time_sig);
        this->setFont(juce::Font{"Arial", "Bold", 14.f});
        this->colourChanged();
    }

  private:
    auto set(sequence::TimeSignature const &time_sig) -> void
    {
        auto const text =
            juce::String{time_sig.numerator} + "/" + juce::String{time_sig.denominator};
        this->setText(text, juce::dontSendNotification);
    }

  protected:
    auto colourChanged() -> void override
    {
        this->setColour(juce::Label::ColourIds::textColourId,
                        this->findColour((int)TimeSignatureColorIDs::Text));
        this->setColour(juce::Label::ColourIds::backgroundColourId,
                        this->findColour((int)TimeSignatureColorIDs::Background));
    }
};

class Measure : public juce::Component
{
  public:
    Measure(sequence::Measure const &measure, State const &state)
        : time_sig_{measure.time_signature}, cell_ptr_{make_cell(measure.cell, state)}
    {
        this->addAndMakeVisible(time_sig_);
        this->addAndMakeVisible(*cell_ptr_);
    }

  public:
    auto select(std::vector<std::size_t> const &indices) -> void
    {
        cell_ptr_->select_child(indices);
    }

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::column;

        flexbox.items.add(juce::FlexItem{time_sig_}.withHeight(20.f));
        flexbox.items.add(juce::FlexItem{*cell_ptr_}.withFlex(1.f));

        flexbox.performLayout(getLocalBounds());
    }

  private:
    [[nodiscard]] static auto make_cell(sequence::Cell const &cell, State const &state)
        -> std::unique_ptr<Cell>
    {
        auto const builder = BuildAndAllocateCell{state};

        return std::visit(builder, cell);
    }

  private:
    TimeSignature time_sig_;
    std::unique_ptr<Cell> cell_ptr_;
};

} // namespace xen::gui