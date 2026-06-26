#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>

#include <xen/command.hpp>
#include <xen/command_catalog_types.hpp>
#include <xen/keymap.hpp>
#include <xen/sequencer_session_port.hpp>
#include <xen/state.hpp>
#include <xen/webview_bridge_protocol.hpp>

namespace xen::bridge
{

class ApplicationBridgeService
{
  public:
    virtual ~ApplicationBridgeService() = default;

    [[nodiscard]] virtual auto project_snapshot() const -> ProjectSnapshot = 0;
    [[nodiscard]] virtual auto instance_binding() const -> InstanceBinding = 0;
    [[nodiscard]] virtual auto library_snapshot() const -> LibrarySnapshot = 0;
    [[nodiscard]] virtual auto command_catalog_metadata() const
        -> std::vector<CatalogCommandMetadata> = 0;
    [[nodiscard]] virtual auto execute_command_string(std::string const &command,
                                                      CommandContext const &context)
        -> CommandApplicationResult = 0;
    virtual void set_channel_id(ChannelId channel_id) = 0;
};

class KeymapBridgeService
{
  public:
    virtual ~KeymapBridgeService() = default;

    [[nodiscard]] virtual auto snapshot() const -> KeymapSnapshot = 0;
    [[nodiscard]] virtual auto revision() const noexcept -> std::uint64_t = 0;
    virtual auto set_override(std::uint64_t expected_revision, std::string context,
                              KeymapTrigger trigger, std::optional<KeymapTarget> target)
        -> KeymapSnapshot = 0;
    virtual auto remove_override(std::uint64_t expected_revision,
                                 std::string const &context,
                                 KeymapTrigger const &trigger) -> KeymapSnapshot = 0;
    virtual auto reset(std::uint64_t expected_revision) -> KeymapSnapshot = 0;
};

class LibraryBridgeService
{
  public:
    virtual ~LibraryBridgeService() = default;

    [[nodiscard]] virtual auto make_payload(LibrarySnapshot const &snapshot) const
        -> nlohmann::json = 0;
};

struct LibraryFileEntry
{
    std::string name{};
    std::string relative_path{};
    std::string stem{};
    std::string path{};
};

struct LibraryTuningEntry
{
    LibraryFileEntry file{};
    std::string description{};
    std::vector<float> intervals{};
    float octave{};
};

class LibraryFilePort
{
  public:
    virtual ~LibraryFilePort() = default;

    [[nodiscard]] virtual auto library_root() const -> std::filesystem::path = 0;
    [[nodiscard]] virtual auto measure_files(std::filesystem::path const &directory)
        const -> std::vector<LibraryFileEntry> = 0;
    [[nodiscard]] virtual auto tuning_files(std::filesystem::path const &directory)
        const -> std::vector<LibraryTuningEntry> = 0;
};

class SequencerApplicationBridgeService final : public ApplicationBridgeService
{
  public:
    explicit SequencerApplicationBridgeService(SequencerSessionPort &session);

    [[nodiscard]] auto project_snapshot() const -> ProjectSnapshot override;
    [[nodiscard]] auto instance_binding() const -> InstanceBinding override;
    [[nodiscard]] auto library_snapshot() const -> LibrarySnapshot override;
    [[nodiscard]] auto command_catalog_metadata() const
        -> std::vector<CatalogCommandMetadata> override;
    [[nodiscard]] auto execute_command_string(std::string const &command,
                                              CommandContext const &context)
        -> CommandApplicationResult override;
    void set_channel_id(ChannelId channel_id) override;

  private:
    SequencerSessionPort &session_;
};

class StoreKeymapBridgeService final : public KeymapBridgeService
{
  public:
    explicit StoreKeymapBridgeService(
        std::filesystem::path keymap_file = KeymapStore::default_file());

    [[nodiscard]] auto snapshot() const -> KeymapSnapshot override;
    [[nodiscard]] auto revision() const noexcept -> std::uint64_t override;
    auto set_override(std::uint64_t expected_revision, std::string context,
                      KeymapTrigger trigger, std::optional<KeymapTarget> target)
        -> KeymapSnapshot override;
    auto remove_override(std::uint64_t expected_revision, std::string const &context,
                         KeymapTrigger const &trigger) -> KeymapSnapshot override;
    auto reset(std::uint64_t expected_revision) -> KeymapSnapshot override;

  private:
    KeymapStore store_;
};

class JuceLibraryBridgeService final : public LibraryBridgeService
{
  public:
    explicit JuceLibraryBridgeService(LibraryFilePort &files);

    [[nodiscard]] auto make_payload(LibrarySnapshot const &snapshot) const
        -> nlohmann::json override;

  private:
    LibraryFilePort &files_;
};

class JuceLibraryFilePort final : public LibraryFilePort
{
  public:
    [[nodiscard]] auto library_root() const -> std::filesystem::path override;
    [[nodiscard]] auto measure_files(std::filesystem::path const &directory) const
        -> std::vector<LibraryFileEntry> override;
    [[nodiscard]] auto tuning_files(std::filesystem::path const &directory) const
        -> std::vector<LibraryTuningEntry> override;
};

class BridgeRequestDispatcher
{
  public:
    BridgeRequestDispatcher(ApplicationBridgeService &application,
                            LibraryBridgeService &library, KeymapBridgeService &keymap);

    [[nodiscard]] auto handle_request_json(std::string const &request_json)
        -> std::string;

  private:
    using Handler = std::function<nlohmann::json(ParsedRequest const &)>;

    ApplicationBridgeService &application_;
    LibraryBridgeService &library_;
    KeymapBridgeService &keymap_;
    std::map<std::string_view, Handler, std::less<>> handlers_;

    [[nodiscard]] auto handle_session_hello(ParsedRequest const &request)
        -> nlohmann::json;
    [[nodiscard]] auto handle_state_get(ParsedRequest const &request) -> nlohmann::json;
    [[nodiscard]] auto handle_session_binding_get(ParsedRequest const &request)
        -> nlohmann::json;
    [[nodiscard]] auto handle_session_binding_set(ParsedRequest const &request)
        -> nlohmann::json;
    [[nodiscard]] auto handle_command_execute(ParsedRequest const &request)
        -> nlohmann::json;
    [[nodiscard]] auto handle_library_get(ParsedRequest const &request)
        -> nlohmann::json;
    [[nodiscard]] auto handle_keymap_get(ParsedRequest const &request)
        -> nlohmann::json;
    [[nodiscard]] auto handle_keymap_override_set(ParsedRequest const &request)
        -> nlohmann::json;
    [[nodiscard]] auto handle_keymap_override_remove(ParsedRequest const &request)
        -> nlohmann::json;
    [[nodiscard]] auto handle_keymap_reset(ParsedRequest const &request)
        -> nlohmann::json;
};

[[nodiscard]] auto quote_command_arg(std::string const &value) -> std::string;
[[nodiscard]] auto normalize_utf8(std::string_view input) -> std::string;

} // namespace xen::bridge
