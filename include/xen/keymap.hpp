#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace xen
{

inline constexpr auto KEYMAP_SCHEMA_VERSION = 1;

struct KeymapTrigger
{
    std::string key{};
    bool shift{false};
    bool command{false};
    bool alt{false};
    std::optional<std::string> input_mode{};

    auto operator==(KeymapTrigger const &) const -> bool = default;
};

enum class KeymapTargetType
{
    UiAction,
    Command,
};

struct KeymapTarget
{
    KeymapTargetType type{KeymapTargetType::Command};
    std::string value{};
    nlohmann::json arguments = nlohmann::json::object();

    auto operator==(KeymapTarget const &) const -> bool = default;
};

struct KeymapBinding
{
    KeymapTrigger trigger{};
    KeymapTarget target{};

    auto operator==(KeymapBinding const &) const -> bool = default;
};

using KeymapContexts = std::map<std::string, std::vector<KeymapBinding>>;

struct KeymapOverride
{
    std::string context{};
    KeymapTrigger trigger{};
    std::optional<KeymapTarget> target{};

    auto operator==(KeymapOverride const &) const -> bool = default;
};

struct KeymapSnapshot
{
    std::uint64_t revision{1};
    KeymapContexts bindings{};
    std::vector<KeymapOverride> overrides{};
};

[[nodiscard]] auto default_keymap() -> KeymapContexts;
void validate(KeymapTrigger const &trigger);
void validate(KeymapTarget const &target);
void validate(KeymapOverride const &override);

class KeymapStore
{
  public:
    explicit KeymapStore(std::filesystem::path file = default_file());

    [[nodiscard]] auto snapshot() const -> KeymapSnapshot;
    [[nodiscard]] auto revision() const noexcept -> std::uint64_t;
    auto set_override(std::uint64_t expected_revision, std::string context,
                      KeymapTrigger trigger, std::optional<KeymapTarget> target)
        -> KeymapSnapshot;
    auto remove_override(std::uint64_t expected_revision, std::string const &context,
                         KeymapTrigger const &trigger) -> KeymapSnapshot;
    auto reset(std::uint64_t expected_revision) -> KeymapSnapshot;

    [[nodiscard]] auto file() const -> std::filesystem::path const &;
    [[nodiscard]] static auto default_file() -> std::filesystem::path;

  private:
    void load();
    void save() const;
    void require_revision(std::uint64_t expected_revision) const;

    std::filesystem::path file_;
    std::uint64_t revision_{1};
    std::vector<KeymapOverride> overrides_{};
};

} // namespace xen
