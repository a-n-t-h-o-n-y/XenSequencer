#pragma once

#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <xen/gui/active_sessions_view.hpp>
#include <xen/gui/directory_list_box.hpp>
#include <xen/gui/xen_list_box.hpp>
#include <xen/scale.hpp>

namespace xen::gui
{

class ScalesList : public XenListBox
{
  public:
    sl::Signal<void(::xen::Scale const &)> on_scale_selected;

  public:
    ScalesList();

  public:
    void update(std::vector<::xen::Scale> const &scales);

  public:
    auto getNumRows() -> int override;

    auto get_row_display(std::size_t index) -> juce::String override;

    void item_selected(std::size_t index) override;

  private:
    std::vector<::xen::Scale> scales_;
};

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
    DirectoryListBox sequences_list;
    Divider divider_1;

    juce::Label active_sessions_label;
    ActiveSessionsView active_sessions_view;
    Divider divider_2;

    juce::Label tunings_label;
    DirectoryListBox tunings_list;
    Divider divider_3;

    juce::Label scales_label;
    ScalesList scales_list;

  public:
    LibraryView(juce::File const &sequence_library_dir,
                juce::File const &tuning_library_dir);

  public:
    void resized() override;

    void lookAndFeelChanged() override;
};

} // namespace xen::gui