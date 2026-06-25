#include <chrono>
#include <exception>
#include <iostream>
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
            std::cerr << "Usage: XenSequencerCoordinator --session-id <id> "
                         "--registry <path>\n";
            return 2;
        }

        auto scoped_juce = juce::ScopedJuceInitialiser_GUI{};
        auto server = xen::ipc::CoordinatorServer{session_id, registry_path};
        auto const entry = server.start();
        std::cout << "XenSequencerCoordinator listening on port " << entry.port << "\n";

        auto constexpr idle_grace_ms = int64_t{5000};
        for (;;)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
            if (server.shutdown_requested() ||
                (server.client_count() == 0 && server.idle_for_ms() > idle_grace_ms))
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
        }
        return 0;
    }
    catch (std::exception const &e)
    {
        std::cerr << "XenSequencerCoordinator error: " << e.what() << "\n";
        return 1;
    }
}
