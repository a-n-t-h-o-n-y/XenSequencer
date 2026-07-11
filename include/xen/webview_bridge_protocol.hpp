#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include <xen/command_catalog_types.hpp>
#include <xen/keymap.hpp>
#include <xen/selection.hpp>

namespace xen::bridge
{

struct ParsedRequest
{
    std::string name{};
    std::optional<std::string> request_id{};
    nlohmann::json payload = nlohmann::json::object();
};

class BridgeError : public std::runtime_error
{
  public:
    BridgeError(std::string code_in, std::string message_in,
                std::string name_in = "bridge.error",
                std::optional<std::string> request_id_in = std::nullopt);

    std::string code{};
    std::string name{};
    std::optional<std::string> request_id{};
};

[[nodiscard]] auto require_object(nlohmann::json const &json,
                                  std::string_view field_name)
    -> nlohmann::json const &;
[[nodiscard]] auto require_string(nlohmann::json const &json,
                                  std::string_view field_name) -> std::string;
[[nodiscard]] auto require_unsigned(nlohmann::json const &json,
                                    std::string_view field_name) -> std::uint64_t;
[[nodiscard]] auto require_keymap_revision(nlohmann::json const &json,
                                           std::string_view field_name)
    -> std::uint64_t;

[[nodiscard]] auto parse_command_context(nlohmann::json const &payload)
    -> CommandContext;
[[nodiscard]] auto selection_to_json(std::optional<SelectionPath> const &selection)
    -> nlohmann::json;

[[nodiscard]] auto parse_request(std::string const &request_json) -> ParsedRequest;
[[nodiscard]] auto make_envelope(std::string_view type, std::string const &name,
                                 std::optional<std::string> const &request_id,
                                 nlohmann::json payload) -> nlohmann::json;
[[nodiscard]] auto make_error_payload(std::string const &code,
                                      std::string const &message) -> nlohmann::json;

void validate_empty_object_payload(nlohmann::json const &payload,
                                   ParsedRequest const &request);
void validate_session_hello_payload(nlohmann::json const &payload,
                                    ParsedRequest const &request);

} // namespace xen::bridge
