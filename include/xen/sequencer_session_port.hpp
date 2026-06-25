#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <xen/command.hpp>
#include <xen/command_catalog_types.hpp>
#include <xen/engine_state_mailbox.hpp>
#include <xen/state.hpp>

namespace xen
{

class SequencerSessionPort
{
  public:
    virtual ~SequencerSessionPort() = default;

    [[nodiscard]] virtual auto project_snapshot() const -> ProjectSnapshot = 0;
    [[nodiscard]] virtual auto library_snapshot() const -> LibrarySnapshot = 0;
    [[nodiscard]] virtual auto instance_binding() const -> InstanceBinding const & = 0;
    [[nodiscard]] virtual auto command_catalog_metadata() const
        -> std::vector<CatalogCommandMetadata> = 0;

    [[nodiscard]] virtual auto execute_command_string(std::string const &command_string,
                                                      CommandContext const &context)
        -> CommandApplicationResult = 0;

    virtual void set_output_id(OutputId output_id) = 0;

    [[nodiscard]] virtual auto audio_project_update_version() const noexcept
        -> std::uint64_t = 0;
    [[nodiscard]] virtual auto try_consume_audio_project_update() noexcept
        -> std::optional<EngineStateMailbox::ReadView> = 0;
};

} // namespace xen
