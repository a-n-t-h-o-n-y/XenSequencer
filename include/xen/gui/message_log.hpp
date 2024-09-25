#pragma once

#include <memory>
#include <utility>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/themes.hpp>
#include <xen/message_level.hpp>

namespace xen::gui
{

class MessageLog : public juce::Viewport
{
  public:
    MessageLog();

  public:
    void resized() override;

  private:
    class Log : public juce::Component
    {
      private:
        class Entry : public juce::Label
        {
          public:
            Entry(juce::String const &message, ::xen::MessageLevel level);

          public:
            void lookAndFeelChanged() override;

          private:
            ::xen::MessageLevel level_;
        };

      public:
        void add_message(juce::String const &message, ::xen::MessageLevel level);

      protected:
        void resized() override;

      private:
        void scroll_to_bottom();

      private:
        std::vector<std::pair<juce::String, ::xen::MessageLevel>> messages_;
        std::vector<std::unique_ptr<Entry>> entries_;
    };

  public:
    Log log;
};

} // namespace xen::gui