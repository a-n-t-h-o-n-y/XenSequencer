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
 * Retrieve the location of the default keys.yml configuration file.
 *
 * If the file does not exist, it will be created.
 *
 * @return The filesystem path of the keybinding file.
 * @throws std::runtime_error if the file cannot be created.
 */
[[nodiscard]] auto get_default_keys_file() -> juce::File;

/**
 * Retrieve the location of the user keybinding configuration file.
 *
 * If the file does not exist, it will be created.
 *
 * @return The filesystem path of the user keybinding file.
 * @throws std::runtime_error if the file cannot be created.
 */
[[nodiscard]] auto get_user_keys_file() -> juce::File;

/**
 * Create a populate the demos/ directory with the demo files.
 */
auto initialize_demo_files() -> void;

} // namespace xen