#include "plugin_editor.hpp"
#include "xen_processor.hpp"

#include <iostream> //temp

namespace xen
{

PluginEditor::PluginEditor(XenProcessor &p) : AudioProcessorEditor{&p}, processorRef{p}
{
    this->setResizable(true, true);
    this->setResizeLimits(400, 300, 1200, 900);

    this->setSize(1000, 300);

    this->addAndMakeVisible(&heading_);
    this->addAndMakeVisible(&phrase_editor_);
    this->addAndMakeVisible(&tuning_box_);

    heading_.set_justification(juce::Justification::centred);

    tuning_box_.on_tuning_changed = [](auto const &tuning) {
        (void)tuning;
        // std::cout << "Tuning changed:\n";
        // std::cout << std::fixed << std::setprecision(6);
        // for (auto const &interval : tuning.intervals)
        // {
        //     std::cout << interval << '\n';
        // }
        // std::cout << "Octave: " << tuning.octave << std::endl;
    };
}

PluginEditor::~PluginEditor()
{
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