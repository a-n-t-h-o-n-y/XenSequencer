#include "plugin_editor.hpp"
#include "xen_processor.hpp"

// TODO delete this file
namespace xen
{

PluginEditor::PluginEditor(XenProcessor &p, State const &state)
    : AudioProcessorEditor{&p}, processor_ref_{p}, cache_{state}
{
    this->setResizable(true, true);
    this->setResizeLimits(400, 300, 1200, 900);

    this->setSize(1000, 300);

    this->addAndMakeVisible(&heading_);
    this->addAndMakeVisible(&phrase_editor_);
    this->addAndMakeVisible(&tuning_box_);

    this->set_state(state);

    heading_.set_justification(juce::Justification::centred);

    // TODO - or more fine grained like bpm/sample rate changed.
    // think about threads, probably doesn't need to be because this will cause
    // thread_safe_update to be called and will take care of it.
    // processor_ref_.on_state_change = []{};

    tuning_box_.on_tuning_changed = [this](auto const &tuning) {
        cache_.tuning = tuning;
        processor_ref_.thread_safe_update(cache_);
        phrase_editor_.set_tuning_length(tuning.intervals.size());
    };thread_safe_update

    phrase_editor_.on_phrase_update = [this] {
        cache_.phrase = phrase_editor_.get_phrase();
        processor_ref_.thread_safe_update(cache_);
    };
}

auto PluginEditor::set_state(State const &state) -> void
{
    cache_ = state;

    tuning_box_.set_tuning(cache_.tuning);
    phrase_editor_.set_phrase(cache_.phrase);

    // TODO set base frequency
}

void PluginEditor::resized()
{
    auto flexbox = juce::FlexBox{};
    flexbox.flexDirection = juce::FlexBox::Direction::column;

    flexbox.items.add(juce::FlexItem(heading_).withHeight(30.f));
    flexbox.items.add(juce::FlexItem(phrase_editor_).withFlex(1.f));
    flexbox.items.add(juce::FlexItem(tuning_box_).withHeight(140.f));

    flexbox.performLayout(this->getLocalBounds());
}

} // namespace xen