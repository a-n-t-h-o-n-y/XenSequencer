#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/pattern.hpp>
#include <sequence/sequence.hpp>
#include <sequence/tuning.hpp>

#include <xen/gui/homogenous_row.hpp>
#include <xen/scale.hpp>

namespace xen::gui
{

class Cell : public juce::Component
{
  public:
    float weight = 1.f;

  public:
    virtual void make_selected();

    /**
     * Makes this cell visually distinct from the default selection.
     * @details Used to mark a Cell that is part of the current Pattern.
     */
    void emphasize_selection(bool emphasized = true);

    /**
     * Used for updating the pattern of a sequence, Sequence is the only class that
     * implements this function.
     */
    virtual void update_pattern(sequence::Pattern const &pattern);

    /**
     * Find a child component by a list of indices.
     * @param indices The list of indices to traverse to find the child.
     * @return The child component if found, otherwise nullptr.
     */
    [[nodiscard]] virtual auto find_child(std::vector<std::size_t> const &indices)
        -> Cell *;

  public:
    void paintOverChildren(juce::Graphics &g) override;

  private:
    bool selected_ = false;
    bool emphasized_ = true;
};

// -------------------------------------------------------------------------------------

class Rest : public Cell
{
  public:
    explicit Rest(sequence::Rest, std::optional<Scale> const &scale,
                  sequence::Tuning const &tuning,
                  TranslateDirection scale_translate_direction);

  public:
    void paint(juce::Graphics &g) override;

  private:
    std::optional<Scale> scale_;
    sequence::Tuning tuning_;
    TranslateDirection scale_translate_direction_;
};

// -------------------------------------------------------------------------------------

class Note : public Cell
{
  public:
    Note(sequence::Note note, std::optional<Scale> const &scale,
         sequence::Tuning const &tuning, TranslateDirection scale_translate_direction);

  public:
    void paint(juce::Graphics &g) override;

  private:
    sequence::Note note_;
    std::optional<Scale> scale_;
    sequence::Tuning tuning_;
    TranslateDirection scale_translate_direction_;
};

// -------------------------------------------------------------------------------------

class Sequence : public Cell
{
  public:
    explicit Sequence(sequence::Sequence const &seq, std::optional<Scale> const &scale,
                      sequence::Tuning const &tuning,
                      TranslateDirection scale_translate_direction);

  public:
    void make_selected() override;

    void update_pattern(sequence::Pattern const &pattern) override;

    [[nodiscard]] auto find_child(std::vector<std::size_t> const &indices)
        -> Cell * override;

  public:
    void resized() override;

  private:
    HomogenousRow<Cell> cells_;
};

// -------------------------------------------------------------------------------------

class BuildAndAllocateCell
{
  public:
    BuildAndAllocateCell(std::optional<Scale> const &scale,
                         sequence::Tuning const &tuning,
                         TranslateDirection scale_translate_direction);

  public:
    [[nodiscard]] auto operator()(sequence::Rest r) const -> std::unique_ptr<Cell>;

    [[nodiscard]] auto operator()(sequence::Note n) const -> std::unique_ptr<Cell>;

    [[nodiscard]] auto operator()(sequence::Sequence s) const -> std::unique_ptr<Cell>;

  private:
    std::optional<Scale> scale_;
    sequence::Tuning tuning_;
    TranslateDirection scale_translate_direction_;
};

} // namespace xen::gui