#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <xen/document.hpp>

namespace xen
{

inline constexpr auto MAX_CELL_FILE_BYTES = std::size_t{16 * 1'024 * 1'024};
inline constexpr auto MAX_PROJECT_FILE_BYTES = std::size_t{64 * 1'024 * 1'024};
inline constexpr auto MAX_TUNING_FILE_BYTES = std::size_t{4 * 1'024 * 1'024};

struct StoredContentFile
{
    ContentFileInfo file{};
    std::string text{};
};

void recover_content_directory(std::filesystem::path const &content_directory);

[[nodiscard]] auto resolve_content_path(std::filesystem::path const &content_directory,
                                        std::string const &relative_path,
                                        std::string_view required_extension)
    -> std::filesystem::path;

[[nodiscard]] auto read_content_file(std::filesystem::path const &content_directory,
                                     std::string const &relative_path,
                                     std::string_view required_extension,
                                     std::size_t maximum_bytes) -> StoredContentFile;

[[nodiscard]] auto write_content_file(
    std::filesystem::path const &content_directory, std::string const &relative_path,
    std::string_view required_extension, std::string const &text,
    std::size_t maximum_bytes, std::optional<std::string> const &expected_file_revision)
    -> ContentFileInfo;

} // namespace xen
