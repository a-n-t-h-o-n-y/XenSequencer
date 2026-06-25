#pragma once

#include <filesystem>

#include <juce_core/juce_core.h>

#include <xen/coordinator_registry.hpp>

namespace xen::ipc
{

class CoordinatorLauncher
{
  public:
    explicit CoordinatorLauncher(std::filesystem::path helper_path = {});

    [[nodiscard]] auto helper_path() const -> std::filesystem::path;
    [[nodiscard]] auto ensure_running(SessionId const &session_id,
                                      CoordinatorRegistry const &registry,
                                      juce::ChildProcess *launched_process = nullptr)
        -> CoordinatorRegistryEntry;

  private:
    std::filesystem::path explicit_helper_path_;
};

} // namespace xen::ipc
