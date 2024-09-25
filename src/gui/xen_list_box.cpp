#include <xen/gui/xen_list_box.hpp>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/fonts.hpp>
#include <xen/gui/themes.hpp>

namespace xen::gui
{

XenListBox::XenListBox(juce::String const &component_id) : ListBox{"", this}
{
    this->setComponentID(component_id);
    this->setWantsKeyboardFocus(true);
    this->setHasFocusOutline(true);
}

void XenListBox::listBoxItemDoubleClicked(int row, juce::MouseEvent const &mouse)
{
    if (mouse.mods.isLeftButtonDown())
    {
        this->item_selected((std::size_t)row);
    }
}

void XenListBox::returnKeyPressed(int last_row_selected)
{
    this->item_selected((std::size_t)last_row_selected);
}

auto XenListBox::keyPressed(juce::KeyPress const &key) -> bool
{
    auto k = key;
    if (key.getTextCharacter() == 'j')
    {
        k = juce::KeyPress{juce::KeyPress::downKey, 0, 0};
    }
    else if (key.getTextCharacter() == 'k')
    {
        k = juce::KeyPress{juce::KeyPress::upKey, 0, 0};
    }
    return this->ListBox::keyPressed(k);
}

void XenListBox::lookAndFeelChanged()
{
    this->ListBox::setColour(juce::ListBox::backgroundColourId,
                             this->findColour(ColorID::BackgroundMedium));
}

void XenListBox::paintListBoxItem(int row_number, juce::Graphics &g, int width,
                                  int height, bool row_is_selected)
{
    if (row_number < this->getNumRows())
    {
        if (row_is_selected)
        {
            g.fillAll(this->findColour(ColorID::BackgroundLow));
            g.setColour(this->findColour(ColorID::ForegroundHigh));
        }
        else
        {
            g.fillAll(this->findColour(ColorID::BackgroundMedium));
            g.setColour(this->findColour(ColorID::ForegroundHigh));
        }
        auto const row_display = this->get_row_display((std::size_t)row_number);

        g.setFont(fonts::monospaced().regular.withHeight(17.f));
        g.drawText(row_display, 2, 0, width - 4, height,
                   juce::Justification::centredLeft, true);
    }
}

} // namespace xen::gui