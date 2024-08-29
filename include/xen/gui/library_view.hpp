#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/active_sessions.hpp>
#include <xen/gui/directory_view.hpp>

namespace xen::gui
{

class LibraryView : public juce::Component
{
  public:
    class Divider : public juce::Component
    {
      public:
        void paint(juce::Graphics &g) override;
    };

  public:
    juce::Label label;
    Divider divider_0;

    juce::Label sequences_label;
    SequencesList sequences_list;
    Divider divider_1;

    juce::Label active_sessions_label;
    ActiveSessionsList active_sessions_list;
    Divider divider_2;

    juce::Label tunings_label;
    TuningsList tunings_list;

  public:
    LibraryView(juce::File const &sequence_library_dir,
                juce::File const &tuning_library_dir);

  public:
    void resized() override;

    void lookAndFeelChanged() override;
};

} // namespace xen::gui