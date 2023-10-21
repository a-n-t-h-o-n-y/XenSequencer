#include <catch2/catch_test_macros.hpp>

#include <juce_audio_basics/juce_audio_basics.h>

#include <xen/midi.hpp>

TEST_CASE("find_most_recent_note_event", "[midi]")
{
    auto buffer = juce::MidiBuffer{};
    buffer.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    buffer.addEvent(juce::MidiMessage::pitchWheel(1, 0), 0);
    buffer.addEvent(juce::MidiMessage::noteOff(1, 60), 20);

    buffer.addEvent(juce::MidiMessage::noteOn(1, 61, 1.0f), 100);
    buffer.addEvent(juce::MidiMessage::pitchWheel(1, 0), 100);
    buffer.addEvent(juce::MidiMessage::noteOff(1, 61), 120);

    buffer.addEvent(juce::MidiMessage::noteOn(1, 62, 1.0f), 200);
    buffer.addEvent(juce::MidiMessage::pitchWheel(1, 0), 200);
    buffer.addEvent(juce::MidiMessage::noteOff(1, 62), 220);

    {
        auto const x = xen::find_most_recent_note_event(buffer, 0);
        CHECK(x.has_value());
        CHECK(x->getNoteNumber() == 60);
        CHECK(x->isNoteOn());
    }
    {
        auto const x = xen::find_most_recent_note_event(buffer, 1);
        CHECK(x.has_value());
        CHECK(x->getNoteNumber() == 60);
        CHECK(x->isNoteOn());
    }
    {
        auto const x = xen::find_most_recent_note_event(buffer, 20);
        CHECK(x.has_value());
        CHECK(x->getNoteNumber() == 60);
        CHECK(x->isNoteOff());
    }
    {
        auto const x = xen::find_most_recent_note_event(buffer, 50);
        CHECK(x.has_value());
        CHECK(x->getNoteNumber() == 60);
        CHECK(x->isNoteOff());
    }
    {
        auto const x = xen::find_most_recent_note_event(buffer, 99);
        CHECK(x.has_value());
        CHECK(x->getNoteNumber() == 60);
        CHECK(x->isNoteOff());
    }
    {
        auto const x = xen::find_most_recent_note_event(buffer, 100);
        CHECK(x.has_value());
        CHECK(x->getNoteNumber() == 61);
        CHECK(x->isNoteOn());
    }
    {
        auto const x = xen::find_most_recent_note_event(buffer, 105);
        CHECK(x.has_value());
        CHECK(x->getNoteNumber() == 61);
        CHECK(x->isNoteOn());
    }
    {
        auto const x = xen::find_most_recent_note_event(buffer, 150);
        CHECK(x.has_value());
        CHECK(x->getNoteNumber() == 61);
        CHECK(x->isNoteOff());
    }
    {
        auto const x = xen::find_most_recent_note_event(buffer, 1000);
        CHECK(x.has_value());
        CHECK(x->getNoteNumber() == 62);
        CHECK(x->isNoteOff());
    }
}