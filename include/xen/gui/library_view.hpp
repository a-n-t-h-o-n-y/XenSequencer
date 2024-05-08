#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/active_sessions.hpp>
#include <xen/gui/color_ids.hpp>
#include <xen/gui/directory_view.hpp>

namespace xen::gui
{

class Divider : public juce::Component
{
  public:
    auto paint(juce::Graphics &g) -> void override
    {
        g.setColour(this->findColour((int)TimeSignatureColorIDs::Outline));
        g.drawLine(0, 0, this->getWidth(), this->getHeight());
    }
};

class LibraryView : public juce::Component
{
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
                juce::File const &tuning_library_dir)
        : sequences_list{sequence_library_dir}, tunings_list{tuning_library_dir}
    {
        this->setComponentID("LibraryView");

        label.setText("Library", juce::dontSendNotification);
        label.setFont({
            juce::Font::getDefaultMonospacedFontName(),
            16.f,
            juce::Font::bold,
        });
        label.setJustificationType(juce::Justification::centred);
        this->addAndMakeVisible(label);
        this->addAndMakeVisible(divider_0);

        sequences_label.setText("Sequences", juce::dontSendNotification);
        sequences_label.setFont({
            juce::Font::getDefaultMonospacedFontName(),
            14.f,
            juce::Font::plain,
        });
        this->addAndMakeVisible(sequences_label);
        this->addAndMakeVisible(sequences_list);
        this->addAndMakeVisible(divider_1);

        active_sessions_label.setText("Active Sessions", juce::dontSendNotification);
        active_sessions_label.setFont({
            juce::Font::getDefaultMonospacedFontName(),
            14.f,
            juce::Font::plain,
        });
        this->addAndMakeVisible(active_sessions_label);
        this->addAndMakeVisible(active_sessions_list);
        this->addAndMakeVisible(divider_2);

        tunings_label.setText("Tunings", juce::dontSendNotification);
        tunings_label.setFont({
            juce::Font::getDefaultMonospacedFontName(),
            14.f,
            juce::Font::plain,
        });
        this->addAndMakeVisible(tunings_label);
        this->addAndMakeVisible(tunings_list);

        this->lookAndFeelChanged();
    }

  public:
    auto resized() -> void override
    {
        auto outer_flexbox = juce::FlexBox{};
        outer_flexbox.flexDirection = juce::FlexBox::Direction::column;

        auto lists_flexbox = juce::FlexBox{};
        lists_flexbox.flexDirection = juce::FlexBox::Direction::row;

        outer_flexbox.items.add(juce::FlexItem{label}.withHeight(23.0f));
        outer_flexbox.items.add(juce::FlexItem{divider_0}.withHeight(1.f));
        outer_flexbox.items.add(juce::FlexItem{lists_flexbox}.withFlex(1.0f));

        auto sequences_flexbox = juce::FlexBox{};
        sequences_flexbox.flexDirection = juce::FlexBox::Direction::column;
        sequences_flexbox.items.add(juce::FlexItem{sequences_label}.withHeight(20.f));
        sequences_flexbox.items.add(juce::FlexItem{sequences_list}.withFlex(1.f));
        lists_flexbox.items.add(juce::FlexItem{sequences_flexbox}.withFlex(1.f));
        lists_flexbox.items.add(juce::FlexItem{divider_1}.withWidth(1.f));

        auto active_sessions_flexbox = juce::FlexBox{};
        active_sessions_flexbox.flexDirection = juce::FlexBox::Direction::column;
        active_sessions_flexbox.items.add(
            juce::FlexItem{active_sessions_label}.withHeight(20.f));
        active_sessions_flexbox.items.add(
            juce::FlexItem{active_sessions_list}.withFlex(1.f));
        lists_flexbox.items.add(juce::FlexItem{active_sessions_flexbox}.withFlex(1.f));
        lists_flexbox.items.add(juce::FlexItem{divider_2}.withWidth(1.f));

        auto tunings_flexbox = juce::FlexBox{};
        tunings_flexbox.flexDirection = juce::FlexBox::Direction::column;
        tunings_flexbox.items.add(juce::FlexItem{tunings_label}.withHeight(20.f));
        tunings_flexbox.items.add(juce::FlexItem{tunings_list}.withFlex(1.f));
        lists_flexbox.items.add(juce::FlexItem{tunings_flexbox}.withFlex(1.f));

        outer_flexbox.performLayout(this->getLocalBounds());
    }

    auto lookAndFeelChanged() -> void override
    {
        label.setColour(juce::Label::backgroundColourId,
                        this->findColour((int)TimeSignatureColorIDs::Background));
        label.setColour(juce::Label::textColourId,
                        this->findColour((int)TimeSignatureColorIDs::Text));
        sequences_label.setColour(
            juce::Label::backgroundColourId,
            this->findColour((int)TimeSignatureColorIDs::Outline));
        active_sessions_label.setColour(
            juce::Label::backgroundColourId,
            this->findColour((int)TimeSignatureColorIDs::Outline));
        tunings_label.setColour(juce::Label::backgroundColourId,
                                this->findColour((int)TimeSignatureColorIDs::Outline));
    }
};

} // namespace xen::gui