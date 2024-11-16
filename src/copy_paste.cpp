#include <xen/copy_paste.hpp>

#include <exception>
#include <optional>
#include <string>

#include <sequence/sequence.hpp>

#include <xen/constants.hpp>
#include <xen/serialize.hpp>
#include <xen/user_directory.hpp>

namespace
{

[[nodiscard]] auto copy_buffer_filepath() -> juce::File
{
    return xen::get_user_library_directory().getChildFile(
        "copy_buffer." + juce::String{xen::VERSION}.replaceCharacter('.', '_') +
        ".json");
}

} // namespace

namespace xen
{

void write_copy_buffer(sequence::Cell const &cell)
{
    auto const filepath = copy_buffer_filepath();
    auto const json_str = serialize_cell(cell);

    if (!filepath.create()) // Does not overwrite.
    {
        throw std::runtime_error{"Failed to create copy buffer"};
    }

    // Shared amongst multiple processes, but replaceWithText is 'safe' and atomic.
    if (!filepath.replaceWithText(json_str))
    {
        throw std::runtime_error{"Failed to write copy buffer"};
    }
}

auto read_copy_buffer() -> std::optional<sequence::Cell>
{
    auto const filepath = copy_buffer_filepath();
    auto const json_str = filepath.loadFileAsString().toStdString();

    if (json_str.empty()) // If file does not exist or other error.
    {
        return std::nullopt;
    }
    else
    {
        return deserialize_cell(json_str);
    }
}

} // namespace xen