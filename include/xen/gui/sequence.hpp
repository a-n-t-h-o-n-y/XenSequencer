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
    auto paintOverChildren(juce::Graphics &g) -> void override;

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

class Note : public Cell
{
  public:
    Note(sequence::Note note, std::size_t tuning_length)
        : note_{note}, tuning_length_{tuning_length}
    {
    }

  protected:
    auto paint(juce::Graphics &g) -> void override;

  private:
    sequence::Note note_;
    std::size_t tuning_length_;
};

// class NoteHolder : public juce::Component
// {
//   public:
//     explicit NoteHolder(sequence::Note const &note, std::size_t tuning_length)
//         : note_{note}, interval_box_{note, tuning_length}
//     {
//         this->addAndMakeVisible(interval_box_);
//     }

//   protected:
//     auto resized() -> void override
//     {
//         // auto const bounds = getLocalBounds();
//         // auto const width = static_cast<float>(bounds.getWidth());
//         // auto const delay = note_.delay;
//         // auto const gate = note_.gate;
//         // auto const left_x = delay * width;
//         // auto const right_x = left_x + (width - left_x) * gate;

//         // interval_box_.setBounds((int)left_x, bounds.getY(), (int)(right_x -
//         left_x),
//         //                         bounds.getHeight());

//         interval_box_.setBounds(this->getLocalBounds());
//     }

//   private:
//     sequence::Note note_;
//     NoteInterval interval_box_;
// };

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

// class Note : public Cell
// {
//   public:
//     explicit Note(sequence::Note const &note, std::size_t tuning_length)
//         : note_{note}, note_interval_{note, tuning_length} //, note_traits_{note}
//     {
//         this->addAndMakeVisible(note_interval_);
//     }

//   protected:
//     auto resized() -> void override
//     {
//       note_interval_
//         auto flexbox = juce::FlexBox{};
//         flexbox.flexDirection = juce::FlexBox::Direction::column;

//         flexbox.items.add(juce::FlexItem(note_interval_).withFlex(1.f));

//         flexbox.performLayout(this->getLocalBounds());
//     }

//   private:
//     sequence::Note note_;
//     NoteInterval note_interval_;
// };

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