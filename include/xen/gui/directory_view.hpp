#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

namespace xen::gui
{

class DirectoryView : public juce::ListBox,
                      public juce::ListBoxModel,
                      public juce::ChangeListener,
                      private juce::Timer
{
  private:
    inline static auto const POLLING_MS = 4'000;

  public:
    sl::Signal<void(juce::File const &)> on_file_selected;
    sl::Signal<void(juce::File const &)> on_directory_change;

  public:
    DirectoryView(juce::File const &initial_directory,
                  juce::WildcardFileFilter const &file_filter);

    ~DirectoryView() override;

  public:
    void changeListenerCallback(juce::ChangeBroadcaster *source) override;

    void listBoxItemDoubleClicked(int row, juce::MouseEvent const &mouse) override;

    void returnKeyPressed(int last_row_selected) override;

    auto keyPressed(juce::KeyPress const &key) -> bool override;

    void lookAndFeelChanged() override;

  public:
    auto getNumRows() -> int override;

    void paintListBoxItem(int row_number, juce::Graphics &g, int width, int height,
                          bool row_is_selected) override;

    void timerCallback() override;

  private:
    // double clicked or enter
    void item_selected(int index);

  private:
    juce::TimeSliceThread dcl_thread_{"DirectoryViewComponentThread"};
    juce::WildcardFileFilter file_filter_;
    juce::DirectoryContentsList directory_contents_list_{&file_filter_, dcl_thread_};
};

class SequencesList : public DirectoryView
{
  public:
    explicit SequencesList(juce::File const &initial_directory);
};

class TuningsList : public DirectoryView
{
  public:
    explicit TuningsList(juce::File const &initial_directory);
};

} // namespace xen::gui