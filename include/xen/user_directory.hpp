#pragma once

#include <juce_core/juce_core.h>

namespace xen
{

/**
 * Retrieve the location of the user library directory for the current OS.
 *
 * @return The filesystem path of the user library directory.
 */
[[nodiscard]] auto get_user_library_directory() -> juce::File;

/**
 * Retrieve the location of the sequences directory.
 *
 * @details If the directory does not exist, it will be created.
 * @return The filesystem path of the sequences directory.
 */
[[nodiscard]] auto get_sequences_directory() -> juce::File;

/**
 * Retrieve the location of the tunings directory.
 *
 * @details If the directory does not exist, it will be created.
 * @return The filesystem path of the tunings directory.
 */
[[nodiscard]] auto get_tunings_directory() -> juce::File;

/**
 * Retrieve the location of the system keys.yml configuration file.
 *
 * @details If the file does not exist, it will be created. If it is outdated, it will
 * be overwritten.
 * @return The filesystem path of the keybinding file.
 * @throws std::runtime_error if the file cannot be created.
 */
[[nodiscard]] auto get_system_keys_file() -> juce::File;

/**
 * Retrieve the location of the user keybinding configuration file.
 *
 * @details If the file does not exist, it will be created.
 * @return The filesystem path of the user keybinding file.
 * @throws std::runtime_error if the file cannot be created.
 */
[[nodiscard]] auto get_user_keys_file() -> juce::File;

/**
 * Retrieve the location of the system scales.yml file.
 *
 * @details If the file does not exist, it will be created. If it is outdated, it will
 * be overwritten.
 * @return The filesystem path of the system scales file.
 * @throws std::runtime_error if the file cannot be created.
 */
[[nodiscard]] auto get_system_scales_file() -> juce::File;

/**
 * Retrieve the location of the user_scales.yml file.
 *
 * @details If the file does not exist, it will be created.
 * @return The filesystem path of the user scales file.
 * @throws std::runtime_error if the file cannot be created.
 */
[[nodiscard]] auto get_user_scales_file() -> juce::File;

/**
 * Retrieve the location of the system chords.yml file.
 *
 * @details If the file does not exist, it will be created. If it is outdated, it will
 * be overwritten.
 * @return The filesystem path of the system chords file.
 * @throws std::runtime_error if the file cannot be created.
 */
[[nodiscard]] auto get_system_chords_file() -> juce::File;

/**
 * Retrieve the location of the user_chords.yml file.
 *
 * @details If the file does not exist, it will be created.
 * @return The filesystem path of the user chords file.
 * @throws std::runtime_error if the file cannot be created.
 */
[[nodiscard]] auto get_user_chords_file() -> juce::File;

/**
 * Create and populate the demos/ directory with the demo files.
 */
void initialize_demo_files();

} // namespace xen