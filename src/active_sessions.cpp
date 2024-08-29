#include <xen/active_sessions.hpp>

#include <string>
#include <variant>

#include <juce_core/juce_core.h>

#include <nlohmann/json.hpp>

#include <sequence/measure.hpp>
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

// -------------------------------------------------------------------------------------

HeartbeatSender::HeartbeatSender(InstanceDirectory &directory, juce::Uuid const &uuid)
    : directory_{directory}, uuid_{uuid}
{
    this->startTimer(PERIOD);
}

HeartbeatSender::~HeartbeatSender()
{
    this->stopTimer();
}

void HeartbeatSender::timerCallback()
{
    try
    {
        directory_.send_heartbeat(uuid_);
    }
    catch (std::exception const &e)
    {
        std::cerr << "Exception in HeartbeatSender:\n" << e.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "Unknown exception in HeartbeatSender\n";
    }
}

// -------------------------------------------------------------------------------------

DeadSessionTrimmer::DeadSessionTrimmer(InstanceDirectory &directory)
    : directory_{directory}
{
    this->timerCallback();
    this->startTimer(PERIOD);
}

DeadSessionTrimmer::~DeadSessionTrimmer()
{
    this->stopTimer();
}

void DeadSessionTrimmer::timerCallback()
{
    try
    {
        directory_.unregister_dead_instances(
            std::chrono::milliseconds{HeartbeatSender::PERIOD} * 4);
    }
    catch (std::exception const &e)
    {
        std::cerr << "Exception in DeadSessionTimer:\n" << e.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "Unknown exception in DeadSessionTimer\n";
    }
}

// -------------------------------------------------------------------------------------

ThisInstance::ThisInstance(InterProcessRelay &relay, InstanceDirectory &directory,
                           juce::Uuid const &uuid, std::string const &display_name)
    : relay_{relay}, directory_{directory}, uuid_{uuid},
      heartbeat_sender_{directory, uuid}
{
    // Get list of other instances and add itself to instance directory in a single
    // 'atomic' step.
    auto const others = [&] {
        auto const lock = std::scoped_lock{directory_.get_mutex()};
        auto const x = directory_.get_active_instances();
        directory_.register_instance(uuid_);
        return x;
    }();

    for (auto const &other : others)
    {
        try
        {
            relay_.send_to(other, serialize(IDUpdate{.uuid = uuid_,
                                                     .display_name = display_name}));
        }
        catch (std::exception const &e)
        {
            // TODO logging
            std::cerr << "Could not send initialization message to other instance ("
                      << other.toString().toStdString() << "):\n"
                      << e.what() << '\n'
                      << "skipping...\n";
        }
        catch (...)
        {
            std::cerr << "Unknown exception in ThisInstance\n";
        }
    }
}

ThisInstance::~ThisInstance()
{
    try
    {
        auto const others = [&] {
            auto const lock = std::scoped_lock{directory_.get_mutex()};
            directory_.unregister_instance(uuid_);
            return directory_.get_active_instances();
        }();

        for (auto const &other : directory_.get_active_instances())
        {
            relay_.send_to(other, serialize(InstanceShutdown{.uuid = uuid_}));
        }
    }
    catch (std::exception const &e)
    {
        std::cerr << "Exception in ~ThisInstance:\n" << e.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "Unknown exception in ~ThisInstance\n";
    }
}

auto ThisInstance::get_uuid() const -> juce::Uuid const &
{
    return uuid_;
}

// -------------------------------------------------------------------------------------

ActiveSessions::ActiveSessions(juce::Uuid const &current_process_id,
                               std::string const &display_name)
    : relay_{current_process_id}, instance_directory_{},
      this_instance_{relay_, instance_directory_, current_process_id, display_name},
      dead_session_trimmer_{instance_directory_}
{
    relay_.on_message.connect([this](std::string const &json) {
        std::visit(
            sequence::utility::overload{
                [this](InstanceShutdown const &x) { on_instance_shutdown(x.uuid); },
                [this](IDUpdate const &x) { on_id_update(x.uuid, x.display_name); },
                [this](MeasureRequest const &x) {
                    auto measure = on_measure_request(x.measure_index);
                    if (!measure.has_value())
                    {
                        throw std::logic_error{"on_measure_request(index) returned "
                                               "std::nullopt, no Slot connected"};
                    }
                    relay_.send_to(x.reply_to, serialize(MeasureResponse{
                                                   .measure = std::move(*measure),
                                               }));
                },
                [this](MeasureResponse const &x) { on_measure_response(x.measure); },
                [this](DisplayNameRequest const &x) {
                    auto name = on_display_name_request();
                    if (!name.has_value())
                    {
                        throw std::logic_error{"on_display_name_request() returned "
                                               "std::nullopt, no Slot connected"};
                    }
                    relay_.send_to(x.reply_to, serialize(IDUpdate{
                                                   .uuid = this_instance_.get_uuid(),
                                                   .display_name = std::move(*name)}));
                },
            },
            deserialize(json));
    });
}

void ActiveSessions::request_other_session_ids() const
{
    auto const instances = instance_directory_.get_active_instances();

    for (auto const &instance : instances)
    {
        if (instance != this_instance_.get_uuid())
        {
            try
            {
                relay_.send_to(instance, serialize(DisplayNameRequest{
                                             .reply_to = this_instance_.get_uuid()}));
            }
            catch (std::exception const &e)
            {
                // TODO change this to use Logging with timestamp.
                std::cerr << "Could not send display name request to other instance ("
                          << instance.toString().toStdString() << "):\n"
                          << e.what() << '\n'
                          << "skipping...\n";
            }
            catch (...)
            {
                std::cerr << "Unknown exception in request_other_session_ids\n";
            }
        }
    }
}

void ActiveSessions::request_measure(juce::Uuid const &uuid, std::size_t index) const
{
    relay_.send_to(uuid, serialize(MeasureRequest{
                             .measure_index = index,
                             .reply_to = this_instance_.get_uuid(),
                         }));
}

void ActiveSessions::notify_display_name_update(std::string const &name)
{
    auto const instances = instance_directory_.get_active_instances();
    for (auto const &instance : instances)
    {
        if (instance != this_instance_.get_uuid())
        {
            relay_.send_to(instance,
                           serialize(IDUpdate{.uuid = this_instance_.get_uuid(),
                                              .display_name = name}));
        }
    }
}

} // namespace xen