#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/sequence.hpp>
#include <sequence/tuning.hpp>

#include <xen/gui/homogenous_row.hpp>
#include <xen/scale.hpp>

namespace xen::gui
{

class Cell : public juce::Component
{
  public:
    virtual void select_child(std::vector<std::size_t> const &indices);

  public:
    void paintOverChildren(juce::Graphics &g) override;

  protected:
    virtual void make_selected();

  public:
    bool selected = false;
};

// -------------------------------------------------------------------------------------

class Rest : public Cell
{
  public:
    explicit Rest(sequence::Rest, std::optional<Scale> const &scale,
                  sequence::Tuning const &tuning);

  public:
    void paint(juce::Graphics &g) override;

  private:
    std::optional<Scale> scale_;
    sequence::Tuning tuning_;
};

// -------------------------------------------------------------------------------------

class Note : public Cell
{
  public:
    Note(sequence::Note note, std::optional<Scale> const &scale,
         sequence::Tuning const &tuning);

  public:
    void paint(juce::Graphics &g) override;

  private:
    sequence::Note note_;
    std::optional<Scale> scale_;
    sequence::Tuning tuning_;
};

// -------------------------------------------------------------------------------------

class Sequence : public Cell
{
  public:
    explicit Sequence(sequence::Sequence const &seq, std::optional<Scale> const &scale,
                      sequence::Tuning const &tuning);

  public:
    void select_child(std::vector<std::size_t> const &indices) override;

  public:
    void resized() override;

  protected:
    void make_selected() override;

  private:
    HomogenousRow<Cell> cells_;
};

// -------------------------------------------------------------------------------------

class BuildAndAllocateCell
{
  public:
    BuildAndAllocateCell(std::optional<Scale> const &scale,
                         sequence::Tuning const &tuning);

  public:
    [[nodiscard]] auto operator()(sequence::Rest r) const -> std::unique_ptr<Cell>;

    [[nodiscard]] auto operator()(sequence::Note n) const -> std::unique_ptr<Cell>;

    [[nodiscard]] auto operator()(sequence::Sequence s) const -> std::unique_ptr<Cell>;

  private:
    std::optional<Scale> scale_;
    sequence::Tuning tuning_;
};

} // namespace xen::gui