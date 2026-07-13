#include <xen/midi_compilation_service.hpp>

#include <exception>
#include <new>
#include <stdexcept>
#include <utility>

#include <juce_core/juce_core.h>

#include <xen/midi_compiler.hpp>

namespace xen
{

MidiCompilationService::MidiCompilationService()
    : worker_{[this](std::stop_token stop) { run(stop); }}
{
}

MidiCompilationService::~MidiCompilationService()
{
    {
        auto const lock = std::scoped_lock{mutex_};
        worker_.request_stop();
    }
    condition_.notify_all();
    if (worker_.joinable())
    {
        worker_.join();
    }
}

auto MidiCompilationService::submit(ProjectState project, ChannelId channel_id)
    -> std::uint64_t
{
    auto lock = std::scoped_lock{mutex_};
    auto const generation = ++requested_generation_;
    pending_ = Request{
        .generation = generation,
        .project = std::move(project),
        .channel_id = std::move(channel_id),
    };
    status_.requested_generation = generation;
    condition_.notify_one();
    return generation;
}

auto MidiCompilationService::published_generation() const noexcept -> std::uint64_t
{
    return published_.generation();
}

auto MidiCompilationService::try_consume_latest() noexcept
    -> std::optional<CompiledMidiMailbox::ReadView>
{
    return published_.try_consume_latest();
}

auto MidiCompilationService::status() const -> MidiCompilationStatus
{
    auto const lock = std::scoped_lock{mutex_};
    return status_;
}

void MidiCompilationService::set_status(std::uint64_t generation,
                                        MidiCompilationError error, std::string message)
{
    status_.published_generation = generation;
    status_.error = error;
    status_.message = std::move(message);
}

void MidiCompilationService::run(std::stop_token stop)
{
    while (!stop.stop_requested())
    {
        auto request = std::optional<Request>{};
        {
            auto lock = std::unique_lock{mutex_};
            condition_.wait(
                lock, [&] { return stop.stop_requested() || pending_.has_value(); });
            if (stop.stop_requested())
            {
                return;
            }
            request = std::move(pending_);
            pending_.reset();
        }

        auto update = CompiledMidiUpdate{.generation = request->generation};
        auto message = std::string{};
        try
        {
            update.schedule = MidiCompiler::compile(
                request->project, request->channel_id, request->generation);
        }
        catch (std::bad_alloc const &error)
        {
            update.error = MidiCompilationError::OutOfMemory;
            message = error.what();
        }
        catch (std::overflow_error const &error)
        {
            update.error = MidiCompilationError::NumericOverflow;
            message = error.what();
        }
        catch (std::invalid_argument const &error)
        {
            update.error = MidiCompilationError::InvalidProject;
            message = error.what();
        }
        catch (std::exception const &error)
        {
            update.error = MidiCompilationError::Internal;
            message = error.what();
        }
        catch (...)
        {
            update.error = MidiCompilationError::Internal;
            message = "Unknown MIDI compilation failure.";
        }

        {
            auto lock = std::scoped_lock{mutex_};
            if (request->generation != requested_generation_)
            {
                continue;
            }
            if (update.error != MidiCompilationError::None)
            {
                juce::Logger::writeToLog("XenSequencer MIDI compilation failed: " +
                                         juce::String{message});
            }
            set_status(request->generation, update.error, message);
            published_.publish(std::move(update));
        }
    }
}

} // namespace xen
