#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace xen
{

/**
 * Converts a string to lowercase.
 *
 * @param x The string to convert.
 * @return std::string The converted string.
 */
[[nodiscard]] auto to_lower(std::string x) -> std::string;

/**
 * Return string with leading and trailing whitespace removed.
 */
[[nodiscard]] auto strip(std::string const &input) -> std::string;

/**
 * Minimizes spaces in a given string, while preserving spaces within double
 * quotes.
 *
 * @details Removes leading, trailing, and adjacent spaces.
 * @param input The input string.
 * @return std::string The modified string with minimized spaces.
 */
[[nodiscard]] auto minimize_spaces(std::string const &input) -> std::string;

/**
 * Returns the first word from a given string, considering double quotes.
 *
 * @details Words are space delimited, unless within double quotes.
 * @param input The input string.
 * @return std::string The first word, or empty string if no words.
 */
[[nodiscard]] auto get_first_word(std::string const &input) -> std::string;

/**
 * Returns the remaining part of the string after extracting the first word.
 *
 * @details Words are space delimited, unless within double quotes.
 * @param input The input string.
 * @return std::string The string after popping the first word.
 */
[[nodiscard]] auto pop_first_word(std::string const &input) -> std::string;

/**
 * Counts the number of words in a string.
 *
 * @details This function takes a string and counts the number of words separated by
 * spaces. It also takes into account words enclosed in double quotes.
 * For example: word_count("hello world") will return 2.
 *              word_count("hello \"world again\"") will return 2.
 * @param input The string in which to count the words.
 * @return std::size_t The number of words in the string.
 */
[[nodiscard]] auto word_count(std::string const &input) -> std::size_t;

/**
 * Splits a string into a vector of strings based on a delimiter.
 *
 * @param input The string to split.
 * @param delimiter The character to split on.
 * @return Container of strings, without the delimiter.
 */
[[nodiscard]] auto split(std::string const &input,
                         char delimiter) -> std::vector<std::string>;

/**
 * Surrounds a string with double quotes.
 *
 * @param input The string to surround with double quotes.
 * @return std::string The string surrounded by double quotes.
 */
[[nodiscard]] auto double_quote(std::string const &input) -> std::string;

// make single_quote() fn:

/**
 * Surrounds a string with single quotes.
 *
 * @param input The string to surround with single quotes.
 * @return std::string The string surrounded by single quotes.
 */
[[nodiscard]] auto single_quote(std::string const &input) -> std::string;

} // namespace xen
