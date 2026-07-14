#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <xen/command.hpp>
#include <xen/command_catalog_types.hpp>
#include <xen/document.hpp>
#include <xen/state.hpp>

namespace xen
{

class SequencerSessionPort
{
  public:
    virtual ~SequencerSessionPort() = default;

    [[nodiscard]] virtual auto project_snapshot() const -> ProjectSnapshot = 0;
    [[nodiscard]] virtual auto persistent_project_snapshot() const
        -> ProjectSnapshot = 0;
    [[nodiscard]] virtual auto library_snapshot() const -> LibrarySnapshot = 0;
    [[nodiscard]] virtual auto instance_binding() const -> InstanceBinding = 0;
    [[nodiscard]] virtual auto command_catalog_metadata() const
        -> std::vector<CatalogCommandMetadata> = 0;

    [[nodiscard]] virtual auto execute_command_string(std::string const &command_string,
                                                      CommandContext const &context)
        -> CommandApplicationResult = 0;

    [[nodiscard]] virtual auto begin_preview(ProjectRevision expected_revision)
        -> PreviewControlResult = 0;
    [[nodiscard]] virtual auto commit_preview(PreviewId const &preview_id,
                                              ProjectRevision expected_revision)
        -> PreviewControlResult = 0;
    [[nodiscard]] virtual auto cancel_preview(PreviewId const &preview_id,
                                              ProjectRevision expected_revision)
        -> PreviewControlResult = 0;

    [[nodiscard]] virtual auto create_project(ProjectRevision expected_revision,
                                              bool discard_unsaved)
        -> DocumentOperationResult = 0;
    [[nodiscard]] virtual auto open_project(std::string relative_path,
                                            ProjectRevision expected_revision,
                                            bool discard_unsaved)
        -> DocumentOperationResult = 0;
    [[nodiscard]] virtual auto save_project(ProjectRevision expected_revision)
        -> DocumentOperationResult = 0;
    [[nodiscard]] virtual auto save_project_as(
        std::string relative_path, ProjectRevision expected_revision,
        std::optional<std::string> expected_file_revision)
        -> DocumentOperationResult = 0;
    [[nodiscard]] virtual auto restore_recovery(std::string recovery_revision,
                                                ProjectRevision expected_revision,
                                                bool discard_unsaved)
        -> DocumentOperationResult = 0;
    [[nodiscard]] virtual auto discard_recovery(std::string recovery_revision)
        -> DocumentOperationResult = 0;
    [[nodiscard]] virtual auto import_cell(std::string relative_path,
                                           ProjectRevision expected_revision,
                                           CompositionCursor cursor)
        -> DocumentOperationResult = 0;
    [[nodiscard]] virtual auto save_cell(
        std::string relative_path, ProjectRevision expected_revision,
        CompositionCursor cursor, SelectionPath selection,
        std::optional<std::string> expected_file_revision)
        -> DocumentOperationResult = 0;

    virtual void set_channel_id(ChannelId channel_id) = 0;
};

} // namespace xen
