#pragma once

#include <array>
#include <cstddef>

#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

namespace xen::gui
{

class SequenceSquare : public juce::TextButton
{
  public:
    void indicate();

    void unindicate();

  public:
    void lookAndFeelChanged() override;

  private:
    [[nodiscard]] auto get_color() const -> juce::Colour;

  private:
    bool is_active_ = false;
};

class SequenceBankGrid : public juce::Component
{
  public:
    sl::Signal<void(std::size_t)> on_index_selected;

  public:
    SequenceBankGrid();

  public:
    void update(std::size_t selected_index);

  public:
    void resized() override;

    void paint(juce::Graphics &g) override;

  private:
    std::array<SequenceSquare, 16> buttons_;
};

} // namespace xen::gui