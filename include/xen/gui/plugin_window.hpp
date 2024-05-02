#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <sequence/sequence.hpp>

#include <xen/command_history.hpp>
#include <xen/gui/active_sessions.hpp>
#include <xen/gui/command_bar.hpp>
#include <xen/gui/phrase_editor.hpp>

#include <xen/gui/accordion.hpp>
#include <xen/gui/color_ids.hpp>
#include <xen/gui/directory_view.hpp>
#include <xen/gui/status_bar.hpp>
#include <xen/gui/timeline.hpp>
#include <xen/key_core.hpp>
#include <xen/xen_command_tree.hpp>

namespace xen
{
struct SequencerState;
struct AuxState;
struct Metadata;
} // namespace xen

namespace xen::gui
{

class PhrasesView : public juce::Component
{
  public:
    LabelWithLine library_label;
    PhraseDirectoryView directory_view;
    LabelWithLine active_sessions_label;
    ActiveSessions active_sessions_view;

  public:
    explicit PhrasesView(juce::File library_location)
        : library_label{"Library Label", "Library"}, directory_view{library_location},
          active_sessions_label{"Active Sessions View", "Active Sessions"}
    {
        library_label.setFont(library_label.getFont().boldened());
        active_sessions_label.setFont(active_sessions_label.getFont().boldened());
        this->addAndMakeVisible(library_label);
        this->addAndMakeVisible(directory_view);
        this->addAndMakeVisible(active_sessions_label);
        this->addAndMakeVisible(active_sessions_view);
    }

  public:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::row;

        auto flexbox_library = juce::FlexBox{};
        flexbox_library.flexDirection = juce::FlexBox::Direction::column;

        flexbox_library.items.add(juce::FlexItem{library_label}.withHeight(15.f));
        flexbox_library.items.add(juce::FlexItem{directory_view}.withFlex(1.f));
        flexbox.items.add(juce::FlexItem{flexbox_library}.withFlex(1.f));

        auto flexbox_active_sessions = juce::FlexBox{};
        flexbox_active_sessions.flexDirection = juce::FlexBox::Direction::column;

        flexbox_active_sessions.items.add(
            juce::FlexItem{active_sessions_label}.withHeight(15.f));
        flexbox_active_sessions.items.add(
            juce::FlexItem{active_sessions_view}.withFlex(1.f));
        flexbox.items.add(juce::FlexItem{flexbox_active_sessions}.withFlex(1.f));

        flexbox.performLayout(this->getLocalBounds());
    }

    auto lookAndFeelChanged() -> void override
    {
        library_label.setColour(
            juce::Label::textColourId,
            this->findColour((int)DirectoryViewColorIDs::TitleText));
        library_label.setColour(
            juce::Label::backgroundColourId,
            this->findColour((int)DirectoryViewColorIDs::TitleBackground));
        active_sessions_label.setColour(
            juce::Label::textColourId,
            this->findColour((int)ActiveSessionsColorIDs::TitleText));
        active_sessions_label.setColour(
            juce::Label::backgroundColourId,
            this->findColour((int)ActiveSessionsColorIDs::TitleBackground));
    }
};

/**
 * The main window for the plugin, holding all other components.
 *
 * This component's main purpose is as a box of other components. It is responsible for
 * updating all child components with the current state of the timeline.
 */
class PluginWindow : public juce::Component
{
  public:
    Accordion<PhrasesView> phrases_view_accordion;
    PhrasesView &phrases_view;
    gui::Timeline gui_timeline;
    gui::PhraseEditor phrase_editor;
    // gui::TuningBox tuning_box; // TODO
    gui::CommandBar command_bar;
    gui::StatusBar status_bar;

  public:
    PluginWindow(XenTimeline &tl, CommandHistory &cmd_history);

  public:
    /**
     * Update all child components with the current state of the timeline.
     *
     * @param state The current state of the timeline
     * @param aux The current aux state of the timeline
     * @param display_name The name of the current instance
     */
    auto update(SequencerState const &state, AuxState const &aux,
                std::string const &display_name) -> void;

    /**
     * Set the focus of the plugin window by ComponentID
     *
     * @param component_id The ComponentID of the component to focus
     * @throws std::invalid_argument if the ComponentID is not found
     */
    auto set_focus(std::string component_id) -> void;

    /**
     * Lookup up the component by ComponentID and update the GUI to show it.
     *
     * @param component_id The ComponentID of the component to show
     * @throws std::invalid_argument if the ComponentID is not found
     */
    auto show_component(std::string component_id) -> void;

  protected:
    auto resized() -> void override;
};

} // namespace xen::gui