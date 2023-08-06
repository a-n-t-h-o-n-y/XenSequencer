#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../state.hpp"
// #include "gui/heading.hpp"
// #include "gui/phrase_editor.hpp"
// #include "gui/tuning.hpp"

namespace xen::gui
{

class PluginWindow : public juce::Component
{
  public:
    PluginWindow()
    {
        // TODO
        // this->addAndMakeVisible(&heading_);
        // this->addAndMakeVisible(&phrase_editor_);
        // this->addAndMakeVisible(&tuning_box_);

        // heading_.set_justification(juce::Justification::centred);

        // tuning_box_.on_tuning_changed = [this](auto const &tuning) {
        //     cache_.tuning = tuning;
        //     processor_ref_.thread_safe_update(cache_);
        //     phrase_editor_.set_tuning_length(tuning.intervals.size());
        // };

        // phrase_editor_.on_phrase_update = [this] {
        //     cache_.phrase = phrase_editor_.get_phrase();
        //     processor_ref_.thread_safe_update(cache_);
        // };
    }

  public:
    auto update(State const &) -> void
    {
        // TODO
        // tuning_box_.set_tuning(cache_.tuning);
        // phrase_editor_.set_phrase(cache_.phrase);
    }

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::column;

        // flexbox.items.add(juce::FlexItem(heading_).withHeight(30.f));
        // flexbox.items.add(juce::FlexItem(phrase_editor_).withFlex(1.f));
        // flexbox.items.add(juce::FlexItem(tuning_box_).withHeight(140.f));

        flexbox.performLayout(this->getLocalBounds());
    }

  private:
    // TODO - child components
    // gui::Heading heading_{"XenSequencer"};
    // gui::PhraseEditor phrase_editor_;
    // gui::TuningBox tuning_box_;
};

} // namespace xen