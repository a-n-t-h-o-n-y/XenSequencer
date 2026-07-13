#pragma once

#include <cstdint>

#include <xen/midi_schedule.hpp>
#include <xen/state.hpp>

namespace xen
{

class MidiCompiler
{
  public:
    [[nodiscard]] static auto compile(ProjectState const &project,
                                      ChannelId const &channel_id,
                                      std::uint64_t generation) -> CompiledMidiSchedule;
};

} // namespace xen
