#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <print>
#include <string>
#include <thread>

#include <juce_events/juce_events.h>

#include <xen/coordinator_registry.hpp>
#include <xen/coordinator_server.hpp>

namespace
{

[[nodiscard]] auto argument_value(int argc, char **argv, std::string const &name)
    -> std::string
{
    for (auto i = 1; i + 1 < argc; ++i)
    {
        if (argv[i] == name)
        {
            return argv[i + 1];
        }
    }
    return {};
}

} // namespace

auto main(int argc, char **argv) -> int
{
    try
    {
        auto const session_id = argument_value(argc, argv, "--session-id");
        auto const registry_path = argument_value(argc, argv, "--registry");
        if (session_id.empty() || registry_path.empty())
        {
            std::println(stderr, "Usage: XenSequencerCoordinator --session-id <id> "
                                 "--registry <path>");
            return 2;
        }

        auto scoped_juce = juce::ScopedJuceInitialiser_GUI{};
        auto server = xen::ipc::CoordinatorServer{session_id, registry_path};
        auto const entry = server.start();
        std::println("XenSequencerCoordinator listening on port {}", entry.port);

        auto constexpr idle_grace_ms = int64_t{5000};
        for (;;)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
            server.perform_maintenance(
                static_cast<std::uint64_t>(juce::Time::currentTimeMillis()));
            if (server.shutdown_requested() ||
                (server.client_count() == 0 && server.idle_for_ms() > idle_grace_ms))
            {
                server.perform_maintenance(
                    static_cast<std::uint64_t>(juce::Time::currentTimeMillis()), true);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
        }
        return 0;
    }
    catch (std::exception const &e)
    {
        std::println(stderr, "XenSequencerCoordinator error: {}", e.what());
        return 1;
    }
}
