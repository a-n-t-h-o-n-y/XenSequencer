#include <xen/copy_paste.hpp>

#include <xen/constants.hpp>
#include <xen/user_directory.hpp>

namespace xen
{

auto copy_buffer_filepath() -> juce::File
{
    return get_user_library_directory().getChildFile(
        "copy_buffer." + juce::String{VERSION}.replaceCharacter('.', '_') + ".json");
}

} // namespace xen
