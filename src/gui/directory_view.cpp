#include <xen/gui/directory_view.hpp>

#include <xen/gui/color_ids.hpp>

namespace xen::gui
{

DirectoryView::DirectoryView(juce::File const &initial_directory,
                             juce::WildcardFileFilter const &file_filter)
    : file_filter_{file_filter}
{
    this->setWantsKeyboardFocus(false); // ListBox child will handle keyboard focus.
    directory_contents_list_.setDirectory(initial_directory, true, true);
    this->on_directory_change(initial_directory);
    directory_contents_list_.addChangeListener(this);
    list_box_.setModel(this);
    list_box_.setWantsKeyboardFocus(true); // This is default, but just to be explicit.
    this->addAndMakeVisible(list_box_);
    this->startTimer(POLLING_MS);
    dcl_thread_.startThread(juce::Thread::Priority::low);
}

DirectoryView::~DirectoryView()
{
    this->stopTimer();
    dcl_thread_.stopThread(3'000); // Allow some time for thread to finish
    directory_contents_list_.removeChangeListener(this);
}

auto DirectoryView::resized() -> void
{
    list_box_.setBounds(this->getLocalBounds());
}

auto DirectoryView::changeListenerCallback(juce::ChangeBroadcaster *source) -> void
{
    if (source == &directory_contents_list_)
    {
        list_box_.updateContent();
        list_box_.repaint();
    }
}

auto DirectoryView::listBoxItemDoubleClicked(int row,
                                             juce::MouseEvent const &mouse) -> void
{
    if (mouse.mods.isLeftButtonDown())
    {
        this->item_selected(row);
    }
}

auto DirectoryView::returnKeyPressed(int lastRowSelected) -> void
{
    this->item_selected(lastRowSelected);
}

auto DirectoryView::keyPressed(juce::KeyPress const &key) -> bool
{
    if (key.getTextCharacter() == 'j')
    {
        return list_box_.keyPressed(juce::KeyPress(juce::KeyPress::downKey, 0, 0));
    }
    else if (key.getTextCharacter() == 'k')
    {
        return list_box_.keyPressed(juce::KeyPress(juce::KeyPress::upKey, 0, 0));
    }
    return list_box_.keyPressed(key);
}

auto DirectoryView::lookAndFeelChanged() -> void
{
    list_box_.setColour(juce::ListBox::backgroundColourId,
                        this->findColour((int)DirectoryViewColorIDs::ItemBackground));
}

auto DirectoryView::getNumRows() -> int
{
    return directory_contents_list_.getNumFiles() + 1;
}

auto DirectoryView::paintListBoxItem(int rowNumber, juce::Graphics &g, int width,
                                     int height, bool rowIsSelected) -> void
{
    if (rowNumber >= 0)
    {
        rowNumber -= 1;
        if (rowIsSelected)
        {
            g.fillAll(
                this->findColour((int)DirectoryViewColorIDs::SelectedItemBackground));
            g.setColour(this->findColour((int)DirectoryViewColorIDs::SelectedItemText));
        }
        else
        {
            g.fillAll(this->findColour((int)DirectoryViewColorIDs::ItemBackground));
            g.setColour(this->findColour((int)DirectoryViewColorIDs::ItemText));
        }
        auto const filename = [&]() -> juce::String {
            if (rowNumber == -1)
            {
                return juce::String{".."} + juce::File::getSeparatorChar();
            }
            auto const file = directory_contents_list_.getFile(rowNumber);
            return file.isDirectory()
                       ? file.getFileName() + juce::File::getSeparatorChar()
                       : file.getFileNameWithoutExtension();
        }();

        g.drawText(filename, 2, 0, width - 4, height, juce::Justification::centredLeft,
                   true);
    }
}

auto DirectoryView::timerCallback() -> void
{
    directory_contents_list_.refresh();
}

auto DirectoryView::item_selected(int index) -> void
{
    index -= 1;

    if (index == -1)
    {
        auto const parent =
            directory_contents_list_.getDirectory().getParentDirectory();
        directory_contents_list_.setDirectory(parent, true, true);
        this->on_directory_change(parent);
        list_box_.selectRow(0);
        return;
    }

    auto file = directory_contents_list_.getFile(index);
    if (file.isDirectory())
    {
        directory_contents_list_.setDirectory(file, true, true);
        this->on_directory_change(file);
        list_box_.selectRow(0);
    }
    else
    {
        this->on_file_selected(file);
    }
}

// -------------------------------------------------------------------------------------

SequencesList::SequencesList(juce::File const &initial_directory)
    : DirectoryView{initial_directory,
                    juce::WildcardFileFilter{"*.xenseq", "*", "XenSeq filter"}}
{
    this->setComponentID("SequencesList");
}

// -------------------------------------------------------------------------------------

TuningsList::TuningsList(juce::File const &initial_directory)
    : DirectoryView{initial_directory,
                    juce::WildcardFileFilter{"*.scl", "*", "scala filter"}}
{
    this->setComponentID("TuningsList");
}

} // namespace xen::gui