#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/sequence.hpp>

#include <xen/gui/homogenous_row.hpp>

namespace xen::gui
{

class Cell : public juce::Component
{
  public:
    void make_selected();

    virtual void select_child(std::vector<std::size_t> const &indices);

  protected:
    void paintOverChildren(juce::Graphics &g) override;

  private:
    bool selected_ = false;
};

// -------------------------------------------------------------------------------------

class Rest : public Cell
{
  public:
    explicit Rest(sequence::Rest, std::size_t interval_count);

  protected:
    void paint(juce::Graphics &g) override;

  private:
    std::size_t interval_count_;
};

// -------------------------------------------------------------------------------------

class Note : public Cell
{
  public:
    Note(sequence::Note note, std::size_t tuning_length);

  protected:
    void paint(juce::Graphics &g) override;

  private:
    sequence::Note note_;
    std::size_t tuning_length_;
};

// -------------------------------------------------------------------------------------

/**
 * Holds the Selected Sequence Indicator, the Interval Column and the actual Sequence.
 */
class Sequence : public Cell
{
  public:
    explicit Sequence(sequence::Sequence const &seq, std::size_t tuning_size);

  public:
    void select_child(std::vector<std::size_t> const &indices) override;

  protected:
    void paint(juce::Graphics &g) override;

    void resized() override;

  private:
    HomogenousRow<Cell> cells_;
};

// -------------------------------------------------------------------------------------

class BuildAndAllocateCell
{
  public:
    BuildAndAllocateCell(std::size_t tuning_octave_size);

  public:
    [[nodiscard]] auto operator()(sequence::Rest r) const -> std::unique_ptr<Cell>;

    [[nodiscard]] auto operator()(sequence::Note n) const -> std::unique_ptr<Cell>;

    [[nodiscard]] auto operator()(sequence::Sequence s) const -> std::unique_ptr<Cell>;

  private:
    std::size_t tos_;
};

} // namespace xen::gui