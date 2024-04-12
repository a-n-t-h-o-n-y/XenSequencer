#pragma once

#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

namespace xen::gui
{

class PhraseDirectoryViewComponent : public juce::Component,
                                     public juce::ListBoxModel,
                                     public juce::ChangeListener,
                                     private juce::Timer
{
  private:
    inline static constexpr auto POLLING_MS = 2'000;

  public:
    sl::Signal<void(juce::File const &)> on_file_selected;
    sl::Signal<void(juce::File const &)> on_directory_change;

  public:
    explicit PhraseDirectoryViewComponent(juce::File const &initial_directory)
    {
        directory_contents_list_.setDirectory(initial_directory, true, true);
        this->on_directory_change(initial_directory);
        directory_contents_list_.addChangeListener(this);
        list_box_.setModel(this);
        this->addAndMakeVisible(list_box_);
        this->startTimer(POLLING_MS);
        dcl_thread_.startThread(juce::Thread::Priority::low);
    }

    ~PhraseDirectoryViewComponent() override
    {
        this->stopTimer();
        dcl_thread_.stopThread(3'000); // Allow some time for thread to finish
        directory_contents_list_.removeChangeListener(this);
    }

  public:
    auto resized() -> void override
    {
        list_box_.setBounds(this->getLocalBounds());
    }

    void changeListenerCallback(juce::ChangeBroadcaster *source) override
    {
        if (source == &directory_contents_list_)
        {
            list_box_.updateContent();
            list_box_.repaint();
        }
    }

    auto listBoxItemDoubleClicked(int row, juce::MouseEvent const &mouse)
        -> void override
    {
        if (mouse.mods.isLeftButtonDown())
        {
            this->item_selected(row);
        }
    }

    auto returnKeyPressed(int lastRowSelected) -> void override
    {
        this->item_selected(lastRowSelected);
    }

    auto keyPressed(juce::KeyPress const &key) -> bool override
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

  private:
    auto getNumRows() -> int override
    {
        return directory_contents_list_.getNumFiles() + 1;
    }

    auto paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                          bool rowIsSelected) -> void override
    {
        if (rowNumber >= 0)
        {
            rowNumber -= 1;
            if (rowIsSelected)
            {
                g.fillAll(juce::Colours::lightblue);
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

            g.setColour(juce::Colours::white);
            g.drawText(filename, 2, 0, width - 4, height,
                       juce::Justification::centredLeft, true);
        }
    }

    auto timerCallback() -> void override
    {
        directory_contents_list_.refresh();
    }

  private:
    // double clicked or enter
    auto item_selected(int index) -> void
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

  private:
    juce::TimeSliceThread dcl_thread_{"DirectoryViewComponentThread"};
    juce::WildcardFileFilter file_filter_{"*.json", "*", "JSON filter"};
    juce::DirectoryContentsList directory_contents_list_{&file_filter_, dcl_thread_};
    juce::ListBox list_box_;
};

} // namespace xen::gui