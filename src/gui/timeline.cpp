#include <xen/gui/timeline.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

#include <sequence/measure.hpp>
#include <sequence/utility.hpp>

#include <xen/gui/color_ids.hpp>
#include <xen/state.hpp>

namespace
{

using namespace xen;

[[nodiscard]] auto build_measures(sequence::Phrase const &phrase,
                                  SelectedState const &selected)
    -> std::vector<std::unique_ptr<gui::TimelineMeasure>>
{
    auto measures = std::vector<std::unique_ptr<gui::TimelineMeasure>>{};
    auto index = std::size_t{0};
    for (auto const &measure : phrase)
    {
        measures.push_back(
            std::make_unique<gui::TimelineMeasure>(measure, index == selected.measure));
        ++index;
    }
    return measures;
}

auto paint_notes(juce::Graphics &g, sequence::Cell const &cell,
                 juce::Rectangle<float> bounds, juce::LookAndFeel const &laf) -> void
{
    using namespace sequence;
    std::visit(utility::overload{
                   [&](Note const &) {
                       g.setColour(laf.findColour((int)gui::TimelineColorIDs::Note));
                       g.fillRoundedRectangle(bounds.toFloat().reduced(2.f, 0.f), 2.f);
                   },
                   [&](Rest const &) {
                       g.setColour(laf.findColour((int)gui::TimelineColorIDs::Rest));
                       g.fillRoundedRectangle(bounds.toFloat().reduced(2.f, 0.f), 2.f);
                   },
                   [&](Sequence const &s) {
                       if (s.cells.empty())
                       {
                           return;
                       }
                       bounds =
                           bounds.withWidth(bounds.getWidth() / (float)s.cells.size());
                       for (auto const &c : s.cells)
                       {
                           paint_notes(g, c, bounds, laf);
                           bounds.setX(bounds.getX() + bounds.getWidth());
                       }
                   }},
               cell);
}

} // namespace

namespace xen::gui
{

void TimelineMeasure::paint(juce::Graphics &g)
{
    if (selected_)
    {
        g.setColour(this->findColour((int)TimelineColorIDs::SelectionHighlight));
        g.fillRoundedRectangle(this->getLocalBounds().toFloat(), 4.f);
    }

    paint_notes(g, measure_.cell, this->getLocalBounds().toFloat().reduced(4.f, 4.f),
                this->getLookAndFeel());
}

auto Timeline::set(sequence::Phrase const &phrase, SelectedState const &selected)
    -> void
{
    measures_.clear();
    measures_ = build_measures(phrase, selected);
    for (auto &measure : measures_)
    {
        this->addAndMakeVisible(*measure);
    }
    this->resized();
}

void Timeline::paint(juce::Graphics &g)
{
    g.fillAll(this->findColour((int)TimelineColorIDs::Background));

    auto const num_measures = measures_.size();
    if (num_measures == 0)
    {
        return;
    }

    g.setColour(this->findColour((int)TimelineColorIDs::VerticalSeparator));

    for (auto i = std::size_t{0}; i < num_measures - 1; ++i)
    {
        auto const &left_measure = *measures_[i];
        auto const &right_measure = *measures_[i + 1];

        auto const left_x = (float)left_measure.getRight();
        auto const right_x = (float)right_measure.getX();

        auto const line_x = (left_x + right_x) / 2.f;
        auto const line_y1 = (float)(left_measure.getY() + 10);
        auto const line_y2 = (float)(left_measure.getBottom() - 10);

        g.drawLine(line_x, line_y1, line_x, line_y2, 1.f);
    }
}

void Timeline::resized()
{
    auto flexbox = juce::FlexBox{};
    flexbox.flexDirection = juce::FlexBox::Direction::row;

    auto const total = [this] {
        auto x = 0.f;
        for (auto const &measure : measures_)
        {
            x += ((float)measure->time_signature().numerator /
                  (float)measure->time_signature().denominator);
        }
        return x;
    }();

    for (auto const &measure : measures_)
    {
        auto const flex = ((float)measure->time_signature().numerator /
                           (float)measure->time_signature().denominator) /
                          total;
        flexbox.items.add(juce::FlexItem(*measure).withFlex(flex));
    }

    flexbox.performLayout(this->getLocalBounds());
}

} // namespace xen::gui