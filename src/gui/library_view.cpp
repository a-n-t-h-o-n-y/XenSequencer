#include <xen/gui/library_view.hpp>

#include <cassert>
#include <cstddef>
#include <iterator>
#include <map>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/tuning.hpp>

#include <xen/gui/fonts.hpp>
#include <xen/gui/themes.hpp>
#include <xen/scale.hpp>

namespace
{

[[nodiscard]] auto scale_interval_display(xen::Scale const &scale) -> juce::String
{
    auto result = juce::String{"["};
    auto separator = juce::String{};
    for (auto i : scale.intervals)
    {
        result += separator;
        result += i;
        separator = ", ";
    }
    result += "]";
    return result;
}

} // namespace

namespace xen::gui
{

TuningsList::TuningsList(juce::File const &tunings_dir)
    : DirectoryListBox{tunings_dir,
                       juce::WildcardFileFilter{"*.scl", "*", "scala filter"},
                       "TuningsList"}
{
}

auto TuningsList::getTooltipForRow(int row) -> juce::String
{
    auto const file = this->get_file((std::size_t)row);
    try
    {
        if (file.has_value() && file->exists())
        {
            auto const at = tooltip_cache_.find(file->hashCode());
            if (at != std::cend(tooltip_cache_))
            {
                return at->second;
            }
            else if (file->getSize() < (1024 * 1'024))
            {
                auto const tuning =
                    sequence::from_scala(file->getFullPathName().toStdString());
                tooltip_cache_.emplace(file->hashCode(), tuning.description);
                return tuning.description;
            }
            else
            {
                return "File Too Large";
            }
        }
        else
        {
            return "";
        }
    }
    catch (...)
    {
        return "Error Reading " + file->getFileName();
    }
}

// -------------------------------------------------------------------------------------

ScalesList::ScalesList() : XenListBox{"ScalesList"}
{
}

void ScalesList::update(std::vector<::xen::Scale> const &scales)
{
    scales_ = scales;
    this->updateContent();
}

auto ScalesList::getNumRows() -> int
{
    return (int)scales_.size() + 1;
}

auto ScalesList::get_row_display(std::size_t index) -> juce::String
{
    return index == 0 ? "chromatic" : scales_[index - 1].name;
}

void ScalesList::item_selected(std::size_t index)
{
    assert(index < scales_.size() + 1);
    this->on_scale_selected(index == 0 ? "chromatic" : scales_[index - 1].name);
}

auto ScalesList::getTooltipForRow(int row) -> juce::String
{
    if ((std::size_t)row < scales_.size())
    {
        return ::scale_interval_display(scales_[(std::size_t)row]);
    }
    else
    {
        return "";
    }
}

// -------------------------------------------------------------------------------------

void LibraryView::Divider::paint(juce::Graphics &g)
{
    g.setColour(this->findColour(ColorID::ForegroundLow));
    g.fillAll();
}

// -------------------------------------------------------------------------------------

LibraryView::LibraryView(juce::File const &sequence_library_dir,
                         juce::File const &tuning_library_dir)
    : sequences{"Sequences", sequence_library_dir,
                juce::WildcardFileFilter{"*.xss", "*", "XenSeq filter"},
                "SequencesList"},
      tunings{"Tunings", tuning_library_dir}, scales{"Scales"}
{
    this->setComponentID("LibraryView");

    this->addAndMakeVisible(sequences);
    this->addAndMakeVisible(divider_1);

    this->addAndMakeVisible(tunings);
    this->addAndMakeVisible(divider_3);

    this->addAndMakeVisible(scales);
}

void LibraryView::resized()
{
    auto fb = juce::FlexBox{};
    fb.flexDirection = juce::FlexBox::Direction::row;

    fb.items.add(juce::FlexItem{sequences}.withFlex(1.f));
    fb.items.add(juce::FlexItem{divider_1}.withWidth(1.f));

    fb.items.add(juce::FlexItem{tunings}.withFlex(1.f));
    fb.items.add(juce::FlexItem{divider_3}.withWidth(1.f));

    fb.items.add(juce::FlexItem{scales}.withFlex(1.f));

    fb.performLayout(this->getLocalBounds());
}

} // namespace xen::gui