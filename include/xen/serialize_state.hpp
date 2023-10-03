#pragma once

#include <string>

#include <xen/state.hpp>

namespace xen
{

/**
 * @brief Serialize a State object to a JSON string.
 *
 * @param state The State object to serialize.
 * @return std::string The JSON string.
 */
[[nodiscard]] auto serialize(State const &state) -> std::string;

/**
 * @brief Deserialize a JSON string to a State object.
 *
 * @param json_str The JSON string to deserialize.
 * @return State The deserialized State object.
 * @throw std::invalid_argument If the JSON string is invalid.
 */
[[nodiscard]] auto deserialize(std::string const &json_str) -> State;

} // namespace xen