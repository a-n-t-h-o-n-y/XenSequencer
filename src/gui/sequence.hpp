#pragma once

#include <cstddef>
#include <memory>
#include <variant>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/sequence.hpp>

#include "../state.hpp"
#include "homogenous_row.hpp"

namespace xen::gui
{

class Cell : public juce::Component
{
};

class Rest : public Cell
{
  public:
    explicit Rest(sequence::Rest)
    {
        this->addAndMakeVisible(label_);

        label_.setFont(juce::Font{"Arial", "Normal", 14.f}.boldened());
        label_.setColour(juce::Label::ColourIds::textColourId, juce::Colours::white);
        label_.setJustificationType(juce::Justification::centred);
    }

  protected:
    auto resized() -> void override
    {
        label_.setBounds(this->getLocalBounds());
    }

  private:
    juce::Label label_{"R", "R"};
};

class NoteInterval : public juce::Component
{
  public:
    NoteInterval(int interval, std::size_t tuning_length, float velocity)
        : interval_{interval}, tuning_length_{tuning_length}, velocity_{velocity}
    {
        // Called explicitly to generate color
        this->set_velocity(velocity);
    }

  protected:
    auto paint(juce::Graphics &g) -> void override;

  private:
    auto set_velocity(float vel) -> void
    {
        auto const brightness = std::lerp(0.3f, 1.f, vel);
        bg_color_ = juce::Colour{0xFFFF5B00}.withBrightness(brightness);
        this->repaint();
    }

    [[nodiscard]] static auto get_interval_and_octave(int interval,
                                                      std::size_t tuning_length)
        -> std::pair<int, int>
    {
        auto const tuning_length_int = static_cast<int>(tuning_length);
        auto octave = interval / tuning_length_int;
        auto adjusted_interval = interval % tuning_length_int;

        if (adjusted_interval < 0)
        {
            adjusted_interval += tuning_length_int;
            --octave;
        }

        return {adjusted_interval, octave};
    }

    // [[nodiscard]] static auto get_interval_and_octave(int interval,
    //                                                   std::size_t tuning_length)
    //     -> std::pair<int, int>
    // {
    //     auto octave = interval / (int)tuning_length;
    //     if (interval >= 0)
    //     {
    //         // For positive interval, use simple division and modulo operations
    //         return std::make_pair(interval % (int)tuning_length, octave);
    //     }
    //     else
    //     {
    //         // For negative interval, calculate the adjusted interval and octave
    //         int adjusted_interval =
    //             ((int)tuning_length + (interval % (int)tuning_length)) %
    //             (int)tuning_length;

    //         if (adjusted_interval != 0)
    //         {
    //             // Adjust Octave for negative intervals, the first negative octave is
    //             // -1, not zero.
    //             --octave;
    //         }

    //         return std::make_pair(adjusted_interval, octave);
    //     }
    // }

  private:
    int interval_;
    std::size_t tuning_length_;
    float velocity_;

    juce::Colour bg_color_;
};

class Note : public Cell
{
  public:
    explicit Note(sequence::Note const &note, std::size_t tuning_length)
        : note_{note}, interval_box_{note.interval, tuning_length, note.velocity}
    {
        this->addAndMakeVisible(interval_box_);
    }

  protected:
    auto resized() -> void override
    {
        auto const bounds = getLocalBounds();
        auto const width = bounds.getWidth();
        auto const delay = note_.delay;
        auto const gate = note_.gate;
        auto const left_x = delay * width;
        auto const right_x = left_x + (width - left_x) * gate;

        interval_box_.setBounds((int)left_x, bounds.getY(), (int)(right_x - left_x),
                                bounds.getHeight());
    }

  private:
    sequence::Note note_;
    NoteInterval interval_box_;
};

class Sequence : public Cell
{
  public:
    explicit Sequence(sequence::Sequence const &seq, State const &state);

  protected:
    auto resized() -> void override
    {

        cells_.setBounds(this->getLocalBounds());
    }

  private:
    HomogenousRow<Cell> cells_;
};

class BuildAndAllocateCell
{
  public:
    BuildAndAllocateCell(State const &state) : state_{state}
    {
    }

  public:
    [[nodiscard]] auto operator()(sequence::Rest r) const -> std::unique_ptr<Cell>
    {
        return std::make_unique<Rest>(r);
    }

    [[nodiscard]] auto operator()(sequence::Note n) const -> std::unique_ptr<Cell>
    {
        return std::make_unique<Note>(n, state_.tuning.intervals.size());
    }

    [[nodiscard]] auto operator()(sequence::Sequence s) const -> std::unique_ptr<Cell>
    {
        return std::make_unique<Sequence>(s, state_);
    }

  private:
    State const &state_;
};

} // namespace xen::gui