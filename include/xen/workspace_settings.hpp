#pragma once

#include <filesystem>

#include <xen/state.hpp>

namespace xen
{

class WorkspaceSettingsStore
{
  public:
    explicit WorkspaceSettingsStore(std::filesystem::path file = default_file());

    [[nodiscard]] auto load_or_initialize() const -> WorkspaceSettings;
    void save(WorkspaceSettings const &settings) const;
    [[nodiscard]] auto file() const -> std::filesystem::path const &;

    [[nodiscard]] static auto default_file() -> std::filesystem::path;

  private:
    std::filesystem::path file_;
};

} // namespace xen
