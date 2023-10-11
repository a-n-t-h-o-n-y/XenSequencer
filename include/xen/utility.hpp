#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>

namespace xen
{

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

/**
 * @brief Checks if a tuple contains a single element that satisfies a given predicate.
 *
 * @param predicate The predicate function to apply.
 * @param tupl The tuple to check.
 * @return bool True if there is exactly one element in the tuple that satisfies the
 * predicate, false otherwise.
 */
template <typename PredicateFn, typename... Args>
[[nodiscard]] auto has_unique_match(PredicateFn const &predicate,
                                    std::tuple<Args...> const &tupl) -> bool
{
    static_assert(std::conjunction_v<std::is_invocable_r<bool, PredicateFn, Args>...>,
                  "Predicate must be invocable with each type in the tuple and return "
                  "a type convertible to bool.");

    auto const count = std::apply(
        [&predicate](Args const &...args) {
            return (... + static_cast<int>(predicate(args)));
        },
        tupl);

    return count == 1;
}

struct ErrorNoMatch
{
};

/**
 * @brief Applies a function to the first element in a tuple that satisfies a given
 * predicate.
 *
 * @tparam R The type of the result.
 * @param predicate The predicate function to apply.
 * @param apply The function to apply to the first matching element.
 * @param tupl The tuple to operate on.
 * @return R The result of applying the function.
 * @exception std::runtime_error Thrown if no element matches the predicate.
 */
template <typename R, typename PredicateFn, typename ApplyFn, typename... Args>
auto apply_if(PredicateFn const &predicate, ApplyFn const &apply,
              std::tuple<Args...> const &tupl) -> R
{
    static_assert(std::conjunction_v<std::is_invocable_r<bool, PredicateFn, Args>...>,
                  "Predicate must be invocable with each type in the tuple and return "
                  "a type convertible to bool.");

    static_assert(std::conjunction_v<std::is_invocable_r<R, ApplyFn, Args>...>,
                  "Apply function must be invocable with each type in the tuple and "
                  "return a type of R.");

    auto result = std::optional<R>{std::nullopt};

    auto apply_if_predicate = [&](auto &&elem) {
        if (!result && predicate(elem))
        {
            result = apply(elem);
        }
    };

    std::apply([&](auto &&...args) { (apply_if_predicate(args), ...); }, tupl);

    if (!result)
    {
        throw ErrorNoMatch{};
    }

    return *result;
}

/**
 * @brief Normalizes an interval to the range [0, length).
 *
 * -1 wraps around to length - 1.
 */
[[nodiscard]] inline auto normalize_interval(int interval, std::size_t length)
    -> std::size_t
{
    return static_cast<std::size_t>(((interval % (int)length) + (int)length) %
                                    (int)length);
}

} // namespace xen
