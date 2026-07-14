#pragma once

#include <string>

#include <sequence/sequence.hpp>
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
 * Serialize a Cell object to a JSON string.
 *
 * @param cell The Cell object to serialize.
 * @return std::string The JSON string.
 */
[[nodiscard]] auto serialize_cell_file(sequence::Cell const &cell) -> std::string;

/**
 * Deserialize a JSON string to a Cell object.
 *
 * @param json_str The JSON string to deserialize.
 * @return Cell The deserialized Cell object.
 * @throw std::invalid_argument If the JSON string is invalid.
 */
[[nodiscard]] auto deserialize_cell_file(std::string const &json_str) -> sequence::Cell;

[[nodiscard]] auto serialize_composition(ProjectState const &project) -> std::string;
[[nodiscard]] auto deserialize_composition(std::string const &json_str) -> ProjectState;

/**
 * Serialize project state using project schema 5.
 *
 * @param state The plugin state to serialize.
 * @return std::string The JSON string.
 */
[[nodiscard]] auto serialize_project(ProjectState const &project) -> std::string;

/**
 * Deserialize project schema 5.
 *
 * @param json_str The JSON string to deserialize.
 * @return ProjectState The deserialized project state.
 * @throw std::invalid_argument If the JSON string is invalid.
 */
[[nodiscard]] auto deserialize_project(std::string const &json_str) -> ProjectState;

[[nodiscard]] auto serialize_processor_state(InstanceBinding const &binding,
                                             ProjectSnapshot const &snapshot)
    -> std::string;

[[nodiscard]] auto deserialize_processor_state(std::string const &json_str)
    -> PersistedProcessorState;

} // namespace xen
