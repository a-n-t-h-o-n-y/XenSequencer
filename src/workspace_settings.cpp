#include <xen/workspace_settings.hpp>

#include <filesystem>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include <xen/project_validation.hpp>
#include <xen/text_file.hpp>
#include <xen/user_directory.hpp>

namespace xen
{

namespace
{

auto default_workspace_settings() -> WorkspaceSettings
{
    return {
        .sequence_directory =
            std::filesystem::path{
                get_sequences_directory().getFullPathName().toStdString()},
        .tuning_directory =
            std::filesystem::path{
                get_tunings_directory().getFullPathName().toStdString()},
    };
}

} // namespace

WorkspaceSettingsStore::WorkspaceSettingsStore(std::filesystem::path file)
    : file_{std::move(file)}
{
}

auto WorkspaceSettingsStore::default_file() -> std::filesystem::path
{
    return std::filesystem::path{
               get_user_library_directory().getFullPathName().toStdString()} /
           "settings" / "workspace.json";
}

auto WorkspaceSettingsStore::load_or_initialize() const -> WorkspaceSettings
{
    auto const text = read_text_file(file_);
    if (!text.has_value())
    {
        auto settings = default_workspace_settings();
        validate(settings);
        return settings;
    }

    try
    {
        auto const json = nlohmann::json::parse(*text);
        if (json.at("schema").get<int>() != 1)
        {
            throw std::invalid_argument{"Unsupported workspace settings schema."};
        }
        auto settings = WorkspaceSettings{
            .sequence_directory =
                std::filesystem::path{json.at("sequence_directory").get<std::string>()},
            .tuning_directory =
                std::filesystem::path{json.at("tuning_directory").get<std::string>()},
        };
        validate(settings);
        return settings;
    }
    catch (std::exception const &error)
    {
        throw std::runtime_error{"Unable to load workspace settings " + file_.string() +
                                 ": " + error.what()};
    }
}

void WorkspaceSettingsStore::save(WorkspaceSettings const &settings) const
{
    validate(settings);

    auto const json = nlohmann::json{
        {"schema", 1},
        {"sequence_directory", settings.sequence_directory.string()},
        {"tuning_directory", settings.tuning_directory.string()},
    };
    atomic_write_text_file(file_, json.dump(2));
}

auto WorkspaceSettingsStore::file() const -> std::filesystem::path const &
{
    return file_;
}

} // namespace xen
