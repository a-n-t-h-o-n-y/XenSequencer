#pragma once

#include <string>
#include <utility>

#include <xen/state.hpp>

namespace xen
{

/**
 * Serialize a SequencerState object to a JSON string.
 *
 * @param state The SequencerState object to serialize.
 * @return std::string The JSON string.
 */
[[nodiscard]] auto serialize_state(SequencerState const &state) -> std::string;

/**
 * Deserialize a JSON string to a SequencerState object.
 *
 * @param json_str The JSON string to deserialize.
 * @return SequencerState The deserialized SequencerState object.
 * @throw std::invalid_argument If the JSON string is invalid.
 */
[[nodiscard]] auto deserialize_state(std::string const &json_str) -> SequencerState;

/**
 * Serialize the full plugin state to a JSON string.
 *
 * @param state The plugin state to serialize.
 * @param display_name The display name of the plugin.
 * @return std::string The JSON string.
 */
[[nodiscard]] auto serialize_plugin(SequencerState const &state,
                                    std::string const &display_name) -> std::string;

/**
 * Deserialize a JSON string to a plugin state and metadata.
 *
 * @param json_str The JSON string to deserialize.
 * @return std::pair<SequencerState, std::string> The deserialized plugin state and
 * display_name.
 * @throw std::invalid_argument If the JSON string is invalid.
 */
[[nodiscard]] auto deserialize_plugin(std::string const &json_str)
    -> std::pair<SequencerState, std::string>;

} // namespace xen