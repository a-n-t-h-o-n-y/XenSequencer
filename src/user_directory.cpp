#include <xen/user_directory.hpp>

#include <filesystem>
#include <stdexcept>

#include <juce_core/juce_core.h>
#include <yaml-cpp/yaml.h>

#include <embed_keys.hpp>

#include <xen/constants.hpp>

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

auto get_default_keys_file() -> juce::File
{
    auto const key_file = get_user_data_directory().getChildFile("keys.yml");
    auto const full_path = key_file.getFullPathName().toStdString();

    auto write_default_keys = [&key_file] {
        return key_file.create().wasOk() &&
               key_file.replaceWithText(juce::String::fromUTF8(
                   embed_keys::keys_yml, embed_keys::keys_ymlSize));
    };

    auto const file_exists = key_file.existsAsFile();

    if (file_exists)
    {
        auto root = YAML::LoadFile(full_path);
        auto const ver_node = root["version"];
        if (ver_node.IsDefined() && ver_node.as<std::string>() == xen::VERSION)
        {
            return key_file;
        }
    }

    return write_default_keys()
               ? key_file
               : throw std::runtime_error(
                     "Unable to create keybinding file: " + full_path + ".");
}

auto get_user_keys_file() -> juce::File
{
    auto const key_file = get_user_data_directory().getChildFile("user_keys.yml");

    // Check if the file exists, if not create it.
    if (!key_file.existsAsFile())
    {
        // write out the default key file
        if (key_file.create().wasOk())
        {
            key_file.replaceWithText(juce::String::fromUTF8(
                embed_keys::user_keys_yml, embed_keys::user_keys_ymlSize));
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