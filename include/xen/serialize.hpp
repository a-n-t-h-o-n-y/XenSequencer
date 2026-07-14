#pragma once

#include <cstddef>
#include <string>

#include <sequence/sequence.hpp>
#include <xen/state.hpp>

namespace xen
{

inline constexpr auto MAX_PERSISTED_STATE_BYTES = std::size_t{65 * 1'024 * 1'024};

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

/**
 * Serialize a complete .xenproj document.
 *
 * @param project The project state to serialize.
 * @return std::string The JSON string.
 */
[[nodiscard]] auto serialize_project(ProjectState const &project) -> std::string;

/**
 * Deserialize a complete .xenproj document.
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

void validate_persisted_processor_state(PersistedProcessorState const &state);

[[nodiscard]] auto serialize_recovery_state(PersistedRecoveryState const &state)
    -> std::string;

[[nodiscard]] auto deserialize_recovery_state(std::string const &json_str)
    -> PersistedRecoveryState;

} // namespace xen
