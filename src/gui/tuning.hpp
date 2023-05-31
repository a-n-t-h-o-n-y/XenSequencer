#pragma once

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iterator>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "file_button.hpp"
#include "heading.hpp"
#include "homogenous_row.hpp"
#include "mantissa_box.hpp"
#include "number_box.hpp"

#include <sequence/tuning.hpp>

namespace xen::gui
{

/**
 * @brief Display a floating point number in separate integer and fractional parts.
 */
class SplitFloatBox : public juce::Component
{
  public:
    /**
     * @brief Construct a SplitFloatBox.
     *
     * @param range The range of the integral part. This is used for the integral part
     * only, it should have an interval of at least 1.
     * @param initial The initial value.
     * @param precision The number of decimal places to display.
     * @param editable Whether the value can be edited.
     */
    SplitFloatBox(juce::NormalisableRange<float> const &range, float initial,
                  int precision, bool editable = true)
        : integral_{range, std::floor(initial), 0, editable, true},
          fractional_{initial, precision, editable}
    {
        addAndMakeVisible(integral_);
        addAndMakeVisible(fractional_);

        integral_.on_number_changed = [this](float) {
            this->on_number_changed(this->get_value());
        };

        fractional_.on_number_changed = [this](float) {
            this->on_number_changed(this->get_value());
        };

        fractional_.on_overflow = [this](int amount) {
            integral_.set_value(integral_.get_value() + static_cast<float>(amount));
        };
    }

  public:
    auto set_value(float value) -> void
    {
        // This will cause on_number_changed to be emitted twice, but most modifications
        // will not happen through this function.
        integral_.set_value(std::floor(value));
        fractional_.set_value(std::fmod(value, 1.f));
    }

    [[nodiscard]] auto get_value() const -> float
    {
        return integral_.get_value() + fractional_.get_value();
    }

    auto set_editable(bool editable) -> void
    {
        integral_.set_editable(editable);
        fractional_.set_editable(editable);
    }

    [[nodiscard]] auto is_editable() const -> bool
    {
        return integral_.is_editable() && fractional_.is_editable();
    }

  public:
    std::function<void(float)> on_number_changed;

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::column;

        flexbox.items.add(juce::FlexItem{integral_}.withFlex(1.f));
        flexbox.items.add(juce::FlexItem{fractional_}.withFlex(1.f));

        flexbox.performLayout(getLocalBounds().toFloat());
    }

    auto paint(juce::Graphics &g) -> void override
    {
        // set all children having a black background and white foreground
        for (auto child : getChildren())
        {
            child->setColour(juce::Label::backgroundColourId, juce::Colours::black);
            child->setColour(juce::Label::textColourId, juce::Colours::white);
        }

        // set the current drawing color
        g.setColour(juce::Colours::white);

        // draw an outline around the component
        g.drawRect(getLocalBounds(), 1);

        // draw an outline around the component with rounded corners
        // g.drawRoundedRectangle(getLocalBounds().toFloat(), 10, 1);

        // add border between integral and fractional parts
        auto integralBounds = integral_.getBounds();
        auto fractionalBounds = fractional_.getBounds();

        int midHeight = (integralBounds.getBottom() + fractionalBounds.getY()) / 2;

        g.setColour(juce::Colours::white);
        g.drawLine(0.0f, static_cast<float>(midHeight), static_cast<float>(getWidth()),
                   static_cast<float>(midHeight));
    }

  private:
    NumberBox<float> integral_;
    MantissaBox<float> fractional_;
};

/**
 * @brief A NumberBox for displaying a single tuning interval.
 */
class IntervalBox : public SplitFloatBox
{
    // TODO give this a specific width. Then in the parent, rely on this width for
    // flexbox
  public:
    explicit IntervalBox(float initial = 0.f)
        : SplitFloatBox{juce::NormalisableRange<float>{0.f, 10'000.f, 1.f}, initial, 6}
    {
    }
};

class IntervalRow : public HomogenousRow<IntervalBox>
{
  public:
    using Intervals_t = std::vector<typename sequence::Tuning::Interval_t>;

    IntervalRow() : HomogenousRow<IntervalBox>{juce::FlexItem{}.withWidth(60.f)}
    {
    }

  public:
    /**
     * @brief Removes all previous intervals and resets the display and state
     * to the given intervals.
     *
     * @param intervals The intervals to display.
     * @throws std::out_of_range if intervals.size() == 0
     * @throws std::invalid_argument if any interval is < 0.f
     * @throws std::invalid_argument if any interval is > 10'000.f
     */
    auto reset(Intervals_t intervals) -> void
    {
        if (intervals.size() == 0)
        {
            throw std::out_of_range{"IntervalRow::reset: intervals.size() == 0"};
        }
        if (std::any_of(std::begin(intervals), std::end(intervals), [](auto interval) {
                return interval < 0.f || interval > 10'000.f;
            }))
        {
            throw std::invalid_argument{"IntervalRow::reset: interval(s) out of range"};
        }

        this->clear();
        intervals_ = std::move(intervals);

        for (auto index = 0; index < intervals_.size(); ++index)
        {
            this->emplace_back(intervals_[index]).on_number_changed =
                [this, index](float cents) {
                    intervals_[index] = cents;
                    this->emit_intervals_change();
                };
        }

        // Zero interval is always zero.
        if (this->size() != 0)
            this->begin()->set_editable(false);

        this->emit_intervals_change();
    }

    /**
     * @brief Inserts a new interval at the given index with value zero.
     *
     * @param at The index to insert at.
     * @throws std::out_of_range if at > intervals_.size()
     */
    auto insert(std::size_t at) -> void
    {
        if (at > intervals_.size())
        {
            throw std::out_of_range{"IntervalRow::insert: at > intervals_.size()"};
        }
        auto next = intervals_;
        next.insert(std::next(std::begin(next), at), 0.f);
        this->reset(std::move(next));
    }

    /**
     * @brief Erases the interval at the given index.
     *
     * @param at The index to erase.
     * @throws std::out_of_range if at >= intervals_.size()
     */
    auto erase(std::size_t at) -> void
    {
        if (at >= intervals_.size())
        {
            throw std::out_of_range{"IntervalRow::erase: at >= intervals_.size()"};
        }
        auto next = intervals_;
        next.erase(std::next(std::begin(next), at));
        this->reset(std::move(next));
    }

  public:
    std::function<void(Intervals_t const &)> on_intervals_change;

  private:
    auto emit_intervals_change() -> void
    {
        if (on_intervals_change)
        {
            this->on_intervals_change(intervals_);
        }
    }

  private:
    Intervals_t intervals_;
};

class PlusButton : public juce::TextButton
{
  public:
    PlusButton() : juce::TextButton{"[+]"}
    {
    }

  public:
    std::function<void()> &on_click = this->juce::TextButton::onClick;
};

/**
 * @brief A NumberBox for displaying a the octave interval and label.
 */
class OctaveBox : public juce::Component
{
  public:
    OctaveBox()
    {
        this->addAndMakeVisible(label_);
        this->addAndMakeVisible(interval_box_);
    }

  public:
    /**
     * @brief Sets the octave interval to the given value.
     *
     * Clamps the value to the range [0.f, 10'000.f].
     *
     * @param cents The octave interval in cents.
     */
    auto reset(float cents) -> void
    {
        interval_box_.set_value(cents);
    }

  public:
    std::function<void(float)> &on_number_changed = interval_box_.on_number_changed;

  protected:
    auto resized() -> void override
    {
        // use flexbox to layout label on top of interval_box
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::column;

        flexbox.items.add(juce::FlexItem{label_}.withFlex(1.f));
        flexbox.items.add(juce::FlexItem{interval_box_}.withFlex(1.1f));

        flexbox.performLayout(getLocalBounds());
    }

  private:
    Heading label_{"Octave", 5, juce::Font{"Arial", "Bold", 14.f}};
    IntervalBox interval_box_{0.f};
};

/**
 * @brief A row of IntervalBoxes, add interval button, and OctaveBox.
 */
class TuningRow : public juce::Component
{
  public:
    TuningRow()
    {
        this->addAndMakeVisible(interval_row_);
        this->addAndMakeVisible(plus_btn_);
        this->addAndMakeVisible(octave_box_);

        interval_row_.on_intervals_change = [this](auto const &intervals) {
            tuning_.intervals = intervals;
            this->emit_tuning_change();
        };

        octave_box_.on_number_changed = [this](auto cents) {
            tuning_.octave = cents;
            this->emit_tuning_change();
        };

        plus_btn_.on_click = [this]() { interval_row_.insert(interval_row_.size()); };

        auto edo12 = sequence::Tuning{
            {
                0.f,
                100.f,
                200.f,
                300.f,
                400.f,
                500.f,
                600.f,
                700.f,
                800.f,
                900.f,
                1000.f,
                1100.f,
            },
            1200.f,
        };

        this->reset(std::move(edo12));
    }

  public:
    /**
     * @brief Removes all previous tuning intervals and resets the display and state
     * to the given tuning.
     *
     * @param tuning The tuning to display.
     */
    auto reset(sequence::Tuning tuning) -> void
    {
        interval_row_.reset(tuning.intervals);
        octave_box_.reset(tuning.octave);
    }

    /**
     * @brief Returns the current tuning.
     */
    auto tuning() const -> sequence::Tuning const &
    {
        return tuning_;
    }

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::row;

        flexbox.items.add(juce::FlexItem(interval_row_).withFlex(1.f));
        flexbox.items.add(juce::FlexItem{plus_btn_}.withWidth(40.f));
        flexbox.items.add(juce::FlexItem(octave_box_).withWidth(60.f));

        flexbox.performLayout(getLocalBounds());
    }

    void paint(juce::Graphics &g) override
    {
        g.fillAll(juce::Colours::black);
    }

  private:
    auto emit_tuning_change() -> void
    {
        if (on_tuning_changed)
        {
            on_tuning_changed(tuning_);
        }
    }

  public:
    std::function<void(sequence::Tuning const &)> on_tuning_changed;

  private:
    IntervalRow interval_row_;
    PlusButton plus_btn_;
    OctaveBox octave_box_;

    sequence::Tuning tuning_;
};

class BottomRow : public juce::Component
{
  public:
    BottomRow()
    {
        this->addAndMakeVisible(load_file_btn_);
        this->addAndMakeVisible(save_file_btn_);
    }

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::row;

        flexbox.items.add(juce::FlexItem(load_file_btn_).withFlex(1.f));
        flexbox.items.add(juce::FlexItem(save_file_btn_).withFlex(1.f));

        flexbox.performLayout(getLocalBounds());
    }

  public:
    std::function<void(std::filesystem::path const &)> &on_load_file_selected =
        load_file_btn_.on_file_selected;
    std::function<void(std::filesystem::path const &)> &on_save_file_selected =
        save_file_btn_.on_file_selected;

  private:
    LoadFileButton load_file_btn_{"Load File", "Select a scala file to open", "*.scl"};
    SaveFileButton save_file_btn_{"Save File", "Create a file to save to.", "*.scl"};
};

class TuningBox : public juce::Component
{
  public:
    TuningBox()
    {
        this->addAndMakeVisible(heading_);
        this->addAndMakeVisible(tuning_row_);
        this->addAndMakeVisible(bottom_row_);

        bottom_row_.on_load_file_selected = [this](auto const &file) {
            auto const tuning = sequence::from_scala(file);
            tuning_row_.reset(tuning);
        };

        bottom_row_.on_save_file_selected = [this](auto const &file) {
            auto const tuning = tuning_row_.tuning();
            (void)file;
            sequence::to_scala(tuning, file);
        };
    }

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::column;

        flexbox.items.add(juce::FlexItem(heading_).withHeight(
            static_cast<float>(heading_.getHeight())));
        flexbox.items.add(juce::FlexItem(tuning_row_).withFlex(1.0f));
        flexbox.items.add(juce::FlexItem(bottom_row_).withFlex(1.0f));

        flexbox.performLayout(this->getLocalBounds());
    }

  public:
    std::function<void(sequence::Tuning const &)> &on_tuning_changed =
        tuning_row_.on_tuning_changed;

  private:
    Heading heading_{"Tuning"};
    TuningRow tuning_row_;
    BottomRow bottom_row_;
};

} // namespace xen::gui