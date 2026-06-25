#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include <juce_events/juce_events.h>

#include <xen/coordinator_registry.hpp>
#include <xen/session_coordinator.hpp>

namespace xen::ipc
{

class CoordinatorConnection;

class CoordinatorServer final : public juce::InterprocessConnectionServer
{
  public:
    explicit CoordinatorServer(SessionId session_id,
                               std::filesystem::path registry_file);
    ~CoordinatorServer() override;

    [[nodiscard]] auto start() -> CoordinatorRegistryEntry;
    [[nodiscard]] auto client_count() const noexcept -> int;
    [[nodiscard]] auto idle_for_ms() const noexcept -> int64_t;
    [[nodiscard]] auto shutdown_requested() const noexcept -> bool;

    void broadcast(nlohmann::json const &message);
    void connection_closed(CoordinatorConnection &connection);
    [[nodiscard]] auto coordinator() noexcept -> SessionCoordinator &;
    [[nodiscard]] auto request_shutdown_if_idle() noexcept -> bool;

  private:
    SessionId session_id_;
    CoordinatorRegistry registry_;
    SessionCoordinator coordinator_;
    mutable std::mutex connections_mutex_;
    std::vector<std::unique_ptr<CoordinatorConnection>> connections_;
    std::atomic<int> client_count_{0};
    std::atomic<int64_t> last_disconnect_ms_{0};
    std::atomic<bool> shutdown_requested_{false};

    auto createConnectionObject() -> juce::InterprocessConnection * override;
};

} // namespace xen::ipc
