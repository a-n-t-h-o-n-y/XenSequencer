#include <xen/workspace_settings.hpp>

#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include <xen/project_validation.hpp>
#include <xen/user_directory.hpp>

namespace xen
{

WorkspaceSettingsStore::WorkspaceSettingsStore(juce::File file) : file_{std::move(file)}
{
}

auto WorkspaceSettingsStore::default_file() -> juce::File
{
    return get_user_library_directory().getChildFile("workspace-settings.json");
}

auto WorkspaceSettingsStore::load_or_initialize() const -> WorkspaceSettings
{
    if (!file_.existsAsFile())
    {
        auto settings = WorkspaceSettings{};
        validate(settings);
        return settings;
    }

    try
    {
        auto const json = nlohmann::json::parse(file_.loadFileAsString().toStdString());
        if (json.at("schema").get<int>() != 1)
        {
            throw std::invalid_argument{"Unsupported workspace settings schema."};
        }
        auto settings = WorkspaceSettings{
            .sequence_directory =
                juce::File{json.at("sequence_directory").get<std::string>()},
            .tuning_directory =
                juce::File{json.at("tuning_directory").get<std::string>()},
        };
        validate(settings);
        return settings;
    }
    catch (std::exception const &error)
    {
        throw std::runtime_error{"Unable to load workspace settings " +
                                 file_.getFullPathName().toStdString() + ": " +
                                 error.what()};
    }
}

void WorkspaceSettingsStore::save(WorkspaceSettings const &settings) const
{
    validate(settings);
    auto const parent = file_.getParentDirectory();
    if (!parent.isDirectory() && !parent.createDirectory().wasOk())
    {
        throw std::runtime_error{"Unable to create workspace settings directory."};
    }

    auto const json = nlohmann::json{
        {"schema", 1},
        {"sequence_directory",
         settings.sequence_directory.getFullPathName().toStdString()},
        {"tuning_directory", settings.tuning_directory.getFullPathName().toStdString()},
    };
    if (!file_.replaceWithText(json.dump(2)))
    {
        throw std::runtime_error{"Unable to persist workspace settings: " +
                                 file_.getFullPathName().toStdString()};
    }
}

auto WorkspaceSettingsStore::file() const -> juce::File const &
{
    return file_;
}

} // namespace xen
