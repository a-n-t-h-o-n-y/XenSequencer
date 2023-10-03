#pragma once

#include <cmath>
#include <functional>

#include "number_box.hpp"

namespace xen::gui
{

template <typename T>
class MantissaBox : public NumberBox<T>
{
  public:
    /**
     * @brief Constructs a MantissaBox to display the fractional part of a floating
     * point number.
     *
     * If `initial` is not in the range [0, 1), the integral part is discarded.
     *
     * @param initial The initial value.
     * @param precision The number of decimal places to display.
     * @param editable Whether the value can be edited.
     */
    MantissaBox(T initial = 0, int precision = 6, bool editable = true)
        : NumberBox<T>{
              juce::NormalisableRange<T>{
                  0,
                  std::nextafter(static_cast<T>(1), static_cast<T>(0)),
                  static_cast<T>(1. / std::pow(10., precision)),
              },
              std::fmod(initial, static_cast<T>(1)),
              precision,
              editable,
              false,
              true,
          }
    {
    }

  public:
    /**
     * @brief Sets the value using the fractional part of the provided number.
     *
     * If there is an integer part of value, that is emitted with the on_overflow
     * callback.
     *
     * @param value The value to set.
     */
    auto set_value(T value) -> void override
    {
        if (!this->is_editable() || (value == this->get_value()))
        {
            return;
        }

        auto integral = T(0);
        auto mantissa = std::modf(value, &integral);

        if (value < 0)
        {
            mantissa = 1 + mantissa;
            integral = integral - 1;
        }

        if (integral != 0 && on_overflow)
        {
            this->on_overflow(static_cast<int>(integral));
        }

        this->NumberBox<T>::set_value(mantissa);

        // Wrap Around
        if (value < 0 || value >= 1)
        {
            this->last_click_proportion_ = this->proportion_;
        }

        // FIXME Not quite right, if integral of integral box is zero(not the integral
        // variable above), this shouldn't be able to decrement past zero.
    }

  public:
    std::function<void(int)> on_overflow;
};

} // namespace xen::gui