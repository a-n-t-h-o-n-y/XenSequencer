#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/message_level.hpp>

namespace xen::gui
{

class MessageLog : public juce::Viewport
{
    class Log : public juce::Component
    {
      public:
        void append(juce::String const &text, juce::Colour color);

      public:
        void paint(juce::Graphics &g) override;

        void resized() override;

      private:
        void refresh();

      private:
        juce::AttributedString content_;
        juce::TextLayout layout_;
    };

  public:
    MessageLog();

  public:
    void add_message(juce::String const &text, ::xen::MessageLevel level);

  public:
    void resized() override;

  private:
    Log log_;
};

} // namespace xen::gui