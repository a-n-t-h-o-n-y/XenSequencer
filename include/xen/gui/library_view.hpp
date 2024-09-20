#pragma once

#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <xen/gui/active_sessions.hpp>
#include <xen/gui/directory_view.hpp>
#include <xen/scale.hpp>

namespace xen::gui
{

class ScalesList : public juce::ListBox, public juce::ListBoxModel
{
  public:
    sl::Signal<void(::xen::Scale const &)> on_scale_selected;

  public:
    ScalesList();

    ~ScalesList() override = default;

  public:
    void update(std::vector<::xen::Scale> const &scales);

  public:
    void listBoxItemDoubleClicked(int row, juce::MouseEvent const &mouse) override;

    void returnKeyPressed(int last_row_selected) override;

    auto keyPressed(juce::KeyPress const &key) -> bool override;

    void lookAndFeelChanged() override;

  public:
    auto getNumRows() -> int override;

    void paintListBoxItem(int row_number, juce::Graphics &g, int width, int height,
                          bool row_is_selected) override;

  private:
    // double clicked or enter
    void item_selected(int index);

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
    SequencesList sequences_list;
    Divider divider_1;

    juce::Label active_sessions_label;
    ActiveSessionsList active_sessions_list;
    Divider divider_2;

    juce::Label tunings_label;
    TuningsList tunings_list;
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