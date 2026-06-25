#include <xen/coordinator_registry.hpp>

#include <cerrno>
#include <fstream>
#include <stdexcept>
#include <utility>

#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>

#if JUCE_LINUX || JUCE_BSD
#include <csignal>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace xen::ipc
{
namespace
{

[[nodiscard]] auto entry_from_json(nlohmann::json const &json)
    -> CoordinatorRegistryEntry
{
    auto entry = CoordinatorRegistryEntry{
        .session_id = json.at("session_id").get<SessionId>(),
        .port = json.at("port").get<int>(),
        .pid = json.at("pid").get<std::uint32_t>(),
        .nonce = json.at("nonce").get<std::string>(),
        .protocol_version = json.at("protocol_version").get<int>(),
        .helper_version = json.at("helper_version").get<std::string>(),
    };
    if (entry.session_id.empty() || entry.port <= 0 || entry.nonce.empty() ||
        entry.protocol_version != coordinator_protocol_version ||
        entry.helper_version != coordinator_helper_version)
    {
        throw std::invalid_argument{"Invalid coordinator registry entry."};
    }
    return entry;
}

[[nodiscard]] auto entry_to_json(CoordinatorRegistryEntry const &entry)
    -> nlohmann::json
{
    if (entry.session_id.empty() || entry.port <= 0 || entry.nonce.empty())
    {
        throw std::invalid_argument{"Invalid coordinator registry entry."};
    }
    return {
        {"session_id", entry.session_id},
        {"port", entry.port},
        {"pid", entry.pid},
        {"nonce", entry.nonce},
        {"protocol_version", entry.protocol_version},
        {"helper_version", entry.helper_version},
    };
}

[[nodiscard]] auto command_line_for_pid(std::uint32_t pid) -> std::string
{
#if JUCE_LINUX || JUCE_BSD
    auto input =
        std::ifstream{"/proc/" + std::to_string(pid) + "/cmdline", std::ios::binary};
    if (!input)
    {
        return {};
    }

    auto data = std::ostringstream{};
    data << input.rdbuf();
    auto command_line = data.str();
    for (auto &ch : command_line)
    {
        if (ch == '\0')
        {
            ch = ' ';
        }
    }
    return command_line;
#else
    (void)pid;
    return {};
#endif
}

} // namespace

CoordinatorRegistry::CoordinatorRegistry(std::filesystem::path registry_file)
    : registry_file_{std::move(registry_file)}
{
}

auto CoordinatorRegistry::path() const -> std::filesystem::path const &
{
    return registry_file_;
}

auto CoordinatorRegistry::read() const -> std::optional<CoordinatorRegistryEntry>
{
    if (!std::filesystem::exists(registry_file_))
    {
        return std::nullopt;
    }

    auto input = std::ifstream{registry_file_};
    if (!input)
    {
        return std::nullopt;
    }

    try
    {
        return entry_from_json(nlohmann::json::parse(input));
    }
    catch (...)
    {
        clear();
        return std::nullopt;
    }
}

void CoordinatorRegistry::write(CoordinatorRegistryEntry const &entry) const
{
    std::filesystem::create_directories(registry_file_.parent_path());
    auto output = std::ofstream{registry_file_, std::ios::trunc};
    if (!output)
    {
        throw std::runtime_error{"Could not write coordinator registry."};
    }
    output << entry_to_json(entry).dump();
}

void CoordinatorRegistry::clear() const
{
    auto ignored = std::error_code{};
    std::filesystem::remove(registry_file_, ignored);
}

auto CoordinatorRegistry::default_file(SessionId const &session_id)
    -> std::filesystem::path
{
    if (session_id.empty())
    {
        throw std::invalid_argument{"Session ID must not be empty."};
    }
    auto file = juce::File{default_directory().string()}.getChildFile(
        "coordinator-" + juce::String{session_id} + ".json");
    return file.getFullPathName().toStdString();
}

auto CoordinatorRegistry::default_directory() -> std::filesystem::path
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("XenSequencer")
        .getFullPathName()
        .toStdString();
}

auto CoordinatorRegistry::active_entries() -> std::vector<CoordinatorRegistryEntry>
{
    auto result = std::vector<CoordinatorRegistryEntry>{};
    auto const directory = default_directory();
    if (!std::filesystem::exists(directory))
    {
        return result;
    }

    for (auto const &entry : std::filesystem::directory_iterator{directory})
    {
        if (!entry.is_regular_file() ||
            entry.path().filename().string().rfind("coordinator-", 0) != 0 ||
            entry.path().extension() != ".json")
        {
            continue;
        }

        auto registry = CoordinatorRegistry{entry.path()};
        auto registry_entry = registry.read();
        if (!registry_entry.has_value())
        {
            continue;
        }

        if (entry_is_live(*registry_entry))
        {
            result.push_back(*registry_entry);
        }
        else
        {
            registry.clear();
        }
    }
    return result;
}

auto CoordinatorRegistry::current_process_id() -> std::uint32_t
{
#if JUCE_LINUX || JUCE_BSD
    return static_cast<std::uint32_t>(::getpid());
#else
    return 0;
#endif
}

auto CoordinatorRegistry::entry_is_live(CoordinatorRegistryEntry const &entry) -> bool
{
    if (entry.pid == 0)
    {
        return false;
    }

#if JUCE_LINUX || JUCE_BSD
    errno = 0;
    if (::kill(static_cast<pid_t>(entry.pid), 0) != 0 && errno == ESRCH)
    {
        return false;
    }

    auto const command_line = command_line_for_pid(entry.pid);
    return command_line.find("XenSequencerCoordinator") != std::string::npos &&
           command_line.find("--session-id " + entry.session_id) != std::string::npos;
#else
    return false;
#endif
}

} // namespace xen::ipc
