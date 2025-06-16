#include <xen/gui/modulation_pane.hpp>

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

namespace
{

using namespace xen::gui;

// {display name, command prefix}
auto COMMANDS = std::vector<std::pair<std::string, std::string>>{
    {"None", ""},
    {"Weight", "set weights "},
    {"Velocity", "set velocities "},
    {"Delay", "set delays "},
    {"Gate", "set gates "},
};

// TODO when min/max are set invalid/backwards, it throws, and the user can easily do
// this either you catch and ignore in this case or emit a warning, or you force the
// sliders to update, but that might be difficult in a generic way and annoying as a
// user if you are trying to set them separately, then you'd have to do it in a specific
// order.

// {display name, type/id, parameters}
auto MODULATORS =
    std::vector<std::tuple<std::string, std::string,
                           std::vector<ModulationParameters::SliderMetadata>>>{
        {
            "None",
            "",
            {},
        },
        {
            "Constant",
            "constant",
            {{
                .id = "value",
                .display_name = "Value",
                .initial = 1.f,
                .min = 0.01f,
                .max = 10.f,
            }},
        },
        {
            "Sine",
            "sine",
            {{
                 .id = "frequency",
                 .display_name = "Frequency",
                 .initial = 0.5f,
                 .min = 0.01f,
                 .max = 10.f,
                 .midpoint = 1.f,
             },
             {
                 .id = "amplitude",
                 .display_name = "Amplitude",
                 .initial = 1.f,
                 .min = 0.01f,
                 .max = 5.f,
             },
             {
                 .id = "phase",
                 .display_name = "Phase",
                 .initial = 0.f,
                 .min = 0.f,
                 .max = 1.f,
             }},
        },
        {
            "Triangle",
            "triangle",
            {{
                 .id = "frequency",
                 .display_name = "Frequency",
                 .initial = 0.5f,
                 .min = 0.01f,
                 .max = 10.f,
                 .midpoint = 1.f,
             },
             {
                 .id = "amplitude",
                 .display_name = "Amplitude",
                 .initial = 1.f,
                 .min = 0.01f,
                 .max = 5.f,
             },
             {
                 .id = "phase",
                 .display_name = "Phase",
                 .initial = 0.f,
                 .min = 0.f,
                 .max = 1.f,
             }},
        },
        {
            "Sawtooth Up",
            "sawtooth_up",
            {{
                 .id = "frequency",
                 .display_name = "Frequency",
                 .initial = 0.5f,
                 .min = 0.01f,
                 .max = 10.f,
                 .midpoint = 1.f,
             },
             {
                 .id = "amplitude",
                 .display_name = "Amplitude",
                 .initial = 1.f,
                 .min = 0.01f,
                 .max = 5.f,
             },
             {
                 .id = "phase",
                 .display_name = "Phase",
                 .initial = 0.f,
                 .min = 0.f,
                 .max = 1.f,
             }},
        },
        {
            "Sawtooth Down",
            "sawtooth_down",
            {{
                 .id = "frequency",
                 .display_name = "Frequency",
                 .initial = 0.5f,
                 .min = 0.01f,
                 .max = 10.f,
                 .midpoint = 1.f,
             },
             {
                 .id = "amplitude",
                 .display_name = "Amplitude",
                 .initial = 1.f,
                 .min = 0.01f,
                 .max = 5.f,
             },
             {
                 .id = "phase",
                 .display_name = "Phase",
                 .initial = 0.f,
                 .min = 0.f,
                 .max = 1.f,
             }},
        },
        {
            "Square",
            "square",
            {{
                 .id = "frequency",
                 .display_name = "Frequency",
                 .initial = 0.5f,
                 .min = 0.01f,
                 .max = 10.f,
                 .midpoint = 1.f,
             },
             {
                 .id = "amplitude",
                 .display_name = "Amplitude",
                 .initial = 1.f,
                 .min = 0.01f,
                 .max = 5.f,
             },
             {
                 .id = "phase",
                 .display_name = "Phase",
                 .initial = 0.f,
                 .min = 0.f,
                 .max = 1.f,
             },
             {
                 .id = "pulse_width",
                 .display_name = "Pulse Width",
                 .initial = 0.5f,
                 .min = 0.01f,
                 .max = 1.f,
             }},
        },
        {
            "Noise",
            "noise",
            {{
                .id = "amplitude",
                .display_name = "Amplitude",
                .initial = 1.f,
                .min = 0.01f,
                .max = 5.f,
            }},
        },
        {
            "Scale",
            "scale",
            {{
                .id = "factor",
                .display_name = "Factor",
                .initial = 1.f,
                .min = 0.01f,
                .max = 10.f,
            }},
        },
        {
            "Bias",
            "bias",
            {{
                .id = "amount",
                .display_name = "Amount",
                .initial = 0.f,
                .min = -5.f,
                .max = 5.f,
            }},
        },
        {"Absolute Value", "absolute_value", {}},
        {
            "Clamp",
            "clamp",
            {{
                 .id = "min",
                 .display_name = "Min",
                 .initial = 0.f,
                 .min = -10.f,
                 .max = 10.f,
             },
             {
                 .id = "max",
                 .display_name = "Max",
                 .initial = 1.f,
                 .min = -10.f,
                 .max = 10.f,
             }},
        },
        {"Invert", "invert", {}},
        {
            "Power",
            "power",
            {{
                .id = "amount",
                .display_name = "Amount",
                .initial = 2.f,
                .min = 0.01f,
                .max = 10.f,
            }},
        },
    };

} // namespace

namespace xen::gui
{

ModulationButtons::ModulationButtons()
{
    for (auto i = std::size_t{0}; i < buttons_.size(); ++i)
    {
        auto &btn = buttons_[i];

        btn.setButtonText(std::to_string(i));
        btn.onClick = [this, i] { this->on_index_selected.emit(i); };
        this->addAndMakeVisible(btn);
    }
}

void ModulationButtons::resized()
{
    using Track = juce::Grid::TrackInfo;
    using Fr = juce::Grid::Fr;

    auto const make_tracks = [](std::size_t count) -> juce::Array<Track> {
        auto tracks = juce::Array<Track>{};
        tracks.ensureStorageAllocated((int)count);
        for (auto i = std::size_t{0}; i < count; ++i)
        {
            tracks.add(Track(Fr(1)));
        }
        return tracks;
    };

    auto const width = 2;
    auto const height = std::size_t{8};

    auto grid = juce::Grid{};
    grid.templateColumns = make_tracks(width);
    grid.templateRows = make_tracks(height);

    for (auto row = std::size_t{0}; row < height; ++row)
    {
        grid.items.add(juce::GridItem(buttons_[row]));
        grid.items.add(juce::GridItem(buttons_[row + 8]));
    }

    grid.performLayout(this->getLocalBounds().reduced(4, 4));
}

// -------------------------------------------------------------------------------------

ModulationParameters::ModulationParameters(
    std::string const &mod_type, std::vector<SliderMetadata> const &slider_data)
    : type_{mod_type}
{
    for (auto const &data : slider_data)
    {
        if (data.initial < data.min || data.initial > data.max)
        {
            throw std::invalid_argument{"ModulationParameters: Slider initial value "
                                        "must be within min and max."};
        }
        if (data.midpoint.has_value() &&
            (*data.midpoint < data.min || *data.midpoint > data.max))
        {
            throw std::invalid_argument{"ModulationParameters: Slider midpoint must be "
                                        "within min and max."};
        }

        auto &[label_ptr, slider_ptr] = sliders_.emplace_back(std::pair{
            std::make_unique<juce::Label>(), std::make_unique<juce::Slider>()});

        auto &slider = *slider_ptr;
        auto &label = *label_ptr;

        this->addAndMakeVisible(label);
        this->addAndMakeVisible(slider);

        label.setText(data.display_name, juce::dontSendNotification);
        label.attachToComponent(&slider, false);

        slider.setComponentID(data.id);
        slider.setRange(data.min, data.max);
        slider.setValue(data.initial);
        if (data.midpoint.has_value())
        {
            slider.setSkewFactorFromMidPoint(*data.midpoint);
        }

        slider.setNumDecimalPlacesToDisplay(2);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                               slider.getTextBoxWidth() / 2, slider.getTextBoxHeight());

        slider.onValueChange = [this] { this->on_change(); };

        // TODO
        // on slider mouse up emit signal?
    }
}

auto ModulationParameters::get_json() -> nlohmann::json
{
    auto j = nlohmann::json{};
    j["type"] = type_;
    for (auto const &[_, slider_ptr] : sliders_)
    {
        j[slider_ptr->getComponentID().toStdString()] = slider_ptr->getValue();
    }
    return j;
}

auto ModulationParameters::empty() -> bool
{
    return type_.empty();
}

auto ModulationParameters::get_type() -> std::string const &
{
    return type_;
}

void ModulationParameters::resized()
{
    auto fb = juce::FlexBox{};
    fb.flexDirection = juce::FlexBox::Direction::column;
    for (auto const &[label_ptr, slider_ptr] : sliders_)
    {
        fb.items.add(juce::FlexItem(*label_ptr).withHeight(40.0f));
        fb.items.add(juce::FlexItem{*slider_ptr}.withHeight(60.f));
    }

    fb.performLayout(this->getLocalBounds());
}

// -------------------------------------------------------------------------------------

ModulationPane::ModulationPane()
{
    std::generate(std::begin(parameter_uis_), std::end(parameter_uis_), [] {
        return std::make_unique<ModulationParameters>("", std::get<2>(MODULATORS[0]));
    });

    this->addAndMakeVisible(target_command_dropdown_);
    this->addAndMakeVisible(modulator_dropdown_);
    this->addAndMakeVisible(buttons_);
    this->addAndMakeVisible(*parameter_uis_[current_selection_]);

    for (auto i = std::size_t{0}; i < MODULATORS.size(); ++i)
    {
        auto const &name = std::get<0>(MODULATORS[i]);
        modulator_dropdown_.addItem(name, (int)i + 1);
    }
    modulator_dropdown_.setSelectedId(1, juce::dontSendNotification);

    modulator_dropdown_.onChange = [this] {
        auto &ui_ptr = parameter_uis_[current_selection_];
        auto const mod_index = (std::size_t)modulator_dropdown_.getSelectedId() - 1;
        ui_ptr = std::make_unique<ModulationParameters>(
            std::get<1>(MODULATORS[mod_index]), std::get<2>(MODULATORS[mod_index]));
        ui_ptr->on_change.connect([this] {
            auto const cmd_str = this->generate_command_string();
            if (!cmd_str.empty())
            {
                this->on_change(cmd_str);
            }
        });
        this->addAndMakeVisible(*ui_ptr);
        this->resized();

        auto const cmd_str = this->generate_command_string();
        if (!cmd_str.empty())
        {
            this->on_change(cmd_str);
        }
    };

    for (auto i = std::size_t{0}; i < COMMANDS.size(); ++i)
    {
        auto const &name = COMMANDS[i].first;
        target_command_dropdown_.addItem(name, (int)i + 1);
    }
    target_command_dropdown_.setSelectedId(1, juce::dontSendNotification);

    target_command_dropdown_.onChange = [this] {
        auto const cmd_str = this->generate_command_string();
        if (!cmd_str.empty())
        {
            this->on_change(cmd_str);
        }
    };

    buttons_.on_index_selected.connect([this](std::size_t index) {
        this->removeChildComponent(parameter_uis_[current_selection_].get());
        current_selection_ = index;
        auto &current_ui = parameter_uis_[current_selection_];
        this->addAndMakeVisible(*current_ui);
        auto const mod_type = current_ui->get_type();
        auto at = std::find_if(
            std::begin(MODULATORS), std::end(MODULATORS),
            [&mod_type](auto const &tup) { return std::get<1>(tup) == mod_type; });
        if (at != std::end(MODULATORS))
        {
            modulator_dropdown_.setSelectedId(
                1 + (int)std::distance(std::begin(MODULATORS), at),
                juce::dontSendNotification);
        }
        this->resized();
    });
}

void ModulationPane::resized()
{
    auto left_fb = juce::FlexBox{};
    left_fb.flexDirection = juce::FlexBox::Direction::column;
    left_fb.items.add(juce::FlexItem{modulator_dropdown_}.withHeight(23.f));
    auto &current_ui = parameter_uis_[current_selection_];
    left_fb.items.add(juce::FlexItem{*current_ui}.withFlex(1.f));

    auto right_fb = juce::FlexBox{};
    right_fb.flexDirection = juce::FlexBox::Direction::column;
    right_fb.items.add(juce::FlexItem{target_command_dropdown_}.withHeight(23.f));
    right_fb.items.add(juce::FlexItem{buttons_}.withFlex(1.f));

    auto outer_fb = juce::FlexBox{};
    outer_fb.flexDirection = juce::FlexBox::Direction::row;
    outer_fb.items.add(juce::FlexItem{left_fb}.withFlex(1));
    outer_fb.items.add(juce::FlexItem{right_fb}.withFlex(1));

    outer_fb.performLayout(this->getLocalBounds());
}

auto ModulationPane::generate_json() -> std::string
{
    auto j = nlohmann::json{};
    j["type"] = "blend";
    j["children"] = std::array{
        nlohmann::json{{"type", "chain"},
                       {"children",
                        [this] {
                            auto result = std::vector<nlohmann::json>{};
                            for (auto i = std::size_t{0}; i < 8; ++i)
                            {
                                if (!parameter_uis_[i]->empty())
                                {
                                    result.push_back(parameter_uis_[i]->get_json());
                                }
                            }
                            return result;
                        }()}},
        nlohmann::json{{"type", "chain"},
                       {"children",
                        [this] {
                            auto result = std::vector<nlohmann::json>{};
                            for (auto i = std::size_t{8}; i < 16; ++i)
                            {
                                if (!parameter_uis_[i]->empty())
                                {
                                    result.push_back(parameter_uis_[i]->get_json());
                                }
                            }
                            return result;
                        }()}},
    };

    return j.dump();
}

auto ModulationPane::generate_command_string() -> std::string
{
    if (target_command_dropdown_.getSelectedId() == 1 ||
        modulator_dropdown_.getSelectedId() == 1)
    {
        return "";
    }
    else
    {
        auto const cmd_index =
            (std::size_t)target_command_dropdown_.getSelectedId() - 1;
        return COMMANDS[cmd_index].second + this->generate_json();
    }
}

} // namespace xen::gui