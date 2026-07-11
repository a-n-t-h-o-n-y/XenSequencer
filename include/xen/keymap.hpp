#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace xen
{

inline constexpr std::size_t MAX_KEYMAP_DOCUMENT_SIZE = 4 * 1'024 * 1'024;

struct KeymapResource
{
    std::uint64_t revision{};
    std::optional<nlohmann::json> document{};

    auto operator==(KeymapResource const &) const -> bool = default;
};

enum class KeymapStorageErrorCode
{
    Conflict,
    MalformedDocument,
    Read,
    Write,
    Delete,
};

class KeymapStorageError final : public std::runtime_error
{
  public:
    KeymapStorageError(KeymapStorageErrorCode code, std::string message);

    KeymapStorageErrorCode code;
};

class KeymapStore
{
  public:
    using AtomicWriter =
        std::function<void(std::filesystem::path const &, std::string const &)>;
    using FileRemover = std::function<void(std::filesystem::path const &)>;

    explicit KeymapStore(std::filesystem::path file = default_file());
    KeymapStore(std::filesystem::path file, AtomicWriter atomic_writer,
                FileRemover file_remover = remove_file);

    auto read() -> KeymapResource;
    auto write(std::uint64_t expected_revision, nlohmann::json document)
        -> KeymapResource;
    auto erase(std::uint64_t expected_revision) -> KeymapResource;
    auto refresh() -> bool;

    [[nodiscard]] auto current() const -> KeymapResource const &;
    [[nodiscard]] auto revision() const noexcept -> std::uint64_t;
    [[nodiscard]] auto file() const -> std::filesystem::path const &;
    [[nodiscard]] static auto default_file() -> std::filesystem::path;
    static void remove_file(std::filesystem::path const &file);

  private:
    [[nodiscard]] auto load_resource() const -> KeymapResource;
    void require_revision(std::uint64_t expected_revision) const;

    std::filesystem::path file_;
    AtomicWriter atomic_writer_;
    FileRemover file_remover_;
    KeymapResource current_{};
};

} // namespace xen
