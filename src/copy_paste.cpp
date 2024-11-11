#include <xen/copy_paste.hpp>

#include <exception>
#include <optional>
#include <string>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

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

/**
 * Acquire a file lock on the copy buffer file and invoke the provided function.
 * @param filepath The file to lock.
 * @param fn The function to invoke. Must take juce::File filename parameter.
 */
template <typename Fn>
void guarded_file_action(juce::File const &filepath, Fn &&fn)
{
    using FLock = boost::interprocess::file_lock;
    auto file_lock = FLock{filepath.getFullPathName().toRawUTF8()};

    {
        namespace bpt = boost::posix_time;
        auto const guard = boost::interprocess::scoped_lock<FLock>{
            file_lock,
            bpt::ptime{bpt::microsec_clock::universal_time()} + bpt::seconds(5)};
        if (!guard)
        {
            throw std::runtime_error("Failed to acquire file lock within timeout");
        }
        std::forward<Fn>(fn)(filepath);
    }
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

    guarded_file_action(filepath, [&json_str](juce::File const &fp) {
        if (!fp.replaceWithText(json_str))
        {
            throw std::runtime_error{"Failed to write copy buffer"};
        }
    });
}

auto read_copy_buffer() -> std::optional<sequence::Cell>
{
    auto const filepath = copy_buffer_filepath();

    if (!filepath.exists())
    {
        return std::nullopt;
    }

    auto json_str = std::string{};
    guarded_file_action(filepath, [&json_str](juce::File const &fp) {
        json_str = fp.loadFileAsString().toStdString();
    });

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