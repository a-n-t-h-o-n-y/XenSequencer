#pragma once

#include <string>

#include <xen/copy_paste.hpp>
#include <xen/measure.hpp>
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
 * Serialize a Measure object to a JSON string.
 *
 * @param m The Measure object to serialize.
 * @return std::string The JSON string.
 */
[[nodiscard]] auto serialize_measure(Measure const &m) -> std::string;

/**
 * Deserialize a JSON string to a Measure object.
 *
 * @param json_str The JSON string to deserialize.
 * @return Measure The deserialized Measure object.
 * @throw std::invalid_argument If the JSON string is invalid.
 */
[[nodiscard]] auto deserialize_measure(std::string const &json_str) -> Measure;

/**
 * Serialize project state using project schema 3.
 *
 * @param state The plugin state to serialize.
 * @return std::string The JSON string.
 */
[[nodiscard]] auto serialize_project(ProjectState const &project) -> std::string;

/**
 * Deserialize project schema 3.
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

[[nodiscard]] auto serialize_copy_buffer_content(CopyBufferContent const &content)
    -> std::string;

[[nodiscard]] auto deserialize_copy_buffer_content(std::string const &json_str)
    -> CopyBufferContent;

} // namespace xen
