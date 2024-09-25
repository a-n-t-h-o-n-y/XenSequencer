#include <xen/gui/library_view.hpp>

#include <cassert>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/fonts.hpp>
#include <xen/gui/themes.hpp>
#include <xen/scale.hpp>

namespace xen::gui
{

ScalesList::ScalesList() : XenListBox{"ScalesList"}
{
}

void ScalesList::update(std::vector<::xen::Scale> const &scales)
{
    scales_ = scales;
    this->updateContent();
}

auto ScalesList::getNumRows() -> int
{
    return (int)scales_.size();
}

auto ScalesList::get_row_display(std::size_t index) -> juce::String
{
    return scales_[index].name;
}

void ScalesList::item_selected(std::size_t index)
{
    assert(index < scales_.size());
    this->on_scale_selected(scales_[index]);
}

// -------------------------------------------------------------------------------------

void LibraryView::Divider::paint(juce::Graphics &g)
{
    g.setColour(this->findColour(ColorID::ForegroundLow));
    g.fillAll();
}

// -------------------------------------------------------------------------------------

LibraryView::LibraryView(juce::File const &sequence_library_dir,
                         juce::File const &tuning_library_dir)
    : sequences_list{sequence_library_dir,
                     juce::WildcardFileFilter{"*.xenseq", "*", "XenSeq filter"},
                     "SequencesList"},
      tunings_list{tuning_library_dir,
                   juce::WildcardFileFilter{"*.scl", "*", "scala filter"},
                   "TuningsList"}
{
    this->setComponentID("LibraryView");

    label.setFont(fonts::monospaced().bold.withHeight(20.f));
    label.setText("Library", juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    this->addAndMakeVisible(label);
    this->addAndMakeVisible(divider_0);

    sequences_label.setText("Sequences", juce::dontSendNotification);
    sequences_label.setFont(fonts::monospaced().bold.withHeight(18.f));
    this->addAndMakeVisible(sequences_label);
    this->addAndMakeVisible(sequences_list);
    this->addAndMakeVisible(divider_1);

    active_sessions_label.setText("Active Sessions", juce::dontSendNotification);
    active_sessions_label.setFont(fonts::monospaced().bold.withHeight(18.f));
    this->addAndMakeVisible(active_sessions_label);
    this->addAndMakeVisible(active_sessions_view);
    this->addAndMakeVisible(divider_2);

    tunings_label.setText("Tunings", juce::dontSendNotification);
    tunings_label.setFont(fonts::monospaced().bold.withHeight(18.f));
    this->addAndMakeVisible(tunings_label);
    this->addAndMakeVisible(tunings_list);
    this->addAndMakeVisible(divider_3);

    scales_label.setText("Scales", juce::dontSendNotification);
    scales_label.setFont(fonts::monospaced().bold.withHeight(18.f));
    this->addAndMakeVisible(scales_label);
    this->addAndMakeVisible(scales_list);
}

void LibraryView::resized()
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
        juce::FlexItem{active_sessions_view}.withFlex(1.f));
    lists_flexbox.items.add(juce::FlexItem{active_sessions_flexbox}.withFlex(1.f));
    lists_flexbox.items.add(juce::FlexItem{divider_2}.withWidth(1.f));

    auto tunings_flexbox = juce::FlexBox{};
    tunings_flexbox.flexDirection = juce::FlexBox::Direction::column;
    tunings_flexbox.items.add(juce::FlexItem{tunings_label}.withHeight(20.f));
    tunings_flexbox.items.add(juce::FlexItem{tunings_list}.withFlex(1.f));
    lists_flexbox.items.add(juce::FlexItem{tunings_flexbox}.withFlex(1.f));
    lists_flexbox.items.add(juce::FlexItem{divider_3}.withWidth(1.f));

    auto scales_flexbox = juce::FlexBox{};
    scales_flexbox.flexDirection = juce::FlexBox::Direction::column;
    scales_flexbox.items.add(juce::FlexItem{scales_label}.withHeight(20.f));
    scales_flexbox.items.add(juce::FlexItem{scales_list}.withFlex(1.f));
    lists_flexbox.items.add(juce::FlexItem{scales_flexbox}.withFlex(1.f));

    outer_flexbox.performLayout(this->getLocalBounds());
}

void LibraryView::lookAndFeelChanged()
{
    label.setColour(juce::Label::backgroundColourId,
                    this->findColour(ColorID::Background));
    label.setColour(juce::Label::textColourId,
                    this->findColour(ColorID::ForegroundHigh));
    sequences_label.setColour(juce::Label::backgroundColourId,
                              this->findColour(ColorID::BackgroundMedium));
    active_sessions_label.setColour(juce::Label::backgroundColourId,
                                    this->findColour(ColorID::BackgroundMedium));
    tunings_label.setColour(juce::Label::backgroundColourId,
                            this->findColour(ColorID::BackgroundMedium));
    scales_label.setColour(juce::Label::backgroundColourId,
                           this->findColour(ColorID::BackgroundMedium));
}

} // namespace xen::gui