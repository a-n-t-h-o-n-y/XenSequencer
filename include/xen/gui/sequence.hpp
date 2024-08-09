#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <variant>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/sequence.hpp>

#include <xen/gui/color_ids.hpp>
#include <xen/gui/homogenous_row.hpp>
#include <xen/state.hpp>
#include <xen/utility.hpp>

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
            constexpr auto thickness = 2;
            constexpr auto margin = 4;

            float const y_offset = 0;
            float const x_start = margin;
            float const x_end = static_cast<float>(this->getWidth() - margin);

            g.setColour(this->findColour((int)MeasureColorIDs::SelectionHighlight));
            g.drawLine(x_start, y_offset, x_end, y_offset, thickness);
        }
    }

  private:
    bool selected_ = false;
};

class Rest : public Cell
{
  public:
    explicit Rest(sequence::Rest, std::size_t interval_count)
        : interval_count_{interval_count}
    {
    }

  protected:
    auto paint(juce::Graphics &g) -> void override;

  private:
    std::size_t interval_count_;
};

class NoteInterval : public juce::Component
{
  public:
    NoteInterval(int interval, std::size_t tuning_length, float velocity)
        : interval_{interval}, tuning_length_{tuning_length}
    {
        // Called explicitly to generate color
        this->set_velocity(velocity);
    }

  protected:
    auto paint(juce::Graphics &g) -> void override;

    auto lookAndFeelChanged() -> void override
    {
        this->set_velocity(velocity_);
    }

  private:
    auto set_velocity(float vel) -> void
    {
        velocity_ = vel;
        auto const brightness = std::lerp(0.5f, 1.f, vel);
        bg_color_ =
            this->findColour((int)NoteColorIDs::Foreground)
                .withBrightness(compare_within_tolerance(vel, 0.f, 1e-6f) ? 0.2f
                                                                          : brightness);
        this->repaint();
    }

  private:
    float velocity_;
    int interval_;
    std::size_t tuning_length_;

    juce::Colour bg_color_;
};

class NoteHolder : public juce::Component
{
  public:
    explicit NoteHolder(sequence::Note const &note, std::size_t tuning_length)
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

// class TraitDisplay : public juce::Component
// {
//   public:
//     explicit TraitDisplay(std::string name, float value)
//         : label_{name, make_display(name, value)}
//     {
//         this->addAndMakeVisible(label_);
//     }

//   protected:
//     auto resized() -> void override
//     {
//         label_.setBounds(this->getLocalBounds());
//     }

//   private:
//     [[nodiscard]] static auto make_display(std::string const &name,
//                                            float value) -> std::string
//     {
//         auto oss = std::ostringstream{};
//         oss << std::fixed << std::setprecision(2) << value;
//         return name + ": " + oss.str();
//     }

//   private:
//     juce::Label label_;
// };

class Note : public Cell
{
  public:
    explicit Note(sequence::Note const &note, std::size_t tuning_length)
        : note_{note}, note_holder_{note, tuning_length} //, note_traits_{note}
    {
        this->addAndMakeVisible(note_holder_);
    }

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::column;

        flexbox.items.add(juce::FlexItem(note_holder_).withFlex(1.f));

        flexbox.performLayout(this->getLocalBounds());
    }

  private:
    sequence::Note note_;
    NoteHolder note_holder_;
};

class SequenceIndicator : public juce::Component
{
  protected:
    void paint(juce::Graphics &g) override;
};

/**
 * Vertical column to display interval numbers, [0, size) bottom to top, evenly spaced.
 */
class IntervalColumn : public juce::Component
{
  public:
    IntervalColumn(std::size_t size, float vertical_offset)
        : size_{size}, vertical_offset_{vertical_offset}
    {
    }

    auto paint(juce::Graphics &g) -> void override;

  private:
    std::size_t size_;
    float vertical_offset_;
};

/**
 * Holds the Selected Sequence Indicator, the Interval Column and the actual Sequence.
 */
class Sequence : public Cell
{
  public:
    explicit Sequence(sequence::Sequence const &seq, std::size_t tuning_size);

  public:
    auto select_child(std::vector<std::size_t> const &indices) -> void override;

  protected:
    auto paint(juce::Graphics &g) -> void override;

    auto resized() -> void override;

  private:
    SequenceIndicator top_indicator_;
    IntervalColumn interval_column_;
    HomogenousRow<Cell> cells_;
};

class BuildAndAllocateCell
{
  public:
    BuildAndAllocateCell(std::size_t tuning_octave_size) : tos_{tuning_octave_size}
    {
    }

  public:
    [[nodiscard]] auto operator()(sequence::Rest r) const -> std::unique_ptr<Cell>
    {
        return std::make_unique<Rest>(r, tos_);
    }

    [[nodiscard]] auto operator()(sequence::Note n) const -> std::unique_ptr<Cell>
    {
        return std::make_unique<Note>(n, tos_);
    }

    [[nodiscard]] auto operator()(sequence::Sequence s) const -> std::unique_ptr<Cell>
    {
        return std::make_unique<Sequence>(s, tos_);
    }

  private:
    std::size_t tos_;
};

} // namespace xen::gui