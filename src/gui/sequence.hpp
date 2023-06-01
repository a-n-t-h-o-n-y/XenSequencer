#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <variant>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/sequence.hpp>
#include <sequence/utility.hpp>

#include "homogenous_row.hpp"

namespace xen::gui
{

class Cell : public juce::Component
{
  public:
    [[nodiscard]] virtual auto get_cell_data() const -> sequence::Cell = 0;

  public:
    /**
     * @brief Callback for when a split request is made.
     *
     * A split request is to transform a single Note or Rest into a Sequence of
     * duplicate Notes or Rests.
     *
     * @param sequence::Cell The Cell's data to be duplicated across new cells.
     * @param std::size_t The number of cells to split into.
     */
    std::function<void(sequence::Cell const &, std::size_t)> on_split_request;

  protected:
    auto mouseDoubleClick(juce::MouseEvent const &) -> void override
    {
        if (this->on_split_request)
        {
            auto const test_split = 3;
            this->on_split_request(this->get_cell_data(), test_split);
        }
    }
};

class Rest : public Cell
{
  public:
    explicit Rest(sequence::Rest)
    {
        this->addMouseListener(this, true);

        this->addAndMakeVisible(label_);
        label_.setFont(juce::Font{"Arial", "Bold", 14.f});
        label_.setColour(juce::Label::ColourIds::textColourId, juce::Colours::white);
    }

  public:
    [[nodiscard]] auto get_cell_data() const -> sequence::Cell override
    {
        return sequence::Rest{};
    }

  protected:
    auto resized() -> void override
    {
        label_.setBounds(getLocalBounds());
    }

    auto paint(juce::Graphics &g) -> void override
    {
        // set the current drawing color
        g.setColour(juce::Colours::white);

        // draw an outline around the component
        g.drawRect(getLocalBounds(), 1);
    }

  private:
    juce::Label label_{"Rest", "Rest"};
};

class Note : public Cell
{
  public:
    explicit Note(sequence::Note note) : note_(note)
    {
        this->addMouseListener(this, true);

        this->addAndMakeVisible(label_);
        label_.setFont(juce::Font{"Arial", "Bold", 14.f});
        label_.setColour(juce::Label::ColourIds::textColourId, juce::Colours::blue);
    }

  public:
    [[nodiscard]] auto get_cell_data() const -> sequence::Cell override
    {
        return note_;
    }

    // TODO signal saying to split into x pieces, emitting note_ data, as a
    // sequence::Cell probably
  protected:
    auto resized() -> void override
    {
        label_.setBounds(getLocalBounds());
    }

    auto paint(juce::Graphics &g) -> void override
    {
        // set the current drawing color
        g.setColour(juce::Colours::white);

        // draw an outline around the component
        g.drawRect(getLocalBounds(), 1);
    }

  private:
    sequence::Note note_;

  private:
    juce::Label label_{"Note", "Note"};
};

class Sequence : public Cell
{
  public:
    explicit Sequence(sequence::Sequence sequence = {})
    {
        this->addAndMakeVisible(cells_);

        this->set(sequence);
    }

  public:
    [[nodiscard]] auto get_cell_data() const -> sequence::Cell override
    {
        // TODO generate by iterating over children.
        return {};
    }

    /**
     * @brief Set the Sequence's data with a sequence::Sequence.
     */
    auto set(sequence::Sequence const &sequence) -> void
    {
        cells_.clear();

        for (auto i = 0; i < sequence.cells.size(); ++i)
        {
            Cell &new_cell = this->push_back_cell(sequence.cells[i]);
            this->attach_to_split_request_signal(new_cell, i);
        }
    }

  protected:
    auto resized() -> void override
    {
        cells_.setBounds(this->getLocalBounds());
    }

  private:
    /**
     * @brief Transform a sequence::Cell into a gui::Cell and push it onto the end of
     * the cells_ sequence.
     *
     * @param cell The sequence::Cell to transform and push onto the end of the
     * cells_ sequence.
     * @return Cell& A reference to the newly created gui::Cell.
     */
    auto push_back_cell(sequence::Cell const &cell) -> ::xen::gui::Cell &
    {
        return cells_.push_back(std::visit(
            sequence::utility::overload{
                [this](sequence::Rest const &rest) -> std::unique_ptr<Cell> {
                    return std::make_unique<Rest>(rest);
                },
                [this](sequence::Note const &note) -> std::unique_ptr<Cell> {
                    return std::make_unique<Note>(note);
                },
                [this](sequence::Sequence const &seq) -> std::unique_ptr<Cell> {
                    return std::make_unique<Sequence>(seq);
                },
            },
            cell));
    }

    /**
     * @brief Attach to the split request signal of a Cell.
     *
     * The passed in cell should be a child Cell of this, this allows this new child
     * cell to also be split when requested.
     *
     * @param cell The Cell to attach to.
     * @param index The index of the Cell in the cells_ sequence.
     */
    auto attach_to_split_request_signal(::xen::gui::Cell &cell, std::size_t index)
        -> void
    {

        cell.on_split_request = [this, index](sequence::Cell const &cell,
                                              std::size_t count) -> void {
            if (count < 2)
            {
                return;
            }
            auto new_seq = std::make_unique<::xen::gui::Sequence>();
            ::xen::gui::Sequence &new_seq_ref = *new_seq;

            auto original_cell = cells_.exchange(index, std::move(new_seq));
            ::xen::gui::Cell &original_cell_ref = *original_cell;

            new_seq_ref.cells_.push_back(std::move(original_cell));
            new_seq_ref.attach_to_split_request_signal(original_cell_ref, 0);

            for (auto i = 1; i < count; ++i)
            {
                auto &new_gui_cell = new_seq_ref.push_back_cell(cell);
                new_seq_ref.attach_to_split_request_signal(new_gui_cell, i);
            }
        };
    }

  private:
    HomogenousRow<Cell> cells_;
};

} // namespace xen::gui