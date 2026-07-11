#include <xen/keymap.hpp>

#include <filesystem>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include <xen/text_file.hpp>
#include <xen/user_directory.hpp>

namespace
{

auto revision_for(std::optional<nlohmann::json> const &document) -> std::uint64_t
{
    // FNV-1a is used as a stable opaque content token. Prefixing the serialized
    // value keeps a missing file distinct from a persisted JSON null.
    auto const bytes =
        document.has_value() ? "document:" + document->dump() : std::string{"missing"};
    auto hash = std::uint64_t{14695981039346656037ULL};
    for (auto const byte : bytes)
    {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

auto storage_error(xen::KeymapStorageErrorCode code, std::string const &operation,
                   std::filesystem::path const &file, std::string const &detail)
    -> xen::KeymapStorageError
{
    return {code, operation + " keymap document " + file.string() + ": " + detail};
}

} // namespace

namespace xen
{

KeymapStorageError::KeymapStorageError(KeymapStorageErrorCode code_in,
                                       std::string message)
    : std::runtime_error{std::move(message)}, code{code_in}
{
}

KeymapStore::KeymapStore(std::filesystem::path file)
    : KeymapStore{std::move(file), atomic_write_text_file, remove_file}
{
}

KeymapStore::KeymapStore(std::filesystem::path file, AtomicWriter atomic_writer,
                         FileRemover file_remover)
    : file_{std::move(file)}, atomic_writer_{std::move(atomic_writer)},
      file_remover_{std::move(file_remover)}
{
    current_.revision = revision_for(current_.document);
}

auto KeymapStore::default_file() -> std::filesystem::path
{
    return std::filesystem::path{
               get_user_settings_directory().getFullPathName().toStdString()} /
           "keymap.json";
}

void KeymapStore::remove_file(std::filesystem::path const &file)
{
    auto error = std::error_code{};
    if (!std::filesystem::remove(file, error) || error)
    {
        throw std::runtime_error{error ? error.message() : "file was not removed"};
    }
}

auto KeymapStore::load_resource() const -> KeymapResource
{
    auto status_error = std::error_code{};
    auto const status = std::filesystem::status(file_, status_error);
    if (status_error && status_error != std::errc::no_such_file_or_directory)
    {
        throw storage_error(KeymapStorageErrorCode::Read, "Unable to inspect", file_,
                            status_error.message());
    }
    if (!status_error && std::filesystem::exists(status) &&
        !std::filesystem::is_regular_file(status))
    {
        throw storage_error(KeymapStorageErrorCode::Read, "Unable to read", file_,
                            "path is not a regular file");
    }

    auto text = std::optional<std::string>{};
    try
    {
        text = read_text_file(file_);
    }
    catch (std::exception const &error)
    {
        throw storage_error(KeymapStorageErrorCode::Read, "Unable to read", file_,
                            error.what());
    }

    if (!text.has_value())
    {
        auto error = std::error_code{};
        auto const exists = std::filesystem::exists(file_, error);
        if (error || exists)
        {
            throw storage_error(KeymapStorageErrorCode::Read, "Unable to read", file_,
                                error ? error.message() : "file is not readable");
        }
        auto resource = KeymapResource{};
        resource.revision = revision_for(resource.document);
        return resource;
    }
    if (text->size() > MAX_KEYMAP_DOCUMENT_SIZE)
    {
        throw storage_error(KeymapStorageErrorCode::MalformedDocument,
                            "Unable to parse", file_, "document exceeds 4 MiB");
    }

    try
    {
        auto resource = KeymapResource{
            .document = nlohmann::json::parse(*text),
        };
        // dump() validates that strings can be represented as UTF-8 JSON.
        (void)resource.document->dump();
        resource.revision = revision_for(resource.document);
        return resource;
    }
    catch (nlohmann::json::exception const &error)
    {
        throw storage_error(KeymapStorageErrorCode::MalformedDocument,
                            "Unable to parse", file_, error.what());
    }
}

auto KeymapStore::refresh() -> bool
{
    auto next = load_resource();
    if (next == current_)
    {
        return false;
    }
    current_ = std::move(next);
    return true;
}

auto KeymapStore::read() -> KeymapResource
{
    (void)refresh();
    return current_;
}

void KeymapStore::require_revision(std::uint64_t expected_revision) const
{
    if (expected_revision != current_.revision)
    {
        throw KeymapStorageError{
            KeymapStorageErrorCode::Conflict,
            "Stale keymap revision: expected " + std::to_string(expected_revision) +
                ", current " + std::to_string(current_.revision),
        };
    }
}

auto KeymapStore::write(std::uint64_t expected_revision, nlohmann::json document)
    -> KeymapResource
{
    (void)refresh();
    require_revision(expected_revision);

    auto serialized = std::string{};
    try
    {
        serialized = document.dump(2);
    }
    catch (nlohmann::json::exception const &error)
    {
        throw storage_error(KeymapStorageErrorCode::MalformedDocument,
                            "Unable to serialize", file_, error.what());
    }
    if (serialized.size() > MAX_KEYMAP_DOCUMENT_SIZE)
    {
        throw storage_error(KeymapStorageErrorCode::MalformedDocument,
                            "Unable to serialize", file_, "document exceeds 4 MiB");
    }

    auto const next = KeymapResource{
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
        throw storage_error(KeymapStorageErrorCode::Write, "Unable to write", file_,
                            error.what());
    }
    current_ = next;
    return current_;
}

auto KeymapStore::erase(std::uint64_t expected_revision) -> KeymapResource
{
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
        throw storage_error(KeymapStorageErrorCode::Delete, "Unable to delete", file_,
                            error.what());
    }
    current_ = KeymapResource{};
    current_.revision = revision_for(current_.document);
    return current_;
}

auto KeymapStore::current() const -> KeymapResource const &
{
    return current_;
}

auto KeymapStore::revision() const noexcept -> std::uint64_t
{
    return current_.revision;
}

auto KeymapStore::file() const -> std::filesystem::path const &
{
    return file_;
}

} // namespace xen
