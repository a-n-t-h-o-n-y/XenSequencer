#pragma once

#include <cstddef>
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
 * @brief Return string with leading and trailing whitespace removed.
 */
[[nodiscard]] auto strip(std::string const &input) -> std::string;

/**
 * @brief Minimizes spaces in a given string, while preserving spaces within double
 * quotes.
 *
 * Removes leading, trailing, and adjacent spaces.
 *
 * @param input The input string.
 * @return std::string The modified string with minimized spaces.
 */
[[nodiscard]] auto minimize_spaces(std::string const &input) -> std::string;

/**
 * @brief Returns the first word from a given string, considering double quotes.
 *
 * Words are space delimited, unless within double quotes.
 *
 * @param input The input string.
 * @return std::string The first word, or empty string if no words.
 */
[[nodiscard]] auto get_first_word(std::string const &input) -> std::string;

/**
 * @brief Returns the remaining part of the string after extracting the first word.
 *
 * Words are space delimited, unless within double quotes.
 *
 * @param input The input string.
 * @return std::string The string after popping the first word.
 */
[[nodiscard]] auto pop_first_word(std::string const &input) -> std::string;

/**
 * @brief Counts the number of words in a string.
 *
 * This function takes a string and counts the number of words separated by spaces.
 * It also takes into account words enclosed in double quotes.
 * For example: word_count("hello world") will return 2.
 *              word_count("hello \"world again\"") will return 2.
 *
 * @param input The string in which to count the words.
 * @return std::size_t The number of words in the string.
 */
[[nodiscard]] auto word_count(std::string const &input) -> std::size_t;

} // namespace xen
