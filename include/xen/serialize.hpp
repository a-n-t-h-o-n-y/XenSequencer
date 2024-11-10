#pragma once

#include <string>
#include <utility>

#include <sequence/measure.hpp>

#include <xen/state.hpp>

namespace xen
{

/**
 * Serialize a sequence::Cell object to a JSON string.
 * @param c The sequence::Cell object to serialize.
 * @return std::string The JSON string.
 */
[[nodiscard]] auto serialize_cell(sequence::Cell const &c) -> std::string;

/**
 * Deserialize a JSON string to a sequence::Cell object.
 * @param json_str The JSON string to deserialize.
 * @return sequence::Cell The deserialized Cell object.
 * @throw std::invalid_argument If the JSON string is invalid.
 */
[[nodiscard]] auto deserialize_cell(std::string const &json_str) -> sequence::Cell;

/**
 * Serialize a sequence::Measure object to a JSON string.
 *
 * @param state The sequence::Measure object to serialize.
 * @return std::string The JSON string.
 */
[[nodiscard]] auto serialize_measure(sequence::Measure const &m) -> std::string;

/**
 * Deserialize a JSON string to a sequence::Measure object.
 *
 * @param json_str The JSON string to deserialize.
 * @return Measure The deserialized Measure object.
 * @throw std::invalid_argument If the JSON string is invalid.
 */
[[nodiscard]] auto deserialize_measure(std::string const &json_str)
    -> sequence::Measure;

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