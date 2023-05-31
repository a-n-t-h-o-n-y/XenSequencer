#include "plugin_editor.hpp"
#include "xen_processor.hpp"

#include <iostream> //temp

namespace xen
{

PluginEditor::PluginEditor(XenProcessor &p) : AudioProcessorEditor{&p}, processorRef{p}
{
    this->setResizable(true, true);
    this->setResizeLimits(400, 300, 1200, 900);

    this->setSize(400, 300);
    bpm_box_.set_value(150.f);
    this->addAndMakeVisible(&bpm_box_);
    this->addAndMakeVisible(&tuning_box_);

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

void PluginEditor::paint(juce::Graphics &g)
{
    // fill the whole window white
    g.fillAll(juce::Colours::white);

    // set the current drawing colour to black
    g.setColour(juce::Colours::black);

    // set the font size and draw text to the screen
    g.setFont(15.0f);

    g.drawFittedText("XenSequencer", 0, 0, getWidth(), 30, juce::Justification::centred,
                     1);
}

void PluginEditor::resized()
{
    auto flexbox = juce::FlexBox{};
    flexbox.flexDirection = juce::FlexBox::Direction::column;
    flexbox.alignContent = juce::FlexBox::AlignContent::center;

    flexbox.items.add(juce::FlexItem(bpm_box_).withFlex(1.f));
    flexbox.items.add(juce::FlexItem(tuning_box_).withHeight(140.f));

    flexbox.performLayout(this->getLocalBounds());
}

} // namespace xen