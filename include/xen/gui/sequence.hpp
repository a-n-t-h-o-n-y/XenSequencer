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