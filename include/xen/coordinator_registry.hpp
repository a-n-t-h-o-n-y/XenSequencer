#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <xen/state.hpp>

namespace xen::ipc
{

inline constexpr auto coordinator_protocol_version = 1;
inline constexpr auto coordinator_helper_version = "0.3.1";

struct CoordinatorRegistryEntry
{
    SessionId session_id{};
    int port{};
    std::uint32_t pid{};
    std::string nonce{};
    int protocol_version{coordinator_protocol_version};
    std::string helper_version{coordinator_helper_version};
};

class CoordinatorRegistry
{
  public:
    explicit CoordinatorRegistry(std::filesystem::path registry_file);

    [[nodiscard]] auto path() const -> std::filesystem::path const &;
    [[nodiscard]] auto read() const -> std::optional<CoordinatorRegistryEntry>;
    void write(CoordinatorRegistryEntry const &entry) const;
    void clear() const;

    [[nodiscard]] static auto default_directory() -> std::filesystem::path;
    [[nodiscard]] static auto default_file(SessionId const &session_id)
        -> std::filesystem::path;
    [[nodiscard]] static auto active_entries() -> std::vector<CoordinatorRegistryEntry>;
    [[nodiscard]] static auto current_process_id() -> std::uint32_t;
    [[nodiscard]] static auto entry_is_live(CoordinatorRegistryEntry const &entry)
        -> bool;

  private:
    std::filesystem::path registry_file_;
};

} // namespace xen::ipc
