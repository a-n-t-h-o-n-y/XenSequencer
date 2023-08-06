#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <variant>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/sequence.hpp>
#include <sequence/utility.hpp>

#include "focusable_component.hpp"
#include "homogenous_row.hpp"

namespace xen::gui
{

class Cell : public FocusableComponent
{
  public:
    [[nodiscard]] virtual auto get_cell_data() const -> sequence::Cell = 0;

  public:
    // Signals - Do not assign to these, they are already assigned, instead call
    // them as a function if needed.

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

    /**
     * @brief Callback for when a cell swap request is made.
     *
     * A cell swap is when this cell wants to be deleted and replaced with a new cell.
     * Be careful with this callback, once it is called it will have deleted *this and
     * should immediately return.
     *
     * @param std::unique_ptr<Cell> The new cell to replace *this with.
     */
    std::function<void(std::unique_ptr<Cell>)> on_cell_swap_request;

  public:
    /**
     * @brief An abstract increment function that is used by keyboard input.
     *
     * Any positive or negative amount can be passed to this function.
     *
     * @param amount The amount to increment by.
     */
    virtual auto increment_interval(int amount) -> void = 0;

    virtual auto increment_octave(int amount) -> void = 0;

    virtual auto increment_delay(float amount) -> void = 0;

    virtual auto increment_gate(float amount) -> void = 0;

    virtual auto increment_velocity(float amount) -> void = 0;

    virtual auto set_tuning_length(std::size_t length) -> void = 0;

    /**
     * @brief This should flip between cell types, rest to note and note to rest.
     */
    virtual auto flip_cell() -> void = 0;

  protected:
    auto emit_on_update() -> void
    {
        if (this->on_update)
        {
            this->on_update();
        }
    }

    auto paintOverChildren(juce::Graphics &g) -> void override
    {
        // Draw Border
        g.setColour(juce::Colours::white);

        auto const bounds = getLocalBounds();
        auto const left_x = (float)bounds.getX();
        auto const right_x = (float)bounds.getRight();

        g.drawLine(left_x, (float)bounds.getY(), left_x, (float)bounds.getBottom(), 1);

        // draw split_preview_ count vertical lines evenly between the start and end
        if (dragging_ && split_preview_ != 0)
        {
            g.setColour(juce::Colours::grey);

            auto const width = right_x - left_x;
            auto const interval = width / (split_preview_ + 1);

            for (int i = 1; i <= split_preview_; ++i)
            {
                auto const x = left_x + interval * i;
                g.drawLine(x, (float)bounds.getY(), x, (float)bounds.getBottom(), 1);
            }
        }
        this->FocusableComponent::paint(g);
    }

    auto keyPressed(juce::KeyPress const &key) -> bool override
    {
        // TODO probably use a switch statement here instead though maybe can't use
        // multiple checks there..

        { // Enter Key
            if (key.isKeyCode(juce::KeyPress::returnKey))
            {
                this->flip_cell();
                return true;
            }
        }

        if (key.getModifiers().isAltDown())
        {
            { // gate
                auto const gate =
                    key.isKeyCode(juce::KeyPress::leftKey)
                        ? -0.01f
                        : (key.isKeyCode(juce::KeyPress::rightKey) ? 0.01f : 0.0f);
                auto const multiplier = key.getModifiers().isShiftDown() ? 3 : 1;
                this->increment_gate(gate * multiplier);
                if (gate != 0.f)
                {
                    return true;
                }
            }
            { // velocity
                auto const velocity =
                    key.isKeyCode(juce::KeyPress::downKey)
                        ? -0.01f
                        : (key.isKeyCode(juce::KeyPress::upKey) ? 0.01f : 0.0f);
                auto const multiplier = key.getModifiers().isShiftDown() ? 3 : 1;
                this->increment_velocity(velocity * multiplier);
                if (velocity != 0.f)
                {
                    return true;
                }
            }
        }

        { // Interval
            auto const interval =
                key.isKeyCode(juce::KeyPress::upKey)
                    ? 1
                    : (key.isKeyCode(juce::KeyPress::downKey) ? -1 : 0);
            if (key.getModifiers().isShiftDown())
            {
                this->increment_octave(interval);
            }
            else
            {
                this->increment_interval(interval);
            }
            if (interval != 0)
            {
                return true;
            }
        }

        { // delay
            auto const delay =
                key.isKeyCode(juce::KeyPress::leftKey)
                    ? -0.01f
                    : (key.isKeyCode(juce::KeyPress::rightKey) ? 0.01f : 0.0f);
            auto const multiplier = key.getModifiers().isShiftDown() ? 3 : 1;
            this->increment_delay(delay * multiplier);
            if (delay != 0.f)
            {
                return true;
            }
        }

        // TODO key input for split preview then commit to split

        return FocusableComponent::keyPressed(key);
    }

    [[nodiscard]] auto is_dragging() const -> bool
    {
        return dragging_;
    }

    [[nodiscard]] auto drag_start_position() const -> juce::Point<float>
    {
        return drag_start_position_;
    }

  protected:
    auto mouseDown(juce::MouseEvent const &) -> void override;

    auto mouseDrag(juce::MouseEvent const &) -> void override;

    auto mouseUp(juce::MouseEvent const &) -> void override;

  protected:
    /**
     * @brief Get the increment for a given number of units.
     *
     * Units are going to be pixels in this context. Used for mouse drag.
     *
     * @param units_per_increment The number of units per increment.
     * @param units The number of units to calculate an increment for.
     * @param multiplier A multiplier to apply to the units before calculating the
     * increment.
     * @return int The increment for the given number of units.
     *
     * @throws std::out_of_range If units_per_increment is zero or less.
     * @throws std::out_of_range If buffer is less than zero.
     */
    [[nodiscard]] static auto get_increment(int units_per_increment, int units,
                                            float multiplier = 1.f, int buffer = 0)
        -> int
    {
        if (units_per_increment <= 0)
        {
            throw std::out_of_range("units_per_increment must be greater than zero");
        }
        if (buffer < 0)
        {
            throw std::out_of_range("buffer must be greater than or equal to zero");
        }

        if (std::abs(units) > buffer)
        {
            units = units > 0 ? units - buffer : units + buffer;
            return static_cast<int>(units > 0 ? std::floor(((float)units * multiplier) /
                                                           (float)units_per_increment)
                                              : std::ceil(((float)units * multiplier) /
                                                          (float)units_per_increment));
        }
        else
        {
            return 0;
        }
    }

  protected:
    auto set_split_preview(int count) -> void
    {
        if (count == split_preview_)
        {
            return;
        }
        split_preview_ = count;
        this->repaint();
    }

  private:
    bool dragging_ = false;
    juce::Point<float> drag_start_position_;
    int split_preview_ = 0;
};

class Rest : public Cell
{
  public:
    explicit Rest(sequence::Rest)
    {
        this->addMouseListener(this, true);

        this->addAndMakeVisible(label_);
        label_.setFont(juce::Font{"Arial", "Normal", 14.f}.boldened());
        label_.setColour(juce::Label::ColourIds::textColourId, juce::Colours::white);
        label_.setJustificationType(juce::Justification::centred);
    }

  public:
    [[nodiscard]] auto get_cell_data() const -> sequence::Cell override
    {
        return sequence::Rest{};
    }

  public:
    auto increment_interval(int) -> void override
    {
    }

    auto increment_octave(int) -> void override
    {
    }

    auto increment_delay(float) -> void override
    {
    }

    auto increment_gate(float) -> void override
    {
    }

    auto increment_velocity(float) -> void override
    {
    }

    auto flip_cell() -> void override;

    auto set_tuning_length(std::size_t) -> void override
    {
    }

  protected:
    auto resized() -> void override
    {
        label_.setBounds(getLocalBounds());
    }

    auto mouseUp(const juce::MouseEvent &event) -> void override;

  private:
    juce::Label label_{"R", "R"};
};

class NoteInterval : public juce::Component
{
  public:
    NoteInterval(int interval, std::size_t tuning_length, float velocity)
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

    auto set_tuning_length(std::size_t tuning_length)
    {
        if (tuning_length_ == tuning_length)
        {
            return;
        }
        tuning_length_ = tuning_length;
        this->repaint();
    }

    auto get_tuning_length() const -> std::size_t
    {
        return tuning_length_;
    }

    auto set_velocity(float vel) -> void
    {
        velocity_ = std::clamp(vel, 0.f, 1.f);
        bg_color_ = NoteInterval::get_color(juce::Colour{0xFFFF5B00}, velocity_);
        this->repaint();
    }

  protected:
    auto paint(juce::Graphics &g) -> void override;

  private:
    [[nodiscard]] static auto get_color(juce::Colour base_color, float velocity)
        -> juce::Colour
    {
        auto const brightness = std::lerp(0.3f, 1.f, velocity);
        return base_color.withBrightness(brightness);
    }

    [[nodiscard]] static auto get_interval_and_octave(int interval,
                                                      std::size_t tuning_length)
        -> std::pair<int, int>
    {
        auto octave = interval / (int)tuning_length;
        if (interval >= 0)
        {
            // For positive interval, use simple division and modulo operations
            return std::make_pair(interval % (int)tuning_length, octave);
        }
        else
        {
            // For negative interval, calculate the adjusted interval and octave
            int adjusted_interval =
                ((int)tuning_length + (interval % (int)tuning_length)) %
                (int)tuning_length;

            if (adjusted_interval != 0)
            {
                // Adjust Octave for negative intervals, the first negative octave is
                // -1, not zero.
                --octave;
            }

            return std::make_pair(adjusted_interval, octave);
        }
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
    // TODO should take a Tuning or tuning size in constructor
    // currently hardcoded as 12, below.
    explicit Note(sequence::Note note, std::size_t tuning_length)
        : note_(note), interval_box_{note.interval, tuning_length, note.velocity}
    {
        this->addMouseListener(this, true);

        this->addAndMakeVisible(interval_box_);
    }

  public:
    auto set_interval(int interval) -> void
    {
        interval = std::clamp(interval, -100, 100);
        if (note_.interval == interval)
        {
            return;
        }
        interval_box_.set_interval(note_.interval = interval);
        this->emit_on_update();
    }

    auto set_delay(float delay) -> void
    {
        delay = std::clamp(delay, 0.f, 0.99f);
        if (note_.delay == delay)
        {
            return;
        }
        note_.delay = delay;
        this->resized();
        this->emit_on_update();
    }

    auto set_gate(float gate) -> void
    {
        gate = std::clamp(gate, 0.01f, 1.f);
        if (note_.gate == gate)
        {
            return;
        }
        note_.gate = gate;
        this->resized();
        this->emit_on_update();
    }

  public:
    [[nodiscard]] auto get_cell_data() const -> sequence::Cell override
    {
        return note_;
    }

  public:
    auto increment_interval(int amount) -> void override
    {
        this->set_interval(note_.interval + amount);
    }

    auto increment_octave(int amount) -> void override
    {
        this->set_interval(note_.interval +
                           (amount * (int)interval_box_.get_tuning_length()));
    }

    auto increment_delay(float amount) -> void override
    {
        this->set_delay(note_.delay + amount);
    }

    auto increment_gate(float amount) -> void override
    {
        this->set_gate(note_.gate + amount);
    }

    auto increment_velocity(float amount) -> void override
    {
        auto const velocity = std::clamp(note_.velocity + amount, 0.f, 1.f);
        if (note_.velocity == velocity)
        {
            return;
        }
        interval_box_.set_velocity(note_.velocity = velocity);
        this->emit_on_update();
    }

    auto flip_cell() -> void override
    {
        if (this->on_cell_swap_request)
        {
            // Warning: This call will delete *this, do not access any data, or do
            // anything after this call!
            this->on_cell_swap_request(std::make_unique<Rest>(sequence::Rest{}));
            return;
        }
    }

    auto set_tuning_length(std::size_t tuning_length) -> void override
    {
        interval_box_.set_tuning_length(tuning_length);
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

    auto mouseDown(juce::MouseEvent const &) -> void override;

    auto mouseDrag(juce::MouseEvent const &) -> void override;

    auto mouseUp(juce::MouseEvent const &) -> void override;

    auto mouseWheelMove(juce::MouseEvent const &event,
                        juce::MouseWheelDetails const &wheel) -> void override;

  private:
    sequence::Note note_;
    int initial_interval_ = 0;
    float initial_delay_ = 0.f;
    float initial_gate_ = 0.f;

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
            this->attach_to_all_signals(this->push_back_cell(cell), i);
            ++i;
        }

        this->emit_on_update();
    }

  public:
    auto increment_interval(int amount) -> void override
    {
        for (auto &cell : cells_)
        {
            cell.increment_interval(amount);
        }
    }

    auto increment_octave(int amount) -> void override
    {
        for (auto &cell : cells_)
        {
            cell.increment_octave(amount);
        }
    }

    auto increment_delay(float amount) -> void override
    {
        for (auto &cell : cells_)
        {
            cell.increment_delay(amount);
        }
    }

    auto increment_gate(float amount) -> void override
    {
        for (auto &cell : cells_)
        {
            cell.increment_gate(amount);
        }
    }

    auto increment_velocity(float amount) -> void override
    {
        for (auto &cell : cells_)
        {
            cell.increment_velocity(amount);
        }
    }

    auto flip_cell() -> void override
    {
        for (auto &cell : cells_)
        {
            cell.flip_cell();
        }
    }

    auto set_tuning_length(std::size_t tuning_length) -> void override
    {
        tuning_length_ = tuning_length;
        for (auto &cell : cells_)
        {
            cell.set_tuning_length(tuning_length);
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
        return cells_.push_back(
            std::visit(sequence::utility::overload{
                           [](sequence::Rest const &rest) -> std::unique_ptr<Cell> {
                               return std::make_unique<Rest>(rest);
                           },
                           [this](sequence::Note const &note) -> std::unique_ptr<Cell> {
                               return std::make_unique<Note>(note, tuning_length_);
                           },
                           [](sequence::Sequence const &seq) -> std::unique_ptr<Cell> {
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
    auto attach_to_split_request_signal(::xen::gui::Cell &gui_cell, std::size_t index)
        -> void
    {

        gui_cell.on_split_request = [this, index](sequence::Cell const &cell,
                                                  std::size_t count) -> void {
            if (count < 2)
            {
                return;
            }

            auto new_seq = std::make_unique<::xen::gui::SubSequence>();
            ::xen::gui::SubSequence &new_seq_ref = *new_seq;
            this->attach_to_update_signal(new_seq_ref);

            bool const had_focus = cells_.at(index).hasKeyboardFocus(false);

            auto original_cell = cells_.exchange(index, std::move(new_seq));

            auto const duplicates = sequence::Sequence{
                std::vector(count, cell),
            };
            new_seq_ref.set(duplicates, false);
            if (had_focus)
            {
                new_seq_ref.grabKeyboardFocus();
            }

            original_cell.reset();
            // Warning: Do not call anything after this, *this has been deleted.
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
        cell.on_update = [this] { this->emit_on_update(); };
    }

    auto attach_to_cell_swap(::xen::gui::Cell &cell, std::size_t index) -> void
    {
        cell.on_cell_swap_request = [this, index](std::unique_ptr<Cell> new_cell) {
            this->attach_to_all_signals(*new_cell, index);
            bool const had_focus = cells_.at(index).hasKeyboardFocus(false);
            auto &new_cell_ref = *new_cell;

            auto to_delete = cells_.exchange(index, std::move(new_cell));

            // Explicit call here because not using set(...);
            this->emit_on_update();

            if (had_focus)
            {
                new_cell_ref.grabKeyboardFocus();
            }

            to_delete.reset();
            // Warning: Do not add any code below!
        };
    }

    auto attach_to_all_signals(::xen::gui::Cell &cell, std::size_t index) -> void
    {
        this->attach_to_split_request_signal(cell, index);
        this->attach_to_update_signal(cell);
        this->attach_to_cell_swap(cell, index);
    }

  private:
    HomogenousRow<Cell> cells_;
    std::size_t tuning_length_;
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

    auto set_tuning_length(std::size_t length) -> void
    {
        sub_sequence_.set_tuning_length(length);
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