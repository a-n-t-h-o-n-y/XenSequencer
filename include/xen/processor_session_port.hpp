#pragma once

#include <cstdint>
#include <optional>

#include <xen/compiled_midi_mailbox.hpp>
#include <xen/sequencer_session_port.hpp>

namespace xen
{

class ProcessorSessionPort : public SequencerSessionPort
{
  public:
    [[nodiscard]] virtual auto compiled_midi_generation() const noexcept
        -> std::uint64_t = 0;
    [[nodiscard]] virtual auto try_consume_compiled_midi() noexcept
        -> std::optional<CompiledMidiMailbox::ReadView> = 0;
    [[nodiscard]] virtual auto midi_compilation_status() const
        -> MidiCompilationStatus = 0;
};

} // namespace xen
