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
                          [](MeasureRequest const &x) {
                              return nlohmann::json{
                                  {"type", "MeasureRequest"},
                                  {"measure_index", x.measure_index},
                                  {"reply_to", x.reply_to.toString().toStdString()},
                              }
                                  .dump();
                          },
                          [](MeasureResponse const &x) {
                              return nlohmann::json{
                                  {"type", "MeasureResponse"},
                                  {"measure", serialize_measure(x.measure)},
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
    else if (type == "MeasureRequest")
    {
        return MeasureRequest{
            .measure_index = j.at("measure_index").get<std::size_t>(),
            .reply_to = juce::Uuid{j.at("reply_to").get<std::string>()},
        };
    }
    else if (type == "MeasureResponse")
    {
        return MeasureResponse{
            deserialize_measure(j.at("measure").get<std::string>()),
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