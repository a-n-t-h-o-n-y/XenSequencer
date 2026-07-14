#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace xen
{

inline constexpr std::size_t MAX_PREFERENCES_DOCUMENT_SIZE = 4 * 1'024 * 1'024;

struct PreferencesResource
{
    std::uint64_t revision{};
    std::optional<nlohmann::json> document{};

    auto operator==(PreferencesResource const &) const -> bool = default;
};

enum class PreferencesStorageErrorCode
{
    Conflict,
    MalformedDocument,
    Read,
    Write,
    Delete,
};

class PreferencesStorageError final : public std::runtime_error
{
  public:
    PreferencesStorageError(PreferencesStorageErrorCode code, std::string message);

    PreferencesStorageErrorCode code;
};

class PreferencesStore
{
  public:
    using AtomicWriter =
        std::function<void(std::filesystem::path const &, std::string const &)>;
    using FileRemover = std::function<void(std::filesystem::path const &)>;

    explicit PreferencesStore(std::filesystem::path file = default_file());
    PreferencesStore(std::filesystem::path file, AtomicWriter atomic_writer,
                     FileRemover file_remover = remove_file);

    auto read() -> PreferencesResource;
    auto write(std::uint64_t expected_revision, nlohmann::json document)
        -> PreferencesResource;
    auto erase(std::uint64_t expected_revision) -> PreferencesResource;
    auto refresh() -> bool;

    [[nodiscard]] auto current() const -> PreferencesResource const &;
    [[nodiscard]] auto revision() const noexcept -> std::uint64_t;
    [[nodiscard]] auto file() const -> std::filesystem::path const &;
    [[nodiscard]] static auto default_file() -> std::filesystem::path;
    static void remove_file(std::filesystem::path const &file);

  private:
    [[nodiscard]] auto load_resource() const -> PreferencesResource;
    void require_revision(std::uint64_t expected_revision) const;

    std::filesystem::path file_;
    AtomicWriter atomic_writer_;
    FileRemover file_remover_;
    PreferencesResource current_{};
};

} // namespace xen
