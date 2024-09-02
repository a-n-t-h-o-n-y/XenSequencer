#include <xen/gui/directory_view.hpp>

#include <xen/gui/fonts.hpp>
#include <xen/gui/themes.hpp>

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

void DirectoryView::resized()
{
    list_box_.setBounds(this->getLocalBounds());
}

void DirectoryView::changeListenerCallback(juce::ChangeBroadcaster *source)
{
    if (source == &directory_contents_list_)
    {
        list_box_.updateContent();
        list_box_.repaint();
    }
}

void DirectoryView::listBoxItemDoubleClicked(int row, juce::MouseEvent const &mouse)
{
    if (mouse.mods.isLeftButtonDown())
    {
        this->item_selected(row);
    }
}

void DirectoryView::returnKeyPressed(int lastRowSelected)
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

void DirectoryView::lookAndFeelChanged()
{
    list_box_.setColour(juce::ListBox::backgroundColourId,
                        this->findColour(ColorID::BackgroundMedium));
}

auto DirectoryView::getNumRows() -> int
{
    return directory_contents_list_.getNumFiles() + 1;
}

void DirectoryView::paintListBoxItem(int rowNumber, juce::Graphics &g, int width,
                                     int height, bool rowIsSelected)
{
    if (rowNumber >= 0)
    {
        rowNumber -= 1;
        if (rowIsSelected)
        {
            g.fillAll(this->findColour(ColorID::BackgroundLow));
            g.setColour(this->findColour(ColorID::ForegroundHigh));
        }
        else
        {
            g.fillAll(this->findColour(ColorID::BackgroundMedium));
            g.setColour(this->findColour(ColorID::ForegroundHigh));
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

        g.setFont(fonts::monospaced().regular.withHeight(16.f));
        g.drawText(filename, 2, 0, width - 4, height, juce::Justification::centredLeft,
                   true);
    }
}

void DirectoryView::timerCallback()
{
    directory_contents_list_.refresh();
}

void DirectoryView::item_selected(int index)
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