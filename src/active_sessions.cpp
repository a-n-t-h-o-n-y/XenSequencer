#include <xen/active_sessions.hpp>

#include <string>
#include <variant>

#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>

#include <sequence/utility.hpp>

#include <xen/serialize.hpp>

namespace xen
{

auto serialize(Message const &m) -> std::string
{
    return std::visit(sequence::utility::overload{
                          [](InstanceShutdown const &x) {
                              return nlohmann::json{
                                  {"type", "InstanceShutdown"},
                                  {"uuid", x.uuid.toString().toStdString()},
                              }
                                  .dump();
                          },
                          [](IDUpdate const &x) {
                              return nlohmann::json{
                                  {"type", "IDUpdate"},
                                  {"uuid", x.uuid.toString().toStdString()},
                                  {"display_name", x.display_name},
                              }
                                  .dump();
                          },
                          [](StateRequest const &x) {
                              return nlohmann::json{
                                  {"type", "StateRequest"},
                                  {"reply_to", x.reply_to.toString().toStdString()},
                              }
                                  .dump();
                          },
                          [](StateResponse const &x) {
                              return nlohmann::json{
                                  {"type", "StateResponse"},
                                  {"state", serialize_state(x.state)},
                              }
                                  .dump();
                          },
                          [](DisplayNameRequest const &x) {
                              return nlohmann::json{
                                  {"type", "DisplayNameRequest"},
                                  {"reply_to", x.reply_to.toString().toStdString()},
                              }
                                  .dump();
                          },
                      },
                      m);
}

auto deserialize(std::string const &x) -> Message
{
    auto const j = nlohmann::json::parse(x);
    auto const type = j.at("type").get<std::string>();
    if (type == "InstanceShutdown")
    {
        return InstanceShutdown{
            juce::Uuid{j.at("uuid").get<std::string>()},
        };
    }
    else if (type == "IDUpdate")
    {
        return IDUpdate{
            juce::Uuid{j.at("uuid").get<std::string>()},
            j.at("display_name").get<std::string>(),
        };
    }
    else if (type == "StateRequest")
    {
        return StateRequest{
            juce::Uuid{j.at("reply_to").get<std::string>()},
        };
    }
    else if (type == "StateResponse")
    {
        return StateResponse{
            deserialize_state(j.at("state").get<std::string>()),
        };
    }
    else if (type == "DisplayNameRequest")
    {
        return DisplayNameRequest{
            juce::Uuid{j.at("reply_to").get<std::string>()},
        };
    }
    else
    {
        throw std::runtime_error{"Invalid message type: " + type};
    }
}

} // namespace xen