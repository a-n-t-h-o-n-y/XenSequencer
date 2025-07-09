#pragma once

#include <map>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <xen/gui/directory_list_box.hpp>
#include <xen/gui/fonts.hpp>
#include <xen/gui/themes.hpp>
#include <xen/gui/xen_list_box.hpp>
#include <xen/scale.hpp>

namespace xen::gui
{

class TuningsList : public DirectoryListBox
{
  public:
    explicit TuningsList(juce::File const &tunings_dir);

  public:
    auto getTooltipForRow(int row) -> juce::String override;

  private:
    // {filename 32bit hash, tooltip string}
    std::map<juce::int32, juce::String> tooltip_cache_;
};

// -------------------------------------------------------------------------------------

class ScalesList : public XenListBox
{
  public:
    // Emits scale name
    sl::Signal<void(std::string const &)> on_scale_selected;

  public:
    ScalesList();

  public:
    void update(std::vector<::xen::Scale> const &scales);

  public:
    auto getNumRows() -> int override;

    auto get_row_display(std::size_t index) -> juce::String override;

    void item_selected(std::size_t index) override;

    auto getTooltipForRow(int row) -> juce::String override;

  private:
    std::vector<::xen::Scale> scales_;
};

// -------------------------------------------------------------------------------------

template <typename ComponentType>
class LabeledLibraryComponent : public juce::Component
{
  public:
    juce::Label label;
    ComponentType component;

  public:
    template <typename... Args>
    explicit LabeledLibraryComponent(juce::String label_text, Args &&...args)
        : component{std::forward<Args>(args)...}
    {
        this->addAndMakeVisible(label);
        this->addAndMakeVisible(component);
        label.setText(std::move(label_text), juce::dontSendNotification);
        label.setFont(fonts::monospaced().bold.withHeight(18.f));
    }

  public:
    void resized() override
    {
        auto fb = juce::FlexBox{};
        fb.flexDirection = juce::FlexBox::Direction::column;
        fb.items.add(juce::FlexItem{label}.withHeight(20.f));
        fb.items.add(juce::FlexItem{component}.withFlex(1.f));
        fb.performLayout(this->getLocalBounds());
    }

    void lookAndFeelChanged() override
    {
        label.setColour(juce::Label::backgroundColourId,
                        this->findColour(ColorID::BackgroundLow));
    }
};

// -------------------------------------------------------------------------------------

class LibraryView : public juce::Component
{
  public:
    class Divider : public juce::Component
    {
      public:
        void paint(juce::Graphics &g) override;
    };

  public:
    LabeledLibraryComponent<DirectoryListBox> sequences;
    DirectoryListBox &sequences_list = sequences.component;
    Divider divider_1;

    LabeledLibraryComponent<TuningsList> tunings;
    TuningsList &tunings_list = tunings.component;
    Divider divider_2;

    LabeledLibraryComponent<ScalesList> scales;
    ScalesList &scales_list = scales.component;

  public:
    LibraryView(juce::File const &sequence_library_dir,
                juce::File const &tuning_library_dir);

  public:
    void resized() override;
};

} // namespace xen::gui