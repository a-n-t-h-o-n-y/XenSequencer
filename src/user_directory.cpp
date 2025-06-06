#include <xen/user_directory.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>

#include <juce_core/juce_core.h>

#include <yaml-cpp/yaml.h>

#include <embed_chords.hpp>
#include <embed_demos.hpp>
#include <embed_keys.hpp>
#include <embed_scales.hpp>

#include <xen/constants.hpp>

namespace xen
{

auto get_user_library_directory() -> juce::File
{
    auto const library_dir =
        juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("XenSequencer");

    // Create directory if it doesn't exist
    if (!library_dir.exists() && !library_dir.createDirectory().wasOk())
    {
        throw std::runtime_error("Unable to create user library directory: " +
                                 library_dir.getFullPathName().toStdString() + ".");
    }
    return library_dir;
}

auto get_sequences_directory() -> juce::File
{
    auto const seq_dir = get_user_library_directory().getChildFile("sequences");

    // Create directory if it doesn't exist
    if (!seq_dir.exists() && !seq_dir.createDirectory().wasOk())
    {
        throw std::runtime_error("Unable to create sequences directory: " +
                                 seq_dir.getFullPathName().toStdString() + ".");
    }
    return seq_dir;
}

auto get_tunings_directory() -> juce::File
{
    auto const tunings_dir = get_user_library_directory().getChildFile("tunings");

    // Create directory if it doesn't exist
    if (!tunings_dir.exists() && !tunings_dir.createDirectory().wasOk())
    {
        throw std::runtime_error("Unable to create tunings directory: " +
                                 tunings_dir.getFullPathName().toStdString() + ".");
    }
    return tunings_dir;
}

auto get_system_keys_file() -> juce::File
{
    auto const key_file = get_user_library_directory().getChildFile("keys.yml");
    auto const full_path = key_file.getFullPathName().toStdString();

    auto write_system_keys = [&key_file] {
        return key_file.create().wasOk() &&
        key_file.replaceWithData(embed_keys::keys_yml,
                                 (std::size_t)embed_keys::keys_ymlSize);
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

    return write_system_keys()
               ? key_file
               : throw std::runtime_error(
                     "Unable to create keybinding file: " + full_path + ".");
}

auto get_user_keys_file() -> juce::File
{
    auto const key_file = get_user_library_directory().getChildFile("user_keys.yml");

    // Check if the file exists, if not create it.
    if (!key_file.existsAsFile())
    {
        // write out the default key file
        if (key_file.create().wasOk())
        {
            key_file.appendData(embed_keys::user_keys_yml,
                                (std::size_t)embed_keys::user_keys_ymlSize);
        }
        else
        {
            throw std::runtime_error("Unable to create keybinding file: " +
                                     key_file.getFullPathName().toStdString() + ".");
        }
    }

    return key_file;
}

auto get_system_scales_file() -> juce::File
{
    auto const scales_file = get_user_library_directory().getChildFile("scales.yml");
    auto const full_path = scales_file.getFullPathName().toStdString();

    auto write_system_scales = [&scales_file] {
        return scales_file.create().wasOk() &&
               scales_file.replaceWithData(embed_scales::scales_yml,
                                          (std::size_t)embed_scales::scales_ymlSize);
    };

    auto const file_exists = scales_file.existsAsFile();

    if (file_exists)
    {
        auto root = YAML::LoadFile(full_path);
        auto const ver_node = root["version"];
        if (ver_node.IsDefined() && ver_node.as<std::string>() == xen::VERSION)
        {
            return scales_file;
        }
    }

    return write_system_scales()
               ? scales_file
               : throw std::runtime_error("Unable to create scales file: " + full_path +
                                          ".");
}

auto get_user_scales_file() -> juce::File
{
    auto const scales_file =
        get_user_library_directory().getChildFile("user_scales.yml");

    if (!scales_file.existsAsFile())
    {
        // write out the default scales file
        if (scales_file.create().wasOk())
        {
            scales_file.appendData(embed_scales::user_scales_yml,
                                   (std::size_t)embed_scales::user_scales_ymlSize);
        }
        else
        {
            throw std::runtime_error("Unable to create scales file: " +
                                     scales_file.getFullPathName().toStdString() + ".");
        }
    }

    return scales_file;
}

auto get_system_chords_file() -> juce::File
{
    auto const chords_file = get_user_library_directory().getChildFile("chords.yml");
    auto const full_path = chords_file.getFullPathName().toStdString();

    auto write_system_chords = [&chords_file] {
        return chords_file.create().wasOk() &&
               chords_file.replaceWithData(embed_chords::chords_yml,
                                          (std::size_t)embed_chords::chords_ymlSize);
    };

    auto const file_exists = chords_file.existsAsFile();

    if (file_exists)
    {
        auto root = YAML::LoadFile(full_path);
        auto const ver_node = root["version"];
        if (ver_node.IsDefined() && ver_node.as<std::string>() == xen::VERSION)
        {
            return chords_file;
        }
    }

    return write_system_chords()
               ? chords_file
               : throw std::runtime_error("Unable to create chords file: " + full_path +
                                          ".");
}

auto get_user_chords_file() -> juce::File
{
    auto const chords_file =
        get_user_library_directory().getChildFile("user_chords.yml");

    // Check if the file exists, if not create it.
    if (!chords_file.existsAsFile())
    {
        // write out the default chords file
        if (chords_file.create().wasOk())
        {
            chords_file.appendData(embed_chords::user_chords_yml,
                                   (std::size_t)embed_chords::user_chords_ymlSize);
        }
        else
        {
            throw std::runtime_error("Unable to create chords file: " +
                                     chords_file.getFullPathName().toStdString() + ".");
        }
    }

    return chords_file;
}

void initialize_demo_files()
{
    auto const demos_dir = get_sequences_directory().getChildFile("demos");
    if (!demos_dir.exists() && !demos_dir.createDirectory().wasOk())
    {
        throw std::runtime_error("Unable to create demos directory: " +
                                 demos_dir.getFullPathName().toStdString() + ".");
    }

    for (auto i = 0; i < embed_demos::namedResourceListSize; ++i)
    {
        int size = 0;
        char const *name = embed_demos::namedResourceList[i];
        char const *data = embed_demos::getNamedResource(name, size);
        char const *filename = embed_demos::getNamedResourceOriginalFilename(name);

        auto const file = demos_dir.getChildFile(filename);

        if (!file.existsAsFile())
        {
            if (file.create().wasOk())
            {
                file.appendData(data, (std::size_t)size);
            }
            else
            {
                throw std::runtime_error("Unable to create demo file: " +
                                         file.getFullPathName().toStdString() + ".");
            }
        }
    }
}

} // namespace xen