#include <xen/preferences.hpp>

#include <filesystem>
#include <mutex>
#include <system_error>
#include <utility>

#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>

#include <xen/text_file.hpp>
#include <xen/user_directory.hpp>

namespace
{

auto fnv1a(std::string const &bytes) -> std::uint64_t
{
    auto hash = std::uint64_t{14695981039346656037ULL};
    for (auto const byte : bytes)
    {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

auto revision_for(std::optional<nlohmann::json> const &document) -> std::uint64_t
{
    // Prefixing the serialized value keeps a missing file distinct from a document.
    auto const bytes =
        document.has_value() ? "document:" + document->dump() : std::string{"missing"};
    return fnv1a(bytes);
}

auto mutation_lock_name(std::filesystem::path const &file) -> juce::String
{
    auto error = std::error_code{};
    auto absolute = std::filesystem::absolute(file, error);
    if (error)
    {
        absolute = file;
    }
    auto const path = absolute.lexically_normal().generic_string();
    return "XenSequencerPreferences-" + juce::String{std::to_string(fnv1a(path))};
}

auto mutation_mutex() -> std::mutex &
{
    static auto mutex = std::mutex{};
    return mutex;
}

auto storage_error(xen::PreferencesStorageErrorCode code, std::string const &operation,
                   std::filesystem::path const &file, std::string const &detail)
    -> xen::PreferencesStorageError
{
    return {code, operation + " preferences document " + file.string() + ": " + detail};
}

} // namespace

namespace xen
{

PreferencesStorageError::PreferencesStorageError(PreferencesStorageErrorCode code_in,
                                                 std::string message)
    : std::runtime_error{std::move(message)}, code{code_in}
{
}

PreferencesStore::PreferencesStore(std::filesystem::path file)
    : PreferencesStore{std::move(file), atomic_write_text_file, remove_file}
{
}

PreferencesStore::PreferencesStore(std::filesystem::path file,
                                   AtomicWriter atomic_writer, FileRemover file_remover)
    : file_{std::move(file)}, atomic_writer_{std::move(atomic_writer)},
      file_remover_{std::move(file_remover)}
{
    current_.revision = revision_for(current_.document);
}

auto PreferencesStore::default_file() -> std::filesystem::path
{
    return std::filesystem::path{
               get_user_settings_directory().getFullPathName().toStdString()} /
           "preferences.json";
}

void PreferencesStore::remove_file(std::filesystem::path const &file)
{
    auto error = std::error_code{};
    if (!std::filesystem::remove(file, error) || error)
    {
        throw std::runtime_error{error ? error.message() : "file was not removed"};
    }
}

auto PreferencesStore::load_resource() const -> PreferencesResource
{
    auto status_error = std::error_code{};
    auto const status = std::filesystem::status(file_, status_error);
    if (status_error && status_error != std::errc::no_such_file_or_directory)
    {
        throw storage_error(PreferencesStorageErrorCode::Read, "Unable to inspect",
                            file_, status_error.message());
    }
    if (!status_error && std::filesystem::exists(status) &&
        !std::filesystem::is_regular_file(status))
    {
        throw storage_error(PreferencesStorageErrorCode::Read, "Unable to read", file_,
                            "path is not a regular file");
    }

    auto text = std::optional<std::string>{};
    try
    {
        text = read_text_file(file_);
    }
    catch (std::exception const &error)
    {
        throw storage_error(PreferencesStorageErrorCode::Read, "Unable to read", file_,
                            error.what());
    }

    if (!text.has_value())
    {
        auto error = std::error_code{};
        auto const exists = std::filesystem::exists(file_, error);
        if (error || exists)
        {
            throw storage_error(PreferencesStorageErrorCode::Read, "Unable to read",
                                file_,
                                error ? error.message() : "file is not readable");
        }
        auto resource = PreferencesResource{};
        resource.revision = revision_for(resource.document);
        return resource;
    }
    if (text->size() > MAX_PREFERENCES_DOCUMENT_SIZE)
    {
        throw storage_error(PreferencesStorageErrorCode::MalformedDocument,
                            "Unable to parse", file_, "document exceeds 4 MiB");
    }

    try
    {
        auto document = nlohmann::json::parse(*text);
        if (!document.is_object())
        {
            throw storage_error(PreferencesStorageErrorCode::MalformedDocument,
                                "Unable to parse", file_,
                                "document must be a JSON object");
        }
        // dump() validates that strings can be represented as UTF-8 JSON.
        (void)document.dump();
        auto resource = PreferencesResource{.document = std::move(document)};
        resource.revision = revision_for(resource.document);
        return resource;
    }
    catch (PreferencesStorageError const &)
    {
        throw;
    }
    catch (nlohmann::json::exception const &error)
    {
        throw storage_error(PreferencesStorageErrorCode::MalformedDocument,
                            "Unable to parse", file_, error.what());
    }
}

auto PreferencesStore::refresh() -> bool
{
    auto next = load_resource();
    if (next == current_)
    {
        return false;
    }
    current_ = std::move(next);
    return true;
}

auto PreferencesStore::read() -> PreferencesResource
{
    (void)refresh();
    return current_;
}

void PreferencesStore::require_revision(std::uint64_t expected_revision) const
{
    if (expected_revision != current_.revision)
    {
        throw PreferencesStorageError{
            PreferencesStorageErrorCode::Conflict,
            "Stale preferences revision: expected " +
                std::to_string(expected_revision) + ", current " +
                std::to_string(current_.revision),
        };
    }
}

auto PreferencesStore::write(std::uint64_t expected_revision, nlohmann::json document)
    -> PreferencesResource
{
    if (!document.is_object())
    {
        throw storage_error(PreferencesStorageErrorCode::MalformedDocument,
                            "Unable to serialize", file_,
                            "document must be a JSON object");
    }

    auto serialized = std::string{};
    try
    {
        serialized = document.dump(2);
    }
    catch (nlohmann::json::exception const &error)
    {
        throw storage_error(PreferencesStorageErrorCode::MalformedDocument,
                            "Unable to serialize", file_, error.what());
    }
    if (serialized.size() > MAX_PREFERENCES_DOCUMENT_SIZE)
    {
        throw storage_error(PreferencesStorageErrorCode::MalformedDocument,
                            "Unable to serialize", file_, "document exceeds 4 MiB");
    }

    auto process_lock = std::scoped_lock{mutation_mutex()};
    auto interprocess_lock = juce::InterProcessLock{mutation_lock_name(file_)};
    auto mutation_lock = juce::InterProcessLock::ScopedLockType{interprocess_lock};
    if (!mutation_lock.isLocked())
    {
        throw storage_error(PreferencesStorageErrorCode::Write, "Unable to write",
                            file_, "unable to acquire mutation lock");
    }

    (void)refresh();
    require_revision(expected_revision);
    auto const next = PreferencesResource{
        .revision = revision_for(document),
        .document = std::move(document),
    };
    if (next == current_)
    {
        return current_;
    }

    try
    {
        atomic_writer_(file_, serialized);
    }
    catch (std::exception const &error)
    {
        throw storage_error(PreferencesStorageErrorCode::Write, "Unable to write",
                            file_, error.what());
    }
    current_ = next;
    return current_;
}

auto PreferencesStore::erase(std::uint64_t expected_revision) -> PreferencesResource
{
    auto process_lock = std::scoped_lock{mutation_mutex()};
    auto interprocess_lock = juce::InterProcessLock{mutation_lock_name(file_)};
    auto mutation_lock = juce::InterProcessLock::ScopedLockType{interprocess_lock};
    if (!mutation_lock.isLocked())
    {
        throw storage_error(PreferencesStorageErrorCode::Delete, "Unable to delete",
                            file_, "unable to acquire mutation lock");
    }

    (void)refresh();
    require_revision(expected_revision);
    if (!current_.document.has_value())
    {
        return current_;
    }

    try
    {
        file_remover_(file_);
    }
    catch (std::exception const &error)
    {
        throw storage_error(PreferencesStorageErrorCode::Delete, "Unable to delete",
                            file_, error.what());
    }
    current_ = PreferencesResource{};
    current_.revision = revision_for(current_.document);
    return current_;
}

auto PreferencesStore::current() const -> PreferencesResource const &
{
    return current_;
}

auto PreferencesStore::revision() const noexcept -> std::uint64_t
{
    return current_.revision;
}

auto PreferencesStore::file() const -> std::filesystem::path const &
{
    return file_;
}

} // namespace xen
