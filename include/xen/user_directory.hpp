#pragma once

#include <filesystem>

namespace xen
{

/**
 * @brief Retrieve the location of the user data directory for the current OS.
 *
 * @return The filesystem path of the user data directory.
 */
[[nodiscard]] auto get_user_data_directory() -> std::filesystem::path;

/**
 * @brief Retrieve the location of the projects directory.
 *
 * If the directory does not exist, it will be created.
 *
 * @return The filesystem path of the projects directory.
 */
[[nodiscard]] auto get_projects_directory() -> std::filesystem::path;

/**
 * @brief Retrieve the location of a specific project directory.
 *
 * If the directory does not exist, it will be created.
 *
 * @param project_name The name of the project.
 * @return The filesystem path of the project directory.
 */
[[nodiscard]] auto get_project_directory(std::string const &project_name)
    -> std::filesystem::path;

/**
 * @brief Retrieve the location of the default keys.yml configuration file.
 *
 * If the file does not exist, it will be created.
 *
 * @return The filesystem path of the keybinding file.
 * @throws std::runtime_error if the file cannot be created.
 */
[[nodiscard]] auto get_default_keys_file() -> std::filesystem::path;

/**
 * @brief Retrieve the location of the user keybinding configuration file.
 *
 * If the file does not exist, it will be created.
 *
 * @return The filesystem path of the user keybinding file.
 * @throws std::runtime_error if the file cannot be created.
 */
[[nodiscard]] auto get_user_keys_file() -> std::filesystem::path;

} // namespace xen