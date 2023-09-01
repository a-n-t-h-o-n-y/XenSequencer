#pragma once

#include <string>

namespace xen
{

/**
 * @brief Converts a string to lowercase.
 *
 * @param x The string to convert.
 * @return std::string The converted string.
 */
[[nodiscard]] auto to_lower(std::string x) -> std::string;

/**
 * @brief Custom toupper function that also handles some keyboard symbols.
 *
 * @param ch Input character.
 * @return Uppercased character or the corresponding shifted keyboard symbol.
 */
[[nodiscard]] auto keyboard_toupper(char ch) -> char;

/**
 * @brief Custom tolower function that also handles some keyboard symbols.
 *
 * @param ch Input character.
 * @return Lowercased character or the corresponding unshifted keyboard symbol.
 */
[[nodiscard]] auto keyboard_tolower(char ch) -> char;

/**
 * @brief False type for else conditions in constexpr if statements.
 */
template <typename T>
struct always_false : std::false_type
{
};

/**
 * @brief Reads the content of a text file into a std::string.
 *
 * @param filepath Path of the text file to read.
 * @return std::string Contents of the text file.
 * @throws std::runtime_error Thrown if file cannot be read.
 */
[[nodiscard]] auto read_file_to_string(std::string const &filepath) -> std::string;

/**
 * @brief Writes a std::string to a text file.
 *
 * @param filepath Path of the text file to write.
 * @param content Content to write to the text file.
 * @throws std::runtime_error Thrown if file cannot be written to.
 */
auto write_string_to_file(std::string const &filepath, std::string const &content)
    -> void;

} // namespace xen