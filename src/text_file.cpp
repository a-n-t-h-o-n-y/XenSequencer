#include <xen/text_file.hpp>

#include <cerrno>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

namespace xen
{

namespace
{

auto as_juce_file(std::filesystem::path const &path) -> juce::File
{
    return juce::File{path.string()};
}

#if !defined(_WIN32)
void sync_path(juce::File const &file, int flags, char const *description)
{
    auto const descriptor = ::open(file.getFullPathName().toRawUTF8(), flags);
    if (descriptor < 0)
    {
        throw std::system_error{errno, std::generic_category(),
                                std::string{"Unable to open "} + description};
    }
    auto const sync_result = ::fsync(descriptor);
    auto const sync_error = errno;
    auto const close_result = ::close(descriptor);
    auto const close_error = errno;
    if (sync_result != 0)
    {
        throw std::system_error{sync_error, std::generic_category(),
                                std::string{"Unable to synchronize "} + description};
    }
    if (close_result != 0)
    {
        throw std::system_error{close_error, std::generic_category(),
                                std::string{"Unable to close "} + description};
    }
}
#endif

} // namespace

auto read_text_file(std::filesystem::path const &path) -> std::optional<std::string>
{
    auto input = std::ifstream{path, std::ios::binary};
    if (!input)
    {
        return std::nullopt;
    }

    auto buffer = std::ostringstream{};
    buffer << input.rdbuf();
    if (input.bad())
    {
        throw std::runtime_error{"Unable to read text file: " + path.string()};
    }
    return buffer.str();
}

auto read_text_file(std::filesystem::path const &path, std::size_t maximum_bytes)
    -> std::optional<std::string>
{
    auto error = std::error_code{};
    auto const size = std::filesystem::file_size(path, error);
    if (error)
    {
        auto exists_error = std::error_code{};
        auto const exists = std::filesystem::exists(path, exists_error);
        if (!exists_error && !exists)
        {
            return std::nullopt;
        }
        throw std::runtime_error{"Unable to inspect text file: " + path.string()};
    }
    if (size > maximum_bytes)
    {
        throw std::length_error{"Text file exceeds the permitted size: " +
                                path.string()};
    }

    auto input = std::ifstream{path, std::ios::binary};
    if (!input)
    {
        return std::nullopt;
    }
    auto text = std::string{};
    text.resize(static_cast<std::size_t>(size));
    if (!text.empty())
    {
        input.read(text.data(), static_cast<std::streamsize>(text.size()));
    }
    if (input.bad() || static_cast<std::size_t>(input.gcount()) != text.size())
    {
        throw std::runtime_error{"Unable to read text file: " + path.string()};
    }
    if (input.peek() != std::char_traits<char>::eof())
    {
        throw std::runtime_error{"Text file changed while being read: " +
                                 path.string()};
    }
    return text;
}

auto text_revision(std::string const &text) -> std::string
{
    auto const digest = juce::SHA256{text.data(), text.size()};
    return "sha256:" + digest.toHexString().toStdString();
}

auto file_revision(std::filesystem::path const &path, std::size_t maximum_bytes)
    -> std::optional<std::string>
{
    auto const text = read_text_file(path, maximum_bytes);
    return text.has_value() ? std::optional<std::string>{text_revision(*text)}
                            : std::nullopt;
}

void atomic_write_text_file_checked(std::filesystem::path const &path,
                                    std::string const &text,
                                    std::function<void()> before_install)
{
    auto const destination = as_juce_file(path);
    auto const parent = destination.getParentDirectory();
    if (!parent.isDirectory() && !parent.createDirectory().wasOk())
    {
        throw std::runtime_error{"Unable to create text file directory: " +
                                 parent.getFullPathName().toStdString()};
    }

    auto const temporary = destination.getSiblingFile(
        destination.getFileName() + ".xen-tmp." + juce::Uuid{}.toString());
    auto output = temporary.createOutputStream();
    if (output == nullptr ||
        !output->write(text.data(), static_cast<std::size_t>(text.size())))
    {
        output.reset();
        (void)temporary.deleteFile();
        throw std::runtime_error{"Unable to write temporary text file: " +
                                 temporary.getFullPathName().toStdString()};
    }
    output->flush();
    output.reset();
#if !defined(_WIN32)
    try
    {
        sync_path(temporary, O_RDWR, "temporary text file");
    }
    catch (...)
    {
        (void)temporary.deleteFile();
        throw;
    }
#endif
    if (before_install)
    {
        try
        {
            before_install();
        }
        catch (...)
        {
            (void)temporary.deleteFile();
            throw;
        }
    }
    if (!temporary.replaceFileIn(destination))
    {
        (void)temporary.deleteFile();
        throw std::runtime_error{"Unable to install text file: " + path.string()};
    }
#if !defined(_WIN32)
    auto directory_flags = O_RDONLY;
#ifdef O_DIRECTORY
    directory_flags |= O_DIRECTORY;
#endif
    sync_path(parent, directory_flags, "text file directory");
#endif
}

void atomic_write_text_file(std::filesystem::path const &path, std::string const &text)
{
    atomic_write_text_file_checked(path, text, {});
}

} // namespace xen
