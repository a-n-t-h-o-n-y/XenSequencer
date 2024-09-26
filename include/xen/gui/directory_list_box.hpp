#pragma once

#include <cstddef>
#include <optional>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <xen/gui/xen_list_box.hpp>

namespace xen::gui
{

class DirectoryListBox : public XenListBox,
                         public juce::ChangeListener,
                         public juce::Timer
{
  private:
    inline static auto const POLLING_MS = 4'000;

  public:
    sl::Signal<void(juce::File const &)> on_file_selected;
    sl::Signal<void(juce::File const &)> on_directory_change;

  public:
    DirectoryListBox(juce::File const &initial_directory,
                     juce::WildcardFileFilter const &file_filter,
                     juce::String const &componenet_id = "");

    ~DirectoryListBox() override;

  public:
    auto getNumRows() -> int override;

    void changeListenerCallback(juce::ChangeBroadcaster *source) override;

    void timerCallback() override;

    void visibilityChanged() override;

    auto get_row_display(std::size_t index) -> juce::String override;

    void item_selected(std::size_t index) override;

  protected:
    /**
     * Retrieve a file if it exists. Index of 0 will return nullopt, that is parent dir.
     * @details Returns nullopt for directories.
     */
    [[nodiscard]] auto get_file(std::size_t index) -> std::optional<juce::File>;

  private:
    juce::TimeSliceThread dcl_thread_{"DirectoryListBoxThread"};
    juce::WildcardFileFilter file_filter_;
    juce::DirectoryContentsList directory_contents_list_{&file_filter_, dcl_thread_};
};

} // namespace xen::gui