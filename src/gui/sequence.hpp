#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
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

    /**
     * @brief Callback for when the cell is updated.
     *
     * This is used by derived classes to notify of changes to the sequencer. These
     * events should eventually cause a sequence::Sequence and sequence::Phrase to be
     * generated and sent to the audio processor.
     */
    std::function<void()> on_update;

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
        label_.setFont(juce::Font{"Arial", "Normal", 14.f});
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

    auto paintOverChildren(juce::Graphics &g) -> void override
    {
        g.setColour(juce::Colours::white);

        auto const bounds = getLocalBounds();
        auto const left_x = (float)bounds.getX();

        g.drawLine(left_x, (float)bounds.getY(), left_x, (float)bounds.getBottom(), 1);
    }

  private:
    juce::Label label_{"R", "R"};
};

class NoteInterval : public juce::Component
{
  public:
    NoteInterval(int interval, int tuning_length, float velocity)
        : interval_{interval}, tuning_length_{tuning_length}
    {
        // Called explicity to generate color
        this->set_velocity(velocity);
    }

  public:
    auto set_interval(int interval)
    {
        interval_ = interval;
        this->repaint();
    }

    auto set_tuning_length(int tuning_length)
    {
        tuning_length_ = tuning_length;
        this->repaint();
    }

    auto set_velocity(float vel) -> void
    {
        velocity_ = std::clamp(vel, 0.f, 1.f);
        bg_color_ = NoteInterval::get_color(juce::Colour{0xFFFF5B00}, velocity_);
        this->repaint();
    }

  protected:
    auto paint(juce::Graphics &g) -> void override
    {
        g.fillAll(bg_color_);
    }

  private:
    [[nodiscard]] static auto get_color(juce::Colour base_color, float velocity)
        -> juce::Colour
    {
        auto const brightness = std::lerp(0.2f, 1.f, velocity);
        return base_color.withBrightness(brightness);
    }

  private:
    int interval_;
    int tuning_length_;
    float velocity_;

    juce::Colour bg_color_;
};

class Note : public Cell
{
  public:
    // TODO should take a Tuning or tuning size in constructor
    explicit Note(sequence::Note note)
        : note_(note), interval_box_{note.interval, 12, note.velocity}
    {
        this->addMouseListener(this, true);

        this->addAndMakeVisible(interval_box_);

        // label_.setFont(juce::Font{"Arial", "Bold", 14.f});
        // label_.setColour(juce::Label::ColourIds::textColourId, juce::Colours::blue);
    }

  public:
    [[nodiscard]] auto get_cell_data() const -> sequence::Cell override
    {
        return note_;
    }

  protected:
    auto resized() -> void override
    {
        // TODO use delay and gate to set bounds
        interval_box_.setBounds(this->getLocalBounds());
    }

    auto paintOverChildren(juce::Graphics &g) -> void override
    {
        g.setColour(juce::Colours::white);

        auto const bounds = getLocalBounds();
        auto const left_x = (float)bounds.getX();

        g.drawLine(left_x, (float)bounds.getY(), left_x, (float)bounds.getBottom(), 1);
    }

  private:
    sequence::Note note_;

  private:
    NoteInterval interval_box_;
};

class SubSequence : public Cell
{
  public:
    explicit SubSequence(sequence::Sequence sequence = {})
    {
        this->addAndMakeVisible(cells_);

        this->set(sequence);
    }

  public:
    [[nodiscard]] auto get_cell_data() const -> sequence::Cell override
    {
        auto seq = sequence::Sequence{};
        for (auto const &cell : cells_)
        {
            seq.cells.push_back(cell.get_cell_data());
        }
        return seq;
    }

    /**
     * @brief Set the SubSequence's data with a sequence::Sequence.
     */
    auto set(sequence::Sequence const &sequence, bool clear = true) -> void
    {
        if (clear)
        {
            cells_.clear();
        }

        auto i = cells_.size();
        for (auto const &cell : sequence.cells)
        {
            Cell &new_cell = this->push_back_cell(cell);
            this->attach_to_split_request_signal(new_cell, i);
            this->attach_to_update_signal(new_cell);
            ++i;
        }

        if (this->Cell::on_update)
        {
            this->Cell::on_update();
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
                    return std::make_unique<SubSequence>(seq);
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
            // this newly created sequence might not have any signals attached
            // its that this is a child of *this and usually the child's signal
            // is assigned a lambda to trigger the parent's signal, but that doesn't
            // happen here.
            auto new_seq = std::make_unique<::xen::gui::SubSequence>();
            ::xen::gui::SubSequence &new_seq_ref = *new_seq;
            this->attach_to_update_signal(new_seq_ref);

            auto original_cell = cells_.exchange(index, std::move(new_seq));
            ::xen::gui::Cell &original_cell_ref = *original_cell;

            new_seq_ref.cells_.push_back(std::move(original_cell));
            new_seq_ref.attach_to_split_request_signal(original_cell_ref, 0);
            new_seq_ref.attach_to_update_signal(original_cell_ref);

            auto const duplicates = [&] {
                auto x = sequence::Sequence{};
                for (auto i = 1; i < count; ++i)
                    x.cells.push_back(cell);
                return x;
            }();
            new_seq_ref.set(duplicates, false);
        };
    }

    /**
     * @brief Attach to the update signal of a child Cell so this will emit its own
     * update signal.
     *
     * @param cell The Cell to attach to.
     */
    auto attach_to_update_signal(::xen::gui::Cell &cell) -> void
    {
        cell.on_update = [this] {
            if (this->Cell::on_update)
            {
                this->Cell::on_update();
            }
        };
    }

  private:
    HomogenousRow<Cell> cells_;
};

class Sequence : public juce::Component
{
  public:
    explicit Sequence(sequence::Sequence sequence = {}) : sub_sequence_{}
    {
        this->addAndMakeVisible(sub_sequence_);
        this->set(sequence);
    }

  public:
    auto set(sequence::Sequence const &sequence) -> void
    {
        sub_sequence_.set(sequence);
        sub_sequence_.on_update = [this] {
            if (this->on_update)
            {
                this->on_update();
            }
        };
    }

    /**
     * @brief Convinience wrapper around get_cell_data which returns a
     * sequence::Sequence instead of sequence::Cell.
     *
     * @return sequence::Sequence The sequence::Sequence data.
     */
    [[nodiscard]] auto get_sequence() const -> sequence::Sequence
    {
        sequence::Cell data = sub_sequence_.get_cell_data();
        if (std::holds_alternative<sequence::Sequence>(data))
        {
            return std::get<sequence::Sequence>(data);
        }
        else
        {
            throw std::logic_error{"Sequence::get_sequence() called on a "
                                   "Sequence that does not contain a "
                                   "sequence::Sequence."};
        }
    }

  public:
    std::function<void()> on_update;

  protected:
    auto resized() -> void override
    {
        sub_sequence_.setBounds(this->getLocalBounds());
    }

  private:
    SubSequence sub_sequence_;
};

} // namespace xen::gui