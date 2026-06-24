#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace xen
{

[[nodiscard]] auto read_text_file(std::filesystem::path const &path)
    -> std::optional<std::string>;

void atomic_write_text_file(std::filesystem::path const &path, std::string const &text);

} // namespace xen
