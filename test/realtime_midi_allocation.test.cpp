#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <juce_audio_basics/juce_audio_basics.h>

#include <sequence/sequence.hpp>

#include <xen/midi_compiler.hpp>
#include <xen/realtime_midi_player.hpp>

namespace
{

thread_local bool tracking = false;
std::atomic<std::uint64_t> allocations{};
std::atomic<std::uint64_t> frees{};

} // namespace

extern "C" void *__real_malloc(std::size_t);
extern "C" void *__real_calloc(std::size_t, std::size_t);
extern "C" void *__real_realloc(void *, std::size_t);
extern "C" void __real_free(void *);

extern "C" void *__wrap_malloc(std::size_t size)
{
    allocations.fetch_add(tracking ? 1U : 0U, std::memory_order_relaxed);
    return __real_malloc(size);
}

extern "C" void *__wrap_calloc(std::size_t count, std::size_t size)
{
    allocations.fetch_add(tracking ? 1U : 0U, std::memory_order_relaxed);
    return __real_calloc(count, size);
}

extern "C" void *__wrap_realloc(void *pointer, std::size_t size)
{
    allocations.fetch_add(tracking ? 1U : 0U, std::memory_order_relaxed);
    return __real_realloc(pointer, size);
}

extern "C" void __wrap_free(void *pointer)
{
    frees.fetch_add(tracking ? 1U : 0U, std::memory_order_relaxed);
    __real_free(pointer);
}

auto main() -> int
{
    auto project = xen::ProjectState{};
    xen::selected_sequence(project, xen::CompositionCursor{}) = {
        .elements = {sequence::Note{.pitch = 0}}, .weight = 1.0F};
    auto update = xen::CompiledMidiUpdate{.generation = 1};
    update.schedule = xen::MidiCompiler::compile(project, xen::DEFAULT_CHANNEL_ID, 1);
    auto changed_project = project;
    changed_project.composition.columns.at(0).pitch.base_frequency = 441.0F;
    auto changed_update = xen::CompiledMidiUpdate{.generation = 2};
    changed_update.schedule =
        xen::MidiCompiler::compile(changed_project, xen::DEFAULT_CHANNEL_ID, 2);

    auto player = xen::RealtimeMidiPlayer{};
    player.prepare(44'100, 512);
    player.adopt(update);
    auto buffer = juce::MidiBuffer{};
    auto transport = xen::TransportBlock{
        .playing = true,
        .has_ppq = true,
        .ppq = 0.0,
        .bpm = 120.0,
        .sample_rate = 44'100,
        .sample_count = 512,
    };
    auto const block_beats = 512.0 * 120.0 / (60.0 * 44'100.0);

    for (auto block = 0; block < 3; ++block)
    {
        buffer.clear();
        player.process(transport, buffer);
        transport.ppq += block_beats;
    }

    for (auto block = 0; block < 10'000; ++block)
    {
        buffer.clear();
        auto const controller = std::array<std::uint8_t, 3>{0xb0U, 7U, 100U};
        buffer.addEvent(controller.data(), static_cast<int>(controller.size()), 3);
        tracking = true;
        if (block == 100)
        {
            player.adopt(changed_update);
        }
        if (block == 200)
        {
            transport.ppq = 1.5;
        }
        player.process(transport, buffer);
        tracking = false;
        transport.ppq += block_beats;
    }

    auto const allocation_count = allocations.load(std::memory_order_relaxed);
    auto const free_count = frees.load(std::memory_order_relaxed);
    if (allocation_count != 0 || free_count != 0)
    {
        std::fprintf(stderr, "RealtimeMidiPlayer allocated %llu and freed %llu times\n",
                     static_cast<unsigned long long>(allocation_count),
                     static_cast<unsigned long long>(free_count));
        return 1;
    }
    return 0;
}
