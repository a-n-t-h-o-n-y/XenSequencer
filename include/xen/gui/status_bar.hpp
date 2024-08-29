#pragma once

#include <string>

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/message_level.hpp>

namespace xen::gui
{

/**
 * Single line message display.
 */
class StatusBar : public juce::Component
{
  public:
    StatusBar();

  public:
    void set_minimum_level(MessageLevel level);

    void set_status(MessageLevel level, std::string text);

    void clear();

  public:
    void resized() override;

    void lookAndFeelChanged() override;

    void paintOverChildren(juce::Graphics &g) override;

  private:
    juce::Label label_;
    MessageLevel minimum_level_ = MessageLevel::Info;
    MessageLevel current_level_ = MessageLevel::Info;
};

} // namespace xen::gui