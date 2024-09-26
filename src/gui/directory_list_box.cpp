#include <xen/gui/directory_list_box.hpp>

#include <xen/gui/fonts.hpp>
#include <xen/gui/themes.hpp>

namespace xen::gui
{
DirectoryListBox::DirectoryListBox(juce::File const &initial_directory,
                                   juce::WildcardFileFilter const &file_filter,
                                   juce::String const &component_id)
    : XenListBox{component_id}, file_filter_{file_filter}
{
    directory_contents_list_.setDirectory(initial_directory, true, true);
    this->on_directory_change(initial_directory);
    directory_contents_list_.addChangeListener(this);
    dcl_thread_.startThread(juce::Thread::Priority::low);
}

DirectoryListBox::~DirectoryListBox()
{
    this->stopTimer();
    dcl_thread_.stopThread(3'000); // Allow some time for thread to finish
    directory_contents_list_.removeChangeListener(this);
}

auto DirectoryListBox::getNumRows() -> int
{
    return directory_contents_list_.getNumFiles() + 1;
}

void DirectoryListBox::changeListenerCallback(juce::ChangeBroadcaster *source)
{
    if (source == &directory_contents_list_)
    {
        this->updateContent();
        this->repaint();
    }
}

void DirectoryListBox::timerCallback()
{
    directory_contents_list_.refresh();
}

void DirectoryListBox::visibilityChanged()
{
    if (this->isVisible())
    {
        this->startTimer(POLLING_MS);
    }
    else
    {
        this->stopTimer();
    }
}

auto DirectoryListBox::get_row_display(std::size_t index) -> juce::String
{
    if (index == 0)
    {
        return juce::String{".."} + juce::File::getSeparatorChar();
    }
    auto const file = directory_contents_list_.getFile((int)index - 1);
    return file.isDirectory() ? file.getFileName() + juce::File::getSeparatorChar()
                              : file.getFileNameWithoutExtension();
}

void DirectoryListBox::item_selected(std::size_t index)
{
    if (index == 0) // Parent Directory
    {
        auto const parent =
            directory_contents_list_.getDirectory().getParentDirectory();
        directory_contents_list_.setDirectory(parent, true, true);
        this->on_directory_change(parent);
        this->ListBox::selectRow(0);
        return;
    }
    else if (auto file = directory_contents_list_.getFile((int)index - 1);
             file.isDirectory())
    {
        directory_contents_list_.setDirectory(file, true, true);
        this->on_directory_change(file);
        this->ListBox::selectRow(0);
    }
    else
    {
        this->on_file_selected(file);
    }
}

auto DirectoryListBox::get_file(std::size_t index) -> std::optional<juce::File>
{
    auto const file = directory_contents_list_.getFile((int)index - 1);
    if (file.exists() && !file.isDirectory())
    {
        return file;
    }
    else
    {
        return std::nullopt;
    }
}

} // namespace xen::gui