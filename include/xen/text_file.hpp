#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace xen
{

[[nodiscard]] auto read_text_file(std::filesystem::path const &path)
    -> std::optional<std::string>;

[[nodiscard]] auto read_text_file(std::filesystem::path const &path,
                                  std::size_t maximum_bytes)
    -> std::optional<std::string>;

[[nodiscard]] auto text_revision(std::string const &text) -> std::string;

[[nodiscard]] auto file_revision(std::filesystem::path const &path,
                                 std::size_t maximum_bytes)
    -> std::optional<std::string>;

void atomic_write_text_file(std::filesystem::path const &path, std::string const &text);

void atomic_write_text_file_checked(std::filesystem::path const &path,
                                    std::string const &text,
                                    std::function<void()> before_install);

} // namespace xen
