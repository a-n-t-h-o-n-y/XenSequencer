#pragma once

#include <juce_core/juce_core.h>

namespace xen
{

/**
 * @brief Retrieve the location of the user data directory for the current OS.
 *
 * @return juce::File The absolute path of the user data directory.
 */
[[nodiscard]] auto get_user_data_directory() -> juce::File;

/**
 * @brief Retrieve the location of the projects directory.
 *
 * If the directory does not exist, it will be created.
 *
 * @return juce::File The absolute path of the projects directory.
 */
[[nodiscard]] auto get_projects_directory() -> juce::File;

/**
 * @brief Retrieve the location of a specific project directory.
 *
 * If the directory does not exist, it will be created.
 *
 * @param project_name The name of the project.
 * @return juce::File The absolute path of the project directory.
 */
[[nodiscard]] auto get_project_directory(std::string const &project_name) -> juce::File;

/**
 * @brief Retrieve the location of the keybinding configuration file.
 *
 * If the file does not exist, it will be created.
 *
 * @return juce::File The absolute path of the keybinding file.
 * @throws std::runtime_error if the file cannot be created.
 */
[[nodiscard]] auto get_keybinding_file() -> juce::File;

} // namespace xen