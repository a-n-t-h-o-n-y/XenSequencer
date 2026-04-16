#pragma once

#include <optional>
#include <variant>

#include <sequence/sequence.hpp>

namespace xen
{

using CopyBufferContent = std::variant<sequence::Cell, sequence::MusicElement>;

/**
 * Write the selected content to the shared copy buffer file.
 * @param content The content to write to the copy buffer.
 * @throws std::runtime_error If the copy buffer file can't be opened or the file lock
 * can't be acquired.
 */
void write_copy_buffer(CopyBufferContent const &content);

/**
 * Read the selected content from the shared copy buffer file.
 * @return The content read from the copy buffer, or std::nullopt if the copy buffer
 * file does not exist.
 * @throws std::runtime_error If the copy buffer file can't be opened or the file lock
 * can't be acquired.
 */
[[nodiscard]] auto read_copy_buffer() -> std::optional<CopyBufferContent>;

} // namespace xen
