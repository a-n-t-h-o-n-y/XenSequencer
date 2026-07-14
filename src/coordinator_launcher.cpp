#include <xen/coordinator_launcher.hpp>

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include <juce_core/juce_core.h>

#include <xen/text_file.hpp>

namespace xen::ipc
{
namespace
{

[[nodiscard]] auto make_lock_name(SessionId const &session_id) -> juce::String
{
    return "XenSequencerCoordinator-" +
           juce::String{text_revision(session_id).substr(7)};
}

[[nodiscard]] auto dev_helper_path() -> std::filesystem::path
{
#ifdef XEN_COORDINATOR_HELPER_PATH
    return std::filesystem::path{XEN_COORDINATOR_HELPER_PATH};
#else
    return {};
#endif
}

} // namespace

CoordinatorLauncher::CoordinatorLauncher(std::filesystem::path helper_path)
    : explicit_helper_path_{std::move(helper_path)}
{
}

auto CoordinatorLauncher::helper_path() const -> std::filesystem::path
{
    if (!explicit_helper_path_.empty())
    {
        return explicit_helper_path_;
    }
    if (auto path = dev_helper_path(); !path.empty())
    {
        return path;
    }

    auto const current =
        juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto const sibling = current.getSiblingFile("XenSequencerCoordinator"
#if JUCE_WINDOWS
                                                ".exe"
#endif
    );
    return sibling.getFullPathName().toStdString();
}

auto CoordinatorLauncher::ensure_running(SessionId const &session_id,
                                         CoordinatorRegistry const &registry,
                                         juce::ChildProcess *launched_process)
    -> CoordinatorRegistryEntry
{
    auto read_live_entry = [&registry]() -> std::optional<CoordinatorRegistryEntry> {
        auto entry = registry.read();
        if (!entry.has_value())
        {
            return std::nullopt;
        }
        if (CoordinatorRegistry::entry_is_live(*entry))
        {
            return entry;
        }
        registry.clear();
        return std::nullopt;
    };

    if (auto entry = read_live_entry())
    {
        return *entry;
    }

    auto lock = juce::InterProcessLock{make_lock_name(session_id)};
    auto scoped_lock = juce::InterProcessLock::ScopedLockType{lock};
    if (!scoped_lock.isLocked())
    {
        throw std::runtime_error{"Could not acquire coordinator launch lock."};
    }

    if (auto entry = read_live_entry())
    {
        return *entry;
    }

    auto const helper = helper_path();
    if (helper.empty() || !std::filesystem::exists(helper))
    {
        throw std::runtime_error{"XenSequencerCoordinator helper is missing."};
    }

    auto local_process = juce::ChildProcess{};
    auto &process = launched_process == nullptr ? local_process : *launched_process;
    auto arguments = juce::StringArray{};
    arguments.add(juce::String{helper.string()});
    arguments.add("--session-id");
    arguments.add(juce::String{session_id});
    arguments.add("--registry");
    arguments.add(juce::String{registry.path().string()});
    if (!process.start(arguments, juce::ChildProcess::wantStdErr))
    {
        throw std::runtime_error{"Could not launch XenSequencerCoordinator."};
    }

    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (auto entry = read_live_entry())
        {
            return *entry;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{25});
    }

    throw std::runtime_error{"XenSequencerCoordinator did not publish registry."};
}

} // namespace xen::ipc
