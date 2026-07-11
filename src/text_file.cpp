#include <xen/text_file.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <juce_core/juce_core.h>

namespace xen
{

namespace
{

auto as_juce_file(std::filesystem::path const &path) -> juce::File
{
    return juce::File{path.string()};
}

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

void atomic_write_text_file(std::filesystem::path const &path, std::string const &text)
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
    if (!temporary.replaceWithText(text))
    {
        throw std::runtime_error{"Unable to write temporary text file: " +
                                 temporary.getFullPathName().toStdString()};
    }
    if (!temporary.replaceFileIn(destination))
    {
        (void)temporary.deleteFile();
        throw std::runtime_error{"Unable to install text file: " + path.string()};
    }
}

} // namespace xen
