#include <xen/gui/sequence_bank.hpp>

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

#include <xen/gui/themes.hpp>

namespace
{

[[nodiscard]] auto to_hex(std::size_t num) -> std::string
{
    auto stream = std::stringstream{};
    stream << "0x" << std::uppercase << std::hex << num;
    return stream.str();
}

} // namespace

namespace xen::gui
{

void SequenceSquare::indicate()
{
    is_active_ = true;
    this->setColour(juce::TextButton::buttonColourId, this->get_color());
}

void SequenceSquare::unindicate()
{
    is_active_ = false;
    this->setColour(juce::TextButton::buttonColourId, this->get_color());
}

void SequenceSquare::lookAndFeelChanged()
{
    this->setColour(juce::TextButton::buttonColourId, this->get_color());
    this->setColour(juce::TextButton::textColourOffId,
                    this->findColour(ColorID::ForegroundHigh));
}

auto SequenceSquare::get_color() const -> juce::Colour
{
    return this->findColour(is_active_ ? ColorID::BackgroundInverse
                                       : ColorID::BackgroundMedium);
}

// -------------------------------------------------------------------------------------

SequenceBankGrid::SequenceBankGrid()
{
    for (auto i = std::size_t{0}; i < buttons_.size(); ++i)
    {
        auto &btn = buttons_[i];

        btn.setButtonText(to_hex(i));
        btn.onClick = [this, i] { this->on_index_selected.emit(i); };
        this->addAndMakeVisible(btn);
    }
}

void SequenceBankGrid::update(std::size_t selected_index)
{
    for (auto &btn : buttons_)
    {
        btn.unindicate();
    }
    buttons_[selected_index].indicate();
}

void SequenceBankGrid::resized()
{
    constexpr auto length = 4;

    auto grid = juce::Grid{};

    // Define the track sizes
    using Track = juce::Grid::TrackInfo;
    using Fr = juce::Grid::Fr;
    for (auto i = 0; i < length; ++i)
    {
        grid.templateRows.add(Track(Fr(1)));
        grid.templateColumns.add(Track(Fr(1)));
    }

    // left to right, bottom to top
    for (auto row = 0; row < length; ++row)
    {
        for (auto col = 0; col < length; ++col)
        {
            auto const index = (std::size_t)(16 - (row * length) + (col - 4));
            grid.items.add(juce::GridItem(buttons_[index]));
        }
    }

    grid.performLayout(this->getLocalBounds().reduced(4, 4));
}

void SequenceBankGrid::paint(juce::Graphics &g)
{
    g.fillAll(this->findColour(ColorID::BackgroundHigh));
}

} // namespace xen::gui