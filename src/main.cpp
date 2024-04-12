#include <juce_audio_processors/juce_audio_processors.h>

#include <xen/xen_processor.hpp>

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new xen::XenProcessor{};
}