#include <xen/copy_paste.hpp>

#include <filesystem>

#include <xen/constants.hpp>
#include <xen/user_directory.hpp>

namespace xen
{

auto copy_buffer_filepath() -> std::filesystem::path
{
    return std::filesystem::path{
               get_user_library_directory().getFullPathName().toStdString()} /
           ("copy_buffer." +
            juce::String{VERSION}.replaceCharacter('.', '_').toStdString() + ".json");
}

} // namespace xen
