#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <variant>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/sequence.hpp>

#include <xen/gui/homogenous_row.hpp>
#include <xen/state.hpp>

namespace xen::gui
{

class Cell : public juce::Component
{
  public:
    auto make_selected() -> void
    {
        selected_ = true;
    }

    virtual auto select_child(std::vector<std::size_t> const &indices) -> void
    {
        if (indices.empty())
        {
            this->make_selected();
        }
        else
        {
            throw std::runtime_error(
                "Invalid index or unexpected type encountered in traversal.");
        }
    }

  protected:
    auto paintOverChildren(juce::Graphics &g) -> void override
    {
        if (selected_)
        {
            constexpr auto thickness = 3;
            constexpr auto margin = 4;

            float const y_offset =
                static_cast<float>(this->getHeight() - (thickness / 2));
            float const x_start = margin;
            float const x_end = static_cast<float>(this->getWidth() - margin);

            g.setColour(juce::Colours::yellow);
            g.drawLine(x_start, y_offset, x_end, y_offset, thickness);
        }
    }

  private:
    bool selected_ = false;
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

    auto paint(juce::Graphics &g) -> void override
    {
        constexpr auto max_radius = 25.f;
        constexpr auto min_radius = 0.f;

        auto const bounds = getLocalBounds().toFloat().reduced(1.f, 2.f);
        auto const width = static_cast<float>(getWidth());
        auto const corner_radius =
            juce::jlimit(min_radius, max_radius,
                         juce::jmap(width, 30.f, 200.f, min_radius, max_radius));
        auto const line_thickness = 1.f;

        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(bounds, corner_radius, line_thickness);
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
        auto const brightness = std::lerp(0.5f, 1.f, vel);
        // old color: FFFF5B00
        bg_color_ = juce::Colour{0xFF0ad0f5}.withBrightness(brightness);
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
        auto const width = static_cast<float>(bounds.getWidth());
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

class SequenceIndicator : public juce::Component
{
  protected:
    void paint(juce::Graphics &g) override
    {
        constexpr auto margin = 4;

        float const y_offset = this->getHeight() / 2.f;
        float const x_start = margin;
        float const x_end = static_cast<float>(this->getWidth() - margin);

        g.setColour(juce::Colours::powderblue);
        g.drawLine(x_start, y_offset, x_end, y_offset,
                   static_cast<float>(this->getHeight() - 2));
    }
};

class Sequence : public Cell
{
  public:
    explicit Sequence(sequence::Sequence const &seq, State const &state);

  public:
    auto select_child(std::vector<std::size_t> const &indices) -> void override
    {
        if (indices.empty())
        {
            this->make_selected();
            return;
        }

        cells_.at(indices[0])
            .select_child(std::vector(std::next(indices.cbegin()), indices.cend()));
    }

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::column;

        flexbox.items.add(juce::FlexItem(indicator_).withHeight(4.f));
        flexbox.items.add(juce::FlexItem(cells_).withFlex(1.f));

        flexbox.performLayout(this->getLocalBounds());
    }

  private:
    HomogenousRow<Cell> cells_;
    SequenceIndicator indicator_;
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