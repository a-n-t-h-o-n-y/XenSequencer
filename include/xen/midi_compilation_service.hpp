#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <xen/compiled_midi_mailbox.hpp>
#include <xen/state.hpp>

namespace xen
{

class MidiCompilationService
{
  public:
    MidiCompilationService();
    ~MidiCompilationService();

    MidiCompilationService(MidiCompilationService const &) = delete;
    auto operator=(MidiCompilationService const &) -> MidiCompilationService & = delete;

    auto submit(ProjectState project, ChannelId channel_id) -> std::uint64_t;
    [[nodiscard]] auto published_generation() const noexcept -> std::uint64_t;
    [[nodiscard]] auto try_consume_latest() noexcept
        -> std::optional<CompiledMidiMailbox::ReadView>;
    [[nodiscard]] auto status() const -> MidiCompilationStatus;

  private:
    struct Request
    {
        std::uint64_t generation{};
        ProjectState project{};
        ChannelId channel_id{};
    };

    void run(std::stop_token stop);
    void set_status(std::uint64_t generation, MidiCompilationError error,
                    std::string message);

    mutable std::mutex mutex_{};
    std::condition_variable condition_{};
    std::optional<Request> pending_{};
    std::uint64_t requested_generation_{};
    MidiCompilationStatus status_{};
    CompiledMidiMailbox published_{};
    std::jthread worker_{};
};

} // namespace xen
