#pragma once

#include <functional>
#include <type_traits>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace xen::gui
{

template <typename T>
class NumberBox : public juce::Component
{
    static_assert(std::is_floating_point<T>::value,
                  "Template argument T must be a floating point type");

  public:
    /** Clamps initial to range. */
    NumberBox(juce::NormalisableRange<T> range, T initial, int precision,
              bool editable = true, bool display_leading_zero = true,
              bool display_trailing_zero = false)
        : range_{range}, proportion_{range_.convertTo0to1(initial)}, initial_{initial},
          precision_{precision}, editable_{editable},
          display_leading_zero_{display_leading_zero},
          display_trailing_zero_{display_trailing_zero}
    {
        addAndMakeVisible(editor_);
        editor_.setEditable(false, true, false);
        editor_.setText(to_precision_string(initial, precision_, display_leading_zero_,
                                            display_trailing_zero_),
                        juce::dontSendNotification);
        editor_.addMouseListener(this, false);

        editor_.setWantsKeyboardFocus(true);
        editor_.setMouseClickGrabsKeyboardFocus(true);

        // editor_.onReturnKey = [this] { updateValueFromEditor(); };
        // editor_.onFocusLost = [this] { updateValueFromEditor(); };
        editor_.onTextChange = [this] { updateValueFromEditor(); };

        this->set_editable(editable_);
    }

  public:
    auto get_precision() const -> int
    {
        return precision_;
    }

    /** Clamps value to range. */
    virtual auto set_value(T value) -> void
    {
        if (!editable_ || (value == this->get_value()))
        {
            return;
        }

        value = snap_to_interval(value, initial_, range_.interval);

        proportion_ = range_.convertTo0to1(value);

        // get_value() call because it clamps the value to the range
        editor_.setText(to_precision_string(this->get_value(), precision_,
                                            display_leading_zero_,
                                            display_trailing_zero_),
                        juce::dontSendNotification);

        if (on_number_changed)
            on_number_changed(this->get_value());
    }

    auto get_value() const -> T
    {
        return range_.convertFrom0to1(proportion_);
    }

    auto set_editable(bool editable) -> void
    {
        editable_ = editable;
        editor_.setEditable(false, editable, false);
        // editor_.setReadOnly(!editable_);
    }

    [[nodiscard]] auto is_editable() const -> bool
    {
        return editable_;
    }

    auto increment(float factor = 1.f, bool use_last_click_proportion = false) -> void
    {
        T const increment_value = range_.interval * factor;
        T const proportion =
            use_last_click_proportion ? last_click_proportion_ : proportion_;
        T const offset = range_.convertFrom0to1(proportion);
        this->set_value(offset + increment_value);
    }

    auto decrement(float factor = 1.f, bool use_last_click_proportion = false) -> void
    {
        this->increment(-factor, use_last_click_proportion);
    }

    auto resized() -> void override
    {
        editor_.setBounds(getLocalBounds());
    }

  public:
    std::function<void(T)> on_number_changed;

  protected:
    void mouseDown(juce::MouseEvent const &e) override
    {
        last_mouse_position_ = e.position;
        last_click_proportion_ = proportion_;
    }

    void mouseDrag(juce::MouseEvent const &e) override
    {
        // Get the vertical distance from the last position
        float const distance = last_mouse_position_.y - e.y;

        // Calculate the factor
        float const factor = [&] {
            auto fac = e.mods.isCtrlDown() ? 0.1f : 1.0f;
            return e.mods.isShiftDown() ? fac * 10.f : fac;
        }();

        // Call increment() with factor * distance
        this->increment(factor * distance, true);
    }

  private:
    auto updateValueFromEditor() -> void
    {
        auto value = editor_.getText().getFloatValue();
        set_value(value);
    }

    static auto to_precision_string(T value, int precision, bool display_leading_zero,
                                    bool display_trailing_zero) -> juce::String
    {
        auto str = juce::String(value, precision);

        // Remove trailing zeros and decimal point if not needed
        if (!display_trailing_zero && str.contains("."))
        {
            str = str.trimCharactersAtEnd("0").trimCharactersAtEnd(".");
        }

        if (!display_leading_zero && str.startsWith("0"))
        {
            str = str.substring(1);
        }

        // Remove decimal point and everything after it if precision is zero
        if (precision == 0 && str.contains("."))
        {
            str = str.upToFirstOccurrenceOf(".", false, false);
        }

        return str;
    }

    [[nodiscard]] static auto snap_to_interval(T input, T initial, T interval) -> T
    {
        if (input >= initial)
            return initial + interval * std::floor((input - initial) / interval);
        else
            return initial - interval * std::floor((initial - input) / interval);
    }

  private:
    juce::NormalisableRange<T> range_;

  protected:
    T proportion_;
    T last_click_proportion_;

  private:
    T initial_; // used to calculate snap to interval
    juce::Label editor_;
    int precision_;
    juce::Point<float> last_mouse_position_;
    bool editable_;
    bool display_leading_zero_;
    bool display_trailing_zero_;
};

} // namespace xen::gui
