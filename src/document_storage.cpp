#include <xen/document_storage.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <ranges>
#include <set>
#include <string>
#include <system_error>

#include <juce_core/juce_core.h>

#include <xen/serialize.hpp>
#include <xen/text_file.hpp>

namespace xen
{
namespace
{

auto lower_ascii(std::string value) -> std::string
{
    std::ranges::transform(value, value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

auto is_reserved_segment(std::string const &segment) -> bool
{
    static auto const reserved = std::set<std::string>{
        "con",  "prn",  "aux",  "nul",  "com1", "com2", "com3", "com4",
        "com5", "com6", "com7", "com8", "com9", "lpt1", "lpt2", "lpt3",
        "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9",
    };
    auto name = lower_ascii(segment);
    if (auto const dot = name.find('.'); dot != std::string::npos)
    {
        name.erase(dot);
    }
    return reserved.contains(name);
}

void validate_segment(std::string const &segment)
{
    if (segment.empty() || segment.size() > 200 || segment == "." || segment == ".." ||
        segment.back() == '.' || segment.back() == ' ' || is_reserved_segment(segment))
    {
        throw DocumentError{DocumentErrorCode::InvalidPath,
                            "Content path contains an invalid filename segment."};
    }
    for (auto const character : segment)
    {
        auto const byte = static_cast<unsigned char>(character);
        if (byte < 0x20 || byte == 0x7f || character == '<' || character == '>' ||
            character == ':' || character == '"' || character == '\\' ||
            character == '|' || character == '?' || character == '*')
        {
            throw DocumentError{DocumentErrorCode::InvalidPath,
                                "Content path contains a non-portable character."};
        }
    }
}

auto is_within(std::filesystem::path const &root,
               std::filesystem::path const &candidate) -> bool
{
    auto root_at = root.begin();
    auto candidate_at = candidate.begin();
    while (root_at != root.end() && candidate_at != candidate.end())
    {
        if (*root_at != *candidate_at)
        {
            return false;
        }
        ++root_at;
        ++candidate_at;
    }
    return root_at == root.end();
}

auto make_file_info(std::string const &relative_path, std::string revision)
    -> ContentFileInfo
{
    auto const path = std::filesystem::path{relative_path};
    auto stem = path;
    stem.replace_extension();
    return ContentFileInfo{
        .name = path.filename().string(),
        .relative_path = path.generic_string(),
        .stem = stem.generic_string(),
        .file_revision = std::move(revision),
    };
}

auto lock_name(std::filesystem::path const &path) -> std::string
{
    return "xen-document-" +
           text_revision(lower_ascii(path.generic_string())).substr(7);
}

auto maximum_file_bytes(juce::File const &destination) -> std::size_t
{
    return destination.hasFileExtension(".xencell") ? MAX_CELL_FILE_BYTES
                                                    : MAX_PROJECT_FILE_BYTES;
}

auto valid_interrupted_document(juce::File const &artifact,
                                juce::File const &destination) -> bool
{
    try
    {
        auto const text = read_text_file(artifact.getFullPathName().toStdString(),
                                         maximum_file_bytes(destination));
        if (!text.has_value())
        {
            return false;
        }
        if (destination.hasFileExtension(".xencell"))
        {
            (void)deserialize_cell_file(*text);
        }
        else
        {
            (void)deserialize_project(*text);
        }
        return true;
    }
    catch (std::exception const &)
    {
        return false;
    }
}

void restore_interrupted_document(juce::File const &destination,
                                  juce::Array<juce::File> const &backups,
                                  juce::Array<juce::File> const &temporaries)
{
    auto restore_from = [&destination](juce::Array<juce::File> const &artifacts) {
        for (auto const &artifact : artifacts)
        {
            if (!destination.existsAsFile() &&
                valid_interrupted_document(artifact, destination) &&
                artifact.moveFileTo(destination))
            {
                break;
            }
        }
    };
    restore_from(backups);
    restore_from(temporaries);
    for (auto const &artifact : backups)
    {
        (void)artifact.deleteFile();
    }
    for (auto const &artifact : temporaries)
    {
        (void)artifact.deleteFile();
    }
}

} // namespace

void recover_content_directory(std::filesystem::path const &content_directory)
{
    auto const root = juce::File{content_directory.string()};
    if (!root.isDirectory())
    {
        throw DocumentError{DocumentErrorCode::Io,
                            "The configured content directory is unavailable."};
    }
    for (auto const &marker : {juce::String{".xen-backup."}, juce::String{".xen-tmp."}})
    {
        auto const interrupted =
            root.findChildFiles(juce::File::findFiles, true, "*" + marker + "*");
        for (auto const &artifact : interrupted)
        {
            auto const name = artifact.getFileName();
            auto const marker_at = name.indexOf(marker);
            if (marker_at <= 0)
            {
                continue;
            }
            auto const destination =
                artifact.getSiblingFile(name.substring(0, marker_at));
            if (!destination.hasFileExtension(".xenproj;.xencell"))
            {
                continue;
            }
            try
            {
                auto const relative_path = destination.getRelativePathFrom(root)
                                               .replaceCharacter('\\', '/')
                                               .toStdString();
                auto const extension =
                    destination.hasFileExtension(".xencell") ? ".xencell" : ".xenproj";
                auto const safe_path =
                    resolve_content_path(content_directory, relative_path, extension);
                auto error = std::error_code{};
                if (std::filesystem::weakly_canonical(
                        destination.getFullPathName().toStdString(), error) !=
                        safe_path ||
                    error)
                {
                    continue;
                }
            }
            catch (DocumentError const &)
            {
                continue;
            }
            auto lock = juce::InterProcessLock{
                juce::String{lock_name(destination.getFullPathName().toStdString())}};
            if (!lock.enter(5000))
            {
                continue;
            }
            if (destination.existsAsFile())
            {
                (void)artifact.deleteFile();
            }
            else
            {
                if (valid_interrupted_document(artifact, destination))
                {
                    (void)artifact.moveFileTo(destination);
                }
                else
                {
                    (void)artifact.deleteFile();
                }
            }
            lock.exit();
        }
    }
}

auto resolve_content_path(std::filesystem::path const &content_directory,
                          std::string const &relative_path,
                          std::string_view required_extension) -> std::filesystem::path
{
    if (relative_path.empty() || relative_path.size() > MAX_PERSISTED_STRING_BYTES)
    {
        throw DocumentError{DocumentErrorCode::InvalidPath,
                            "Content path must contain between 1 and 4096 bytes."};
    }
    auto const relative = std::filesystem::path{relative_path};
    if (relative.is_absolute() || relative.has_root_name() || relative.has_root_path())
    {
        throw DocumentError{DocumentErrorCode::InvalidPath,
                            "Content path must be relative."};
    }
    for (auto const &component : relative)
    {
        validate_segment(component.string());
    }
    if (relative.extension().string() != std::string{required_extension})
    {
        throw DocumentError{DocumentErrorCode::InvalidPath,
                            "Content path has the wrong file extension."};
    }
    if (relative.filename().stem().empty())
    {
        throw DocumentError{DocumentErrorCode::InvalidPath,
                            "Content filename must have a non-empty stem."};
    }

    auto error = std::error_code{};
    auto const root = std::filesystem::weakly_canonical(content_directory, error);
    if (error)
    {
        throw DocumentError{DocumentErrorCode::Io,
                            "The configured content directory is unavailable."};
    }
    auto const root_is_directory = std::filesystem::is_directory(root, error);
    if (error || !root_is_directory)
    {
        throw DocumentError{DocumentErrorCode::Io,
                            "The configured content directory is unavailable."};
    }
    auto const candidate = std::filesystem::weakly_canonical(root / relative, error);
    if (error || !is_within(root, candidate) || candidate == root)
    {
        throw DocumentError{DocumentErrorCode::InvalidPath,
                            "Content path escapes the configured content directory."};
    }
    return candidate;
}

auto read_content_file(std::filesystem::path const &content_directory,
                       std::string const &relative_path,
                       std::string_view required_extension, std::size_t maximum_bytes)
    -> StoredContentFile
{
    auto const path =
        resolve_content_path(content_directory, relative_path, required_extension);
    try
    {
        auto text = read_text_file(path, maximum_bytes);
        if (!text.has_value())
        {
            throw DocumentError{DocumentErrorCode::NotFound,
                                "Content file was not found: " + relative_path};
        }
        auto const revision = text_revision(*text);
        return {
            .file = make_file_info(relative_path, revision),
            .text = std::move(*text),
        };
    }
    catch (std::length_error const &)
    {
        throw DocumentError{DocumentErrorCode::FileTooLarge,
                            "Content file exceeds the permitted size."};
    }
    catch (DocumentError const &)
    {
        throw;
    }
    catch (std::exception const &)
    {
        throw DocumentError{DocumentErrorCode::Io,
                            "Unable to read the requested content file."};
    }
}

auto write_content_file(std::filesystem::path const &content_directory,
                        std::string const &relative_path,
                        std::string_view required_extension, std::string const &text,
                        std::size_t maximum_bytes,
                        std::optional<std::string> const &expected_file_revision)
    -> ContentFileInfo
{
    if (text.size() > maximum_bytes)
    {
        throw DocumentError{DocumentErrorCode::FileTooLarge,
                            "Serialized content exceeds the permitted size."};
    }
    auto path =
        resolve_content_path(content_directory, relative_path, required_extension);
    auto lock = juce::InterProcessLock{juce::String{lock_name(path)}};
    if (!lock.enter(5000))
    {
        throw DocumentError{DocumentErrorCode::Io,
                            "Timed out waiting for the content file lock."};
    }

    try
    {
        auto directory_error = std::error_code{};
        (void)std::filesystem::create_directories(path.parent_path(), directory_error);
        if (directory_error)
        {
            throw DocumentError{DocumentErrorCode::Io,
                                "Unable to create the content file directory."};
        }
        auto const verified_path =
            resolve_content_path(content_directory, relative_path, required_extension);
        if (verified_path != path)
        {
            throw DocumentError{
                DocumentErrorCode::InvalidPath,
                "Content path changed while its directory was being created."};
        }
        path = verified_path;
        auto const destination = juce::File{path.string()};
        auto const parent = destination.getParentDirectory();
        auto temporaries = parent.findChildFiles(
            juce::File::findFiles, false, destination.getFileName() + ".xen-tmp.*");
        auto backups = parent.findChildFiles(
            juce::File::findFiles, false, destination.getFileName() + ".xen-backup.*");
        restore_interrupted_document(destination, backups, temporaries);
        auto const require_expected_revision = [&] {
            auto const current = file_revision(path, maximum_bytes);
            if (!expected_file_revision.has_value() && current.has_value())
            {
                throw DocumentError{DocumentErrorCode::FileExists,
                                    "Content file already exists: " + relative_path,
                                    current};
            }
            if (expected_file_revision.has_value() && current != expected_file_revision)
            {
                throw DocumentError{
                    DocumentErrorCode::FileConflict,
                    "Content file changed since it was opened or listed.", current};
            }
        };
        require_expected_revision();
        atomic_write_text_file_checked(path, text, require_expected_revision);
        auto const revision = text_revision(text);
        lock.exit();
        return make_file_info(relative_path, revision);
    }
    catch (std::length_error const &)
    {
        lock.exit();
        throw DocumentError{DocumentErrorCode::FileTooLarge,
                            "Existing content file exceeds the permitted size."};
    }
    catch (DocumentError const &)
    {
        lock.exit();
        throw;
    }
    catch (std::exception const &)
    {
        lock.exit();
        throw DocumentError{DocumentErrorCode::Io,
                            "Unable to write the requested content file."};
    }
}

} // namespace xen
