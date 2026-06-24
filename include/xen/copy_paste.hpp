#pragma once

#include <filesystem>
#include <variant>

#include <sequence/sequence.hpp>

namespace xen
{

using CopyBufferContent = std::variant<sequence::Cell, sequence::MusicElement>;

[[nodiscard]] auto copy_buffer_filepath() -> std::filesystem::path;

} // namespace xen
