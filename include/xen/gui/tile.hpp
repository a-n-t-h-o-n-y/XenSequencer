#pragma once

#include <string>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <xen/gui/themes.hpp>
#include <xen/gui/fonts.hpp>

namespace xen::gui
{

/**
 * A square that displays a single letter.
 */
class Tile : public juce::Component
{
  public:
    /**
     * Sets the initial single cell display and margin in pixels.
     */
    explicit Tile(std::string display, int margin = 3);

  public:
    /**
     * Sets the (potentially multi-byte) letter to display.
     */
    void set(std::string display);

    /**
     * Returns the current letter.
     */
    [[nodiscard]] auto get() const -> std::string;

  public:
    void paint(juce::Graphics &g) override;

  public:
    int background_color_id = ColorID::Background;
    int text_color_id = ColorID::ForegroundMedium;
    juce::Font font = fonts::monospaced().bold;

  private:
    std::string display_;
    int margin_;
};

// -------------------------------------------------------------------------------------

class ClickableTile : public Tile
{
  public:
    /**
     * Emitted on Left Mouse Button Up.
     */
    sl::Signal<void()> clicked;

  public:
    using Tile::Tile;

  public:
    void mouseUp(juce::MouseEvent const &event) override;
};

} // namespace xen::gui