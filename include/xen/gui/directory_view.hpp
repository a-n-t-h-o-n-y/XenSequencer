#pragma once

#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

namespace xen::gui
{

class PhraseDirectoryView : public juce::Component,
                            public juce::ListBoxModel,
                            public juce::ChangeListener,
                            private juce::Timer
{
  private:
    inline static constexpr auto POLLING_MS = 2'000;

  public:
    sl::Signal<void(juce::File const &)> on_file_selected;
    sl::Signal<void(juce::File const &)> on_directory_change;

  public:
    explicit PhraseDirectoryView(juce::File const &initial_directory);

    ~PhraseDirectoryView() override;

  public:
    auto resized() -> void override;

    auto changeListenerCallback(juce::ChangeBroadcaster *source) -> void override;

    auto listBoxItemDoubleClicked(int row, juce::MouseEvent const &mouse)
        -> void override;

    auto returnKeyPressed(int lastRowSelected) -> void override;

    auto keyPressed(juce::KeyPress const &key) -> bool override;

    auto lookAndFeelChanged() -> void override;

  private:
    auto getNumRows() -> int override;

    auto paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                          bool rowIsSelected) -> void override;

    auto timerCallback() -> void override;

  private:
    // double clicked or enter
    auto item_selected(int index) -> void;

  private:
    juce::TimeSliceThread dcl_thread_{"DirectoryViewComponentThread"};
    juce::WildcardFileFilter file_filter_{"*.json", "*", "JSON filter"};
    juce::DirectoryContentsList directory_contents_list_{&file_filter_, dcl_thread_};
    juce::ListBox list_box_;
};

} // namespace xen::gui