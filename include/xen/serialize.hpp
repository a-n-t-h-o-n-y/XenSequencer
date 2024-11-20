#pragma once

#include <array>
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
 * @param m The sequence::Measure object to serialize.
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
 * Serialize a SequenceBank object to a JSON string.
 *
 * @param bank The SequenceBank object to serialize.
 * @param sequence_names The names of the sequences in the bank.
 * @return std::string The JSON string.
 */
[[nodiscard]] auto serialize_sequence_bank(
    SequenceBank const &bank, std::array<std::string, 16> const &sequence_names)
    -> std::string;

/**
 * Deserialize a JSON string to a SequenceBank object.
 *
 * @param json_str The JSON string to deserialize.
 * @return SequenceBank The deserialized SequenceBank object.
 * @throw std::invalid_argument If the JSON string is invalid.
 */
[[nodiscard]] auto deserialize_sequence_bank(std::string const &json_str)
    -> std::pair<SequenceBank, std::array<std::string, 16>>;

/**
 * Serialize the full plugin state to a JSON string.
 *
 * @param state The plugin state to serialize.
 * @return std::string The JSON string.
 */
[[nodiscard]] auto serialize_plugin(SequencerState const &state) -> std::string;

/**
 * Deserialize a JSON string to a plugin state and metadata.
 *
 * @param json_str The JSON string to deserialize.
 * @return SequencerState The deserialized plugin state.
 * @throw std::invalid_argument If the JSON string is invalid.
 */
[[nodiscard]] auto deserialize_plugin(std::string const &json_str) -> SequencerState;

} // namespace xen