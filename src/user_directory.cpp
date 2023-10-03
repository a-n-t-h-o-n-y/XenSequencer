#include <xen/user_directory.hpp>

#include <stdexcept>

#include <juce_core/juce_core.h>

#include <embed_keys.hpp>

namespace xen
{

auto get_user_data_directory() -> juce::File
{
    auto const data_dir =
        juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("XenSequencer");

    // Create directory if it doesn't exist
    if (!data_dir.exists() && !data_dir.createDirectory().wasOk())
    {
        throw std::runtime_error("Unable to create user data directory: " +
                                 data_dir.getFullPathName().toStdString() + ".");
    }
    return data_dir;
}

auto get_projects_directory() -> juce::File
{
    auto const projects_dir = get_user_data_directory().getChildFile("projects");

    // Create directory if it doesn't exist
    if (!projects_dir.exists() && !projects_dir.createDirectory().wasOk())
    {
        throw std::runtime_error("Unable to create projects directory: " +
                                 projects_dir.getFullPathName().toStdString() + ".");
    }
    return projects_dir;
}

auto get_project_directory(std::string const &project_name) -> juce::File
{
    auto const project_dir = get_projects_directory().getChildFile(project_name);

    // Create directory if it doesn't exist
    if (!project_dir.exists() && !project_dir.createDirectory().wasOk())
    {
        throw std::runtime_error("Unable to create project directory: " +
                                 project_dir.getFullPathName().toStdString() + ".");
    }
    return project_dir;
}

auto get_keybinding_file() -> juce::File
{
    auto const key_file = get_user_data_directory().getChildFile("keys.yml");

    // Check if the file exists, if not create it.
    if (!key_file.existsAsFile())
    {
        // write out the default key file
        if (key_file.create().wasOk())
        {
            key_file.replaceWithText(
                juce::String::fromUTF8(embed_keys::keys_yml, embed_keys::keys_ymlSize));
        }
        else
        {
            throw std::runtime_error("Unable to create keybinding file: " +
                                     key_file.getFullPathName().toStdString() + ".");
        }
    }

    return key_file;
}

} // namespace xen