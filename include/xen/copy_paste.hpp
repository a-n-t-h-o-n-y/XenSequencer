#pragma once

#include <optional>

#include <sequence/sequence.hpp>

namespace xen
{

/**
 * Write the Cell to the shared copy buffer file.
 * @param cell The Cell to write to the copy buffer.
 * @throws std::runtime_error If the copy buffer file can't be opened or the file lock
 * can't be acquired.
 */
void write_copy_buffer(sequence::Cell const &cell);

/**
 * Read the Cell from the shared copy buffer file.
 * @return The Cell read from the copy buffer, or std::nullopt if the copy buffer file
 * does not exist.
 * @throws std::runtime_error If the copy buffer file can't be opened or the file lock
 * can't be acquired.
 */
[[nodiscard]] auto read_copy_buffer() -> std::optional<sequence::Cell>;

} // namespace xen