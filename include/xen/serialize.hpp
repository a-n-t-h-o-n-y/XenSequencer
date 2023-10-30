#pragma once

#include <string>
#include <utility>

#include <xen/state.hpp>

namespace xen
{

/**
 * @brief Serialize a State object to a JSON string.
 *
 * @param state The State object to serialize.
 * @return std::string The JSON string.
 */
[[nodiscard]] auto serialize_state(State const &state) -> std::string;

/**
 * @brief Deserialize a JSON string to a State object.
 *
 * @param json_str The JSON string to deserialize.
 * @return State The deserialized State object.
 * @throw std::invalid_argument If the JSON string is invalid.
 */
[[nodiscard]] auto deserialize_state(std::string const &json_str) -> State;

/**
 * @brief Serialize the full plugin state to a JSON string.
 *
 * @param state The plugin state to serialize.
 * @param metadata The plugin metadata to serialize.
 * @return std::string The JSON string.
 */
[[nodiscard]] auto serialize_plugin(State const &state, Metadata const &metadata)
    -> std::string;

/**
 * @brief Deserialize a JSON string to a plugin state and metadata.
 *
 * @param json_str The JSON string to deserialize.
 * @return std::pair<State, Metadata> The deserialized plugin state and metadata.
 * @throw std::invalid_argument If the JSON string is invalid.
 */
[[nodiscard]] auto deserialize_plugin(std::string const &json_str)
    -> std::pair<State, Metadata>;

} // namespace xen