#pragma once

#include <filesystem>
#include <functional>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace xen::gui
{

/**
 * @brief A button that opens a file dialog for loading a file.
 *
 * Calls on_file_selected with the selected std::filesystem::path.
 */
class LoadFileButton : public juce::TextButton
{
  public:
    LoadFileButton(juce::String const &text, juce::String const &dialog_text,
                   juce::String const &file_pattern)
        : juce::TextButton{text}
    {
        this->onClick = [=] {
            auto chooser = juce::FileChooser{dialog_text, {}, file_pattern};

            if (chooser.browseForFileToOpen())
            {
                auto const file = std::filesystem::path{
                    chooser.getResult().getFullPathName().toStdString()};
                if (this->on_file_selected)
                {
                    this->on_file_selected(file);
                }
            }
        };
    }

  public:
    std::function<void(std::filesystem::path const &)> on_file_selected;
};

/**
 * @brief A button that opens a file dialog for saving a file.
 */
class SaveFileButton : public juce::TextButton
{
  public:
    SaveFileButton(juce::String const &text, juce::String const &dialog_text,
                   juce::String const &file_pattern)
        : juce::TextButton{text}
    {
        this->onClick = [=] {
            auto chooser = juce::FileChooser{dialog_text, {}, file_pattern};

            if (chooser.browseForFileToSave(true))
            {
                auto const file = std::filesystem::path{
                    chooser.getResult().getFullPathName().toStdString()};
                if (this->on_file_selected)
                {
                    this->on_file_selected(file);
                }
            }
        };
    }

  public:
    std::function<void(std::filesystem::path const &)> on_file_selected;
};

} // namespace xen::gui