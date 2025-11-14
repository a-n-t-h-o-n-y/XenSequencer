#include <xen/gui/modulation_pane.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <xen/gui/themes.hpp>
#include <xen/modulator.hpp>

namespace
{

using namespace xen::gui;

struct WaveformMetadata
{
    std::string display_name;
    std::string command_name;
};

// TODO complete this list
// <ID, Metadata> - ID is used in combobox
auto const WAVEFORMS = std::map<int, WaveformMetadata>{
    std::pair{1, WaveformMetadata{"Sine", "sine"}},
    std::pair{2, WaveformMetadata{"Triangle", "triangle"}},
    std::pair{3, WaveformMetadata{"Sawtooth", "sawtooth_up"}},
    std::pair{4, WaveformMetadata{"Square", "square"}},
};

// TODO update this if you add to WAVEFORMS
[[nodiscard]]
auto make_modulator(std::string const &waveform_cmd_name, float frequency, float offset)
    -> xen::Modulator
{
    if (waveform_cmd_name == "sine")
    {
        return xen::modulator::sine(frequency, 1.f, offset);
    }
    else if (waveform_cmd_name == "triangle")
    {
        return xen::modulator::triangle(frequency, 1.f, offset);
    }
    else if (waveform_cmd_name == "sawtooth_up")
    {
        return xen::modulator::sawtooth_up(frequency, 1.f, offset);
    }
    else if (waveform_cmd_name == "square")
    {
        return xen::modulator::square(frequency, 1.f, offset, 0.5f);
    }
    else
    {
        throw std::invalid_argument("Unknown waveform command name: " +
                                    waveform_cmd_name);
    }
}

template <std::size_t N>
[[nodiscard]]
auto lerp_samples(std::array<float, N> const &a, std::array<float, N> const &b, float t)
    -> std::array<float, N>
{
    auto result = std::array<float, N>{};
    for (std::size_t i = 0; i < N; ++i)
    {
        result[i] = a[i] * (1.f - t) + b[i] * t;
    }
    return result;
}

/// Returns true if \p subject is within \p radius of \p target.
[[nodiscard]]
auto is_within_target(juce::Point<float> subject, juce::Point<float> target,
                      float radius) -> bool
{
    auto const distance_sq = subject.getDistanceSquaredFrom(target);
    auto const radius_sq = radius * radius;
    return distance_sq <= radius_sq;
}

[[nodiscard]]
auto get_handle_position(float frequency, float offset,
                         juce::Rectangle<float> const &bounds) -> juce::Point<float>
{
    return {
        frequency * bounds.getWidth() / 4.f + bounds.getX(),
        (0.5f - offset) * bounds.getHeight() + bounds.getY(),
    };
}

/// Find and return the closes value in \p values to \p target
template <std::size_t N>
[[nodiscard]]
auto closest_value(std::array<float, N> const &values, float target) -> float
{
    auto closest = values.front();
    auto min_diff = std::numeric_limits<float>::max();

    for (auto const &v : values)
    {
        auto const diff = std::abs(v - target);
        if (diff < min_diff)
        {
            min_diff = diff;
            closest = v;
        }
    }

    return closest;
}

template <std::size_t N, std::size_t M>
void draw_grid(juce::Graphics &g, juce::Rectangle<float> bounds, juce::Colour color,
               std::array<float, N> const &frequency_grid_values,
               std::array<float, M> const &offset_grid_values)
{
    g.setColour(color);
    g.setOpacity(0.3f);

    const auto w = bounds.getWidth();
    const auto h = bounds.getHeight();
    const float line_thickness = 0.5f;

    // vertical
    for (auto const frequency : frequency_grid_values)
    {
        g.fillRect(bounds.getX() + w * frequency / 4.f, bounds.getY(), line_thickness,
                   h);
    }

    // horizontal
    for (auto const offset : offset_grid_values)
    {
        g.fillRect(bounds.getX(), bounds.getY() + h * (0.5f - offset), w,
                   line_thickness);
    }
}

// {display name, command prefix}
// auto COMMANDS = std::vector<std::pair<std::string, std::string>>{
//     {"None", ""},
//     {"Weight", "set weights "},
//     {"Velocity", "set velocity "},
//     {"Delay", "set delay "},
//     {"Gate", "set gate "},
// };

// TODO when min/max are set invalid/backwards, it throws, and the user can easily do
// this either you catch and ignore in this case or emit a warning, or you force the
// sliders to update, but that might be difficult in a generic way and annoying as a
// user if you are trying to set them separately, then you'd have to do it in a specific
// order.

// {display name, type/id, parameters}
// auto MODULATORS =
//     std::vector<std::tuple<std::string, std::string,
//     std::vector<XenSlider::Metadata>>>{
//         {
//             "None",
//             "",
//             {},
//         },
//         {
//             "Constant",
//             "constant",
//             {{
//                 .id = "value",
//                 .display_name = "Value",
//                 .initial = 1.f,
//                 .min = 0.01f,
//                 .max = 10.f,
//             }},
//         },
//         {
//             "Sine",
//             "sine",
//             {{
//                  .id = "frequency",
//                  .display_name = "Frequency",
//                  .initial = 0.5f,
//                  .min = 0.01f,
//                  .max = 10.f,
//                  .midpoint = 1.f,
//              },
//              {
//                  .id = "amplitude",
//                  .display_name = "Amplitude",
//                  .initial = 1.f,
//                  .min = 0.01f,
//                  .max = 5.f,
//              },
//              {
//                  .id = "phase",
//                  .display_name = "Phase",
//                  .initial = 0.f,
//                  .min = 0.f,
//                  .max = 1.f,
//              }},
//         },
//         {
//             "Triangle",
//             "triangle",
//             {{
//                  .id = "frequency",
//                  .display_name = "Frequency",
//                  .initial = 0.5f,
//                  .min = 0.01f,
//                  .max = 10.f,
//                  .midpoint = 1.f,
//              },
//              {
//                  .id = "amplitude",
//                  .display_name = "Amplitude",
//                  .initial = 1.f,
//                  .min = 0.01f,
//                  .max = 5.f,
//              },
//              {
//                  .id = "phase",
//                  .display_name = "Phase",
//                  .initial = 0.f,
//                  .min = 0.f,
//                  .max = 1.f,
//              }},
//         },
//         {
//             "Sawtooth Up",
//             "sawtooth_up",
//             {{
//                  .id = "frequency",
//                  .display_name = "Frequency",
//                  .initial = 0.5f,
//                  .min = 0.01f,
//                  .max = 10.f,
//                  .midpoint = 1.f,
//              },
//              {
//                  .id = "amplitude",
//                  .display_name = "Amplitude",
//                  .initial = 1.f,
//                  .min = 0.01f,
//                  .max = 5.f,
//              },
//              {
//                  .id = "phase",
//                  .display_name = "Phase",
//                  .initial = 0.f,
//                  .min = 0.f,
//                  .max = 1.f,
//              }},
//         },
//         {
//             "Sawtooth Down",
//             "sawtooth_down",
//             {{
//                  .id = "frequency",
//                  .display_name = "Frequency",
//                  .initial = 0.5f,
//                  .min = 0.01f,
//                  .max = 10.f,
//                  .midpoint = 1.f,
//              },
//              {
//                  .id = "amplitude",
//                  .display_name = "Amplitude",
//                  .initial = 1.f,
//                  .min = 0.01f,
//                  .max = 5.f,
//              },
//              {
//                  .id = "phase",
//                  .display_name = "Phase",
//                  .initial = 0.f,
//                  .min = 0.f,
//                  .max = 1.f,
//              }},
//         },
//         {
//             "Square",
//             "square",
//             {{
//                  .id = "frequency",
//                  .display_name = "Frequency",
//                  .initial = 0.5f,
//                  .min = 0.01f,
//                  .max = 10.f,
//                  .midpoint = 1.f,
//              },
//              {
//                  .id = "amplitude",
//                  .display_name = "Amplitude",
//                  .initial = 1.f,
//                  .min = 0.01f,
//                  .max = 5.f,
//              },
//              {
//                  .id = "phase",
//                  .display_name = "Phase",
//                  .initial = 0.f,
//                  .min = 0.f,
//                  .max = 1.f,
//              },
//              {
//                  .id = "pulse_width",
//                  .display_name = "Pulse Width",
//                  .initial = 0.5f,
//                  .min = 0.01f,
//                  .max = 1.f,
//              }},
//         },
//         {
//             "Noise",
//             "noise",
//             {{
//                 .id = "amplitude",
//                 .display_name = "Amplitude",
//                 .initial = 1.f,
//                 .min = 0.01f,
//                 .max = 5.f,
//             }},
//         },
//         {
//             "Scale",
//             "scale",
//             {{
//                 .id = "factor",
//                 .display_name = "Factor",
//                 .initial = 1.f,
//                 .min = 0.01f,
//                 .max = 10.f,
//             }},
//         },
//         {
//             "Bias",
//             "bias",
//             {{
//                 .id = "amount",
//                 .display_name = "Amount",
//                 .initial = 0.f,
//                 .min = -5.f,
//                 .max = 5.f,
//             }},
//         },
//         {"Absolute Value", "absolute_value", {}},
//         {
//             "Clamp",
//             "clamp",
//             {{
//                  .id = "min",
//                  .display_name = "Min",
//                  .initial = 0.f,
//                  .min = -10.f,
//                  .max = 10.f,
//              },
//              {
//                  .id = "max",
//                  .display_name = "Max",
//                  .initial = 1.f,
//                  .min = -10.f,
//                  .max = 10.f,
//              }},
//         },
//         {"Invert", "invert", {}},
//         {
//             "Power",
//             "power",
//             {{
//                 .id = "amount",
//                 .display_name = "Amount",
//                 .initial = 2.f,
//                 .min = 0.01f,
//                 .max = 10.f,
//             }},
//         },
//     };

} // namespace

namespace xen::gui
{

// ModulationButtons::ModulationButtons()
// {
//     for (auto i = std::size_t{0}; i < buttons_.size(); ++i)
//     {
//         auto &btn = buttons_[i];

//         btn.setButtonText(std::to_string(i));
//         btn.onClick = [this, i] { this->on_index_selected.emit(i); };
//         this->addAndMakeVisible(btn);
//     }
// }

// void ModulationButtons::resized()
// {
//     using Track = juce::Grid::TrackInfo;
//     using Fr = juce::Grid::Fr;

//     auto const make_tracks = [](std::size_t count) -> juce::Array<Track> {
//         auto tracks = juce::Array<Track>{};
//         tracks.ensureStorageAllocated((int)count);
//         for (auto i = std::size_t{0}; i < count; ++i)
//         {
//             tracks.add(Track(Fr(1)));
//         }
//         return tracks;
//     };

//     auto const width = 2;
//     auto const height = std::size_t{8};

//     auto grid = juce::Grid{};
//     grid.templateColumns = make_tracks(width);
//     grid.templateRows = make_tracks(height);

//     for (auto row = std::size_t{0}; row < height; ++row)
//     {
//         grid.items.add(juce::GridItem(buttons_[row]));
//         grid.items.add(juce::GridItem(buttons_[row + 8]));
//     }

//     grid.performLayout(this->getLocalBounds().reduced(4, 4));
// }

// -------------------------------------------------------------------------------------

// ModulationParameters::ModulationParameters(
//     std::string const &mod_type, std::vector<XenSlider::Metadata> const &slider_data)
//     : type_{mod_type}
// {
//     for (auto const &data : slider_data)
//     {
//         auto &slider_ptr = sliders_.emplace_back(
//             std::make_unique<XenSlider>(data, juce::Slider::LinearHorizontal));

//         this->addAndMakeVisible(*slider_ptr);

//         slider_ptr->on_change.connect([this](float) { this->on_change(); });
//         slider_ptr->on_release.connect([this] { this->on_commit(); });
//     }
// }

// auto ModulationParameters::get_json() -> nlohmann::json
// {
//     auto j = nlohmann::json{};
//     j["type"] = type_;
//     for (auto const &slider_ptr : sliders_)
//     {
//         j[slider_ptr->slider.getComponentID().toStdString()] =
//             slider_ptr->slider.getValue();
//     }
//     return j;
// }

// auto ModulationParameters::empty() -> bool
// {
//     return type_.empty();
// }

// auto ModulationParameters::get_type() -> std::string const &
// {
//     return type_;
// }

// void ModulationParameters::paint(juce::Graphics &g)
// {
//     g.fillAll(this->findColour(ColorID::BackgroundHigh));
// }

// void ModulationParameters::resized()
// {
//     auto fb = juce::FlexBox{};

//     fb.flexDirection = juce::FlexBox::Direction::column;
//     for (auto const &slider_ptr : sliders_)
//     {
//         fb.items.add(juce::FlexItem{*slider_ptr}.withFlex(1.f).withMaxHeight(100.f));
//     }

//     fb.performLayout(this->getLocalBounds());
// }

// -------------------------------------------------------------------------------------

// ModulationPane::ModulationPane()
// {
//     std::generate(std::begin(parameter_uis_), std::end(parameter_uis_), [] {
//         return std::make_unique<ModulationParameters>("",
//         std::get<2>(MODULATORS[0]));
//     });

//     this->addAndMakeVisible(target_command_dropdown_);
//     this->addAndMakeVisible(modulator_dropdown_);
//     this->addAndMakeVisible(buttons_);
//     this->addAndMakeVisible(*parameter_uis_[current_selection_]);

//     for (auto i = std::size_t{0}; i < MODULATORS.size(); ++i)
//     {
//         auto const &name = std::get<0>(MODULATORS[i]);
//         modulator_dropdown_.addItem(name, (int)i + 1);
//     }
//     modulator_dropdown_.setSelectedId(1, juce::dontSendNotification);

//     modulator_dropdown_.onChange = [this] {
//         auto &ui_ptr = parameter_uis_[current_selection_];
//         auto const mod_index = (std::size_t)modulator_dropdown_.getSelectedId() - 1;
//         ui_ptr = std::make_unique<ModulationParameters>(
//             std::get<1>(MODULATORS[mod_index]), std::get<2>(MODULATORS[mod_index]));
//         ui_ptr->on_change.connect([this] {
//             auto const cmd_str = this->generate_command_string(false);
//             if (!cmd_str.empty())
//             {
//                 this->on_change(cmd_str);
//             }
//         });
//         ui_ptr->on_commit.connect([this] {
//             auto const cmd_str = this->generate_command_string(true);
//             if (!cmd_str.empty())
//             {
//                 this->on_change(cmd_str);
//             }
//         });
//         this->addAndMakeVisible(*ui_ptr);
//         this->resized();

//         auto const cmd_str = this->generate_command_string(true);
//         if (!cmd_str.empty())
//         {
//             this->on_change(cmd_str);
//         }
//     };

//     for (auto i = std::size_t{0}; i < COMMANDS.size(); ++i)
//     {
//         auto const &name = COMMANDS[i].first;
//         target_command_dropdown_.addItem(name, (int)i + 1);
//     }
//     target_command_dropdown_.setSelectedId(1, juce::dontSendNotification);

//     target_command_dropdown_.onChange = [this] {
//         auto const cmd_str = this->generate_command_string(true);
//         if (!cmd_str.empty())
//         {
//             this->on_change(cmd_str);
//         }
//     };

//     buttons_.on_index_selected.connect([this](std::size_t index) {
//         this->removeChildComponent(parameter_uis_[current_selection_].get());
//         current_selection_ = index;
//         auto &current_ui = parameter_uis_[current_selection_];
//         this->addAndMakeVisible(*current_ui);
//         auto const mod_type = current_ui->get_type();
//         auto at = std::find_if(
//             std::begin(MODULATORS), std::end(MODULATORS),
//             [&mod_type](auto const &tup) { return std::get<1>(tup) == mod_type; });
//         if (at != std::end(MODULATORS))
//         {
//             modulator_dropdown_.setSelectedId(
//                 1 + (int)std::distance(std::begin(MODULATORS), at),
//                 juce::dontSendNotification);
//         }
//         this->resized();
//     });
// }

// void ModulationPane::resized()
// {
//     auto left_fb = juce::FlexBox{};
//     left_fb.flexDirection = juce::FlexBox::Direction::column;
//     left_fb.items.add(juce::FlexItem{modulator_dropdown_}.withHeight(23.f));
//     auto &current_ui = parameter_uis_[current_selection_];
//     left_fb.items.add(juce::FlexItem{*current_ui}.withFlex(1.f));

//     auto right_fb = juce::FlexBox{};
//     right_fb.flexDirection = juce::FlexBox::Direction::column;
//     right_fb.items.add(juce::FlexItem{target_command_dropdown_}.withHeight(23.f));
//     right_fb.items.add(juce::FlexItem{buttons_}.withFlex(1.f));

//     auto outer_fb = juce::FlexBox{};
//     outer_fb.flexDirection = juce::FlexBox::Direction::row;
//     outer_fb.items.add(juce::FlexItem{left_fb}.withFlex(1));
//     outer_fb.items.add(juce::FlexItem{right_fb}.withFlex(1));

//     outer_fb.performLayout(this->getLocalBounds());
// }

// auto ModulationPane::generate_json() -> std::string
// {
//     auto j = nlohmann::json{};
//     j["type"] = "blend";
//     j["children"] = std::array{
//         nlohmann::json{{"type", "chain"},
//                        {"children",
//                         [this] {
//                             auto result = std::vector<nlohmann::json>{};
//                             for (auto i = std::size_t{0}; i < 8; ++i)
//                             {
//                                 if (!parameter_uis_[i]->empty())
//                                 {
//                                     result.push_back(parameter_uis_[i]->get_json());
//                                 }
//                             }
//                             return result;
//                         }()}},
//         nlohmann::json{{"type", "chain"},
//                        {"children",
//                         [this] {
//                             auto result = std::vector<nlohmann::json>{};
//                             for (auto i = std::size_t{8}; i < 16; ++i)
//                             {
//                                 if (!parameter_uis_[i]->empty())
//                                 {
//                                     result.push_back(parameter_uis_[i]->get_json());
//                                 }
//                             }
//                             return result;
//                         }()}},
//     };

//     return j.dump();
// }

// auto ModulationPane::generate_command_string(bool commit) -> std::string
// {
//     if (target_command_dropdown_.getSelectedId() == 1 ||
//         modulator_dropdown_.getSelectedId() == 1)
//     {
//         return "";
//     }
//     else
//     {
//         auto const cmd_index =
//             (std::size_t)target_command_dropdown_.getSelectedId() - 1;
//         return COMMANDS[cmd_index].second + this->generate_json() +
//                (commit ? " true" : " false");
//     }
// }

WaveformSelect::WaveformSelect(int initial_selection)
{
    this->addAndMakeVisible(combo_box_);
    combo_box_.setEditableText(false);
    combo_box_.setJustificationType(juce::Justification::centredLeft);
    combo_box_.onChange = [this] {
        auto const selected_id = combo_box_.getSelectedId();
        auto at = WAVEFORMS.find(selected_id);
        if (at != WAVEFORMS.end())
        {
            this->on_change.emit(at->second.command_name);
        }
    };

    for (auto const &wf : WAVEFORMS)
    {
        combo_box_.addItem(wf.second.display_name, wf.first);
    }
    combo_box_.setSelectedId(initial_selection, juce::dontSendNotification);
}

void WaveformSelect::resized()
{
    combo_box_.setBounds(this->getLocalBounds());
}

// ================

// TODO make a free function, RESOLUTION can be a template parameter you provide.
// but honestly you are probably going to remove this and go straight from modulator to
// juce path in the paint function.
auto WaveformBox::generate_samples(xen::Modulator const &modulator)
    -> std::array<float, RESOLUTION>
{
    auto samples = std::array<float, RESOLUTION>{};

    for (std::size_t i = 0; i < RESOLUTION; ++i)
    {
        // TODO RESOLUTION -1 gives [0, 1] but do you really want [0, 1) instead?
        float t = static_cast<float>(i) / static_cast<float>(RESOLUTION - 1);
        samples[i] = modulator(t);
    }
    return samples;
}

// TODO this can be free fn with template on N, any others that can do this?
auto WaveformBox::generate_waveform_path(std::array<float, RESOLUTION> const &samples)
    -> juce::Path
{
    // samples are assumed to be in range x: [0, 1], y: [-1, 1]
    // We need to map y to [0, 1] for drawing
    auto path = juce::Path{};

    for (std::size_t i = 0; i < RESOLUTION; ++i)
    {
        float x = static_cast<float>(i) / static_cast<float>(RESOLUTION - 1);
        float y = 0.5f - (samples[i] / 2.f); // Normalize to [0, 1]

        if (i == 0)
        {
            path.startNewSubPath(x, y);
        }
        else
        {
            path.lineTo(x, y);
        }
    }

    return path;
}

// ================

auto WaveformBox::waveform_a() const -> Waveform const &
{
    return waveforms_[WAVE_A_INDEX];
}

auto WaveformBox::waveform_b() const -> Waveform const &
{
    return waveforms_[WAVE_B_INDEX];
}

void WaveformBox::set_waveform_a(std::string const &waveform_cmd_name)
{
    auto &wave = waveforms_[WAVE_A_INDEX];
    wave.cmd_name = waveform_cmd_name;
    auto const modulator = make_modulator(wave.cmd_name, wave.frequency, wave.offset);
    wave.samples = generate_samples(modulator);
    this->set_lerp(lerp_); // recalculates lerp and repaints
    this->on_commit();
}

void WaveformBox::set_waveform_b(std::string const &waveform_cmd_name)
{
    auto &wave = waveforms_[WAVE_B_INDEX];
    wave.cmd_name = waveform_cmd_name;
    auto const modulator = make_modulator(wave.cmd_name, wave.frequency, wave.offset);
    wave.samples = generate_samples(modulator);
    this->set_lerp(lerp_); // recalculates lerp and repaints
    this->on_commit();
}

void WaveformBox::set_lerp(float lerp)
{
    lerp_ = lerp;
    waveform_lerp_samples_ =
        lerp_samples(this->waveform_a().samples, this->waveform_b().samples, lerp_);
    this->repaint();
    this->on_change();
}

void WaveformBox::paint(juce::Graphics &g)
{
    auto bounds = this->getLocalBounds().toFloat();

    // Fill background
    g.fillAll(juce::Colours::black);

    // Draw grid
    draw_grid(g, bounds, grid_color_, frequency_grid_values_, offset_grid_values_);

    // Draw border
    g.setColour(grid_color_);
    g.setOpacity(0.5f);
    auto const border_width = 1.f;
    g.drawRect(bounds, border_width);

    bounds = bounds.reduced(border_width); // paint within border space

    auto const &wave_a = this->waveform_a();
    auto const &wave_b = this->waveform_b();

    { // Waveform A
        auto const path = generate_waveform_path(wave_a.samples);
        g.setColour(wave_a.color.withAlpha(0.625f * std::pow(1.f - lerp_, 0.385f)));
        auto const stroke_width = 1.f;
        auto const inner = bounds.reduced(stroke_width * 0.5f);
        g.strokePath(path, juce::PathStrokeType(stroke_width),
                     juce::AffineTransform::scale(inner.getWidth(), inner.getHeight())
                         .translated(inner.getX(), inner.getY()));
    }

    { // Waveform B
        auto const path = generate_waveform_path(wave_b.samples);
        g.setColour(wave_b.color.withAlpha(0.625f * std::pow(lerp_, 0.385f)));
        auto const stroke_width = 1.f;
        auto const inner = bounds.reduced(stroke_width * 0.5f);
        g.strokePath(path, juce::PathStrokeType(stroke_width),
                     juce::AffineTransform::scale(inner.getWidth(), inner.getHeight())
                         .translated(inner.getX(), inner.getY()));
    }

    { // Third - Linear Interpolation
        auto const path = generate_waveform_path(waveform_lerp_samples_);
        auto gradient = juce::ColourGradient{
            wave_a.color, bounds.getX(),     bounds.getCentreY(),
            wave_b.color, bounds.getRight(), bounds.getCentreY(),
            false,
        };
        auto const blend_width = std::min({lerp_, 1.f - lerp_, 0.4f});
        auto const left_stop = std::clamp((1.f - lerp_) - blend_width * 0.5f, 0.f, 1.f);
        auto const right_stop =
            std::clamp((1.f - lerp_) + blend_width * 0.5f, 0.f, 1.f);
        gradient.clearColours();
        gradient.addColour(0.f, wave_a.color);
        gradient.addColour(left_stop, wave_a.color);
        gradient.addColour(right_stop, wave_b.color);
        gradient.addColour(1.f, wave_b.color);

        g.setGradientFill(gradient);

        auto const stroke_width = 3.f;
        auto const inner = bounds.reduced(stroke_width * 0.5f);
        g.strokePath(path, juce::PathStrokeType(stroke_width),
                     juce::AffineTransform::scale(inner.getWidth(), inner.getHeight())
                         .translated(inner.getX(), inner.getY()));
    }

    { // Mouse Handle - Waveform A
        auto const handle =
            get_handle_position(wave_a.frequency, wave_a.offset, bounds);

        g.setColour(wave_a.color);
        g.fillEllipse(handle.x - HANDLE_RADIUS, handle.y - HANDLE_RADIUS,
                      HANDLE_RADIUS * 2.f, HANDLE_RADIUS * 2.f);
    }

    { // Mouse Handle - Waveform A
        auto const handle =
            get_handle_position(wave_b.frequency, wave_b.offset, bounds);
        g.setColour(wave_b.color);
        g.fillEllipse(handle.x - HANDLE_RADIUS, handle.y - HANDLE_RADIUS,
                      HANDLE_RADIUS * 2.f, HANDLE_RADIUS * 2.f);
    }
}

void WaveformBox::mouseDown(juce::MouseEvent const &e)
{
    // Change selected handle
    if (e.mods.isLeftButtonDown())
    {
        is_dragging_ = false;
        auto bounds =
            this->getLocalBounds().toFloat().reduced(1.f); // TODO magic number 1

        auto const &non_selected = waveforms_[1 - selected_waveform_];
        auto non_selected_pos =
            get_handle_position(non_selected.frequency, non_selected.offset, bounds);

        // If non-selected is under the mouse
        if (is_within_target(e.position, non_selected_pos, HANDLE_RADIUS))
        {
            auto const &selected = waveforms_[selected_waveform_];
            auto selected_pos =
                get_handle_position(selected.frequency, selected.offset, bounds);
            if (not is_within_target(e.position, selected_pos, HANDLE_RADIUS))
            {
                // change selection
                selected_waveform_ = 1 - selected_waveform_;
            }
        }
    }
}

void WaveformBox::mouseDrag(juce::MouseEvent const &e)
{
    // TODO you might need to reduce the bounds to get accurate results that match
    // painting, but maybe not.
    if (e.mods.isLeftButtonDown())
    {
        is_dragging_ = true;
        auto &wave = waveforms_[selected_waveform_];
        auto const bounds = this->getLocalBounds().toFloat();
        if (not e.mods.isShiftDown())
        {
            wave.frequency = 4 * (e.position.x - bounds.getX()) / bounds.getWidth();
            wave.frequency = std::clamp(wave.frequency, 0.0001f, 4.f);
        }
        if (not e.mods.isCtrlDown())
        {
            wave.offset = 0.5f - ((e.position.y - bounds.getY()) / bounds.getHeight());
            wave.offset = std::clamp(wave.offset, -0.5f, +0.5f);
        }
        wave.samples = generate_samples(
            make_modulator(wave.cmd_name, wave.frequency, wave.offset));
        this->set_lerp(lerp_); // recalculates lerp and repaints
    }
}

void WaveformBox::mouseUp(juce::MouseEvent const &e)
{
    if (e.mods.isLeftButtonDown())
    {
        if (not is_dragging_)
        {
            // Snap to Grid
            auto const bounds = this->getLocalBounds().toFloat();

            auto frequency = 4 * (e.position.x - bounds.getX()) / bounds.getWidth();
            frequency = std::clamp(frequency, 0.0001f, 4.f);

            auto offset = 0.5f - ((e.position.y - bounds.getY()) / bounds.getHeight());
            offset = std::clamp(offset, -0.5f, +0.5f);

            auto &wave = waveforms_[selected_waveform_];

            wave.frequency = closest_value(frequency_grid_values_, frequency);
            wave.offset = closest_value(offset_grid_values_, offset);

            wave.samples = generate_samples(
                make_modulator(wave.cmd_name, wave.frequency, wave.offset));

            this->set_lerp(lerp_); // recalculates lerp and repaints
        }
        this->on_commit();
    }
}

// ================

WaveformDestination::WaveformDestination(juce::String name)
    : label_{"", std::move(name)}, value_{{.initial = 0.f, .min = -1.f, .max = 1.f}}
{
    this->addAndMakeVisible(label_);
    this->addAndMakeVisible(value_);

    label_.setJustificationType(juce::Justification::centredLeft);
    label_.setColour(juce::Label::textColourId, juce::Colours::white);

    value_.on_change.connect([this](float x) { this->on_change(x); });
    value_.on_release.connect([this] { this->on_commit(); });
}

void WaveformDestination::resized()
{
    auto bounds = this->getLocalBounds().reduced(4);

    auto fb = juce::FlexBox{};
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.items.add(juce::FlexItem{label_}.withWidth(80.f));
    fb.items.add(juce::FlexItem{value_}.withFlex(1.f).withMargin(
        juce::FlexItem::Margin{0.f, 6.f, 0.f, 6.f}));
    fb.performLayout(bounds);
}

// ================

WaveformDestinations::WaveformDestinations()
{
    this->addAndMakeVisible(velocity);
    this->addAndMakeVisible(weight);
    this->addAndMakeVisible(delay);
    this->addAndMakeVisible(gate);
}

void WaveformDestinations::resized()
{
    auto bounds = this->getLocalBounds().reduced(4);
    auto fb = juce::FlexBox{};
    fb.flexDirection = juce::FlexBox::Direction::column;

    fb.items.add(juce::FlexItem{velocity}.withHeight(23.f));
    fb.items.add(juce::FlexItem{weight}.withHeight(23.f));
    fb.items.add(juce::FlexItem{delay}.withHeight(23.f));
    fb.items.add(juce::FlexItem{gate}.withHeight(23.f));
    // TODO pitch

    fb.performLayout(bounds);
}

// ================

ModulationPane::ModulationPane()
    : waveform_a_selector_{1},
      waveform_lerp_slider_{{.initial = 0.f, .min = 0.f, .max = 1.f},
                            juce::Slider::LinearHorizontal},
      waveform_b_selector_{2}
{
    this->addAndMakeVisible(waveform_box_);
    this->addAndMakeVisible(waveform_a_selector_);
    this->addAndMakeVisible(waveform_lerp_slider_);
    this->addAndMakeVisible(waveform_b_selector_);
    this->addAndMakeVisible(destinations_);

    waveform_a_selector_.on_change.connect([this](std::string const &cmd_name) {
        waveform_box_.set_waveform_a(cmd_name);
    });
    waveform_b_selector_.on_change.connect([this](std::string const &cmd_name) {
        waveform_box_.set_waveform_b(cmd_name);
    });

    // TODO you should probably hold state in this parent class and handle cmd
    // generation etc.. all here and orchestrate on change and on commit from here.
    // depending on signal connection order isn't great.
    waveform_lerp_slider_.on_change.connect(
        [this](float value) { waveform_box_.set_lerp(value); });
    waveform_lerp_slider_.on_release.connect([this] { waveform_box_.on_commit(); });

    waveform_box_.set_waveform_a(WAVEFORMS.at(1).command_name);
    waveform_box_.set_waveform_b(WAVEFORMS.at(2).command_name);

    waveform_box_.on_change.connect(
        [this] { this->emit_all_active_destination_cmds(); });
    waveform_box_.on_commit.connect([this] { this->on_change("commit"); });

    destinations_.velocity.on_change.connect([this](float amp) {
        this->on_change(this->generate_command_string("velocity", amp));
    });
    destinations_.velocity.on_commit.connect([this] { this->on_change("commit"); });

    destinations_.weight.on_change.connect([this](float amp) {
        this->on_change(this->generate_command_string("weights", amp));
    });
    destinations_.weight.on_commit.connect([this] { this->on_change("commit"); });

    destinations_.delay.on_change.connect([this](float amp) {
        this->on_change(this->generate_command_string("delay", amp));
    });
    destinations_.delay.on_commit.connect([this] { this->on_change("commit"); });

    destinations_.gate.on_change.connect([this](float amp) {
        this->on_change(this->generate_command_string("gate", amp));
    });
    destinations_.gate.on_commit.connect([this] { this->on_change("commit"); });

    // TODO pitch

    // TODO temp
    // this->on_change.connect([](std::string const &cmd) {
    //     std::cerr << cmd << '\n' << std::endl;
    // });
}

void ModulationPane::resized()
{
    auto bounds = this->getLocalBounds().reduced(10);

    // TODO make outer fb

    auto combo_width = bounds.getWidth() / 4.f;
    auto top_fb = juce::FlexBox{};

    top_fb.flexDirection = juce::FlexBox::Direction::row;
    top_fb.items.add(juce::FlexItem{waveform_a_selector_}.withWidth(combo_width));
    top_fb.items.add(juce::FlexItem{waveform_lerp_slider_}.withFlex(1.f).withMargin(
        juce::FlexItem::Margin{0.f, 6.f, 0.f, 6.f}));
    top_fb.items.add(juce::FlexItem{waveform_b_selector_}.withWidth(combo_width));

    top_fb.performLayout(bounds.withHeight(23.f));

    // Waveform Display
    auto width = bounds.getWidth();
    waveform_box_.setBounds(bounds.withHeight(width / 2).withY(bounds.getY() + 30.f));

    // Destinations
    destinations_.setBounds(
        bounds.withY(bounds.getY() + width / 2 + 40.f).withHeight(100.f));
}

// TODO add user controlled bias into this, probably as its own modulator, then add a
// clamp modulator to whatever is required, probably provided as a parameter as well.
auto ModulationPane::generate_json(float amplitude, float bias) -> std::string
{
    auto const &wave_a = waveform_box_.waveform_a();
    auto const &wave_b = waveform_box_.waveform_b();
    return nlohmann::json{
        {"type", "chain"},
        {"children",
         std::array{
             nlohmann::json{
                 {"type", "blend"},
                 {"children",
                  std::array{
                      // Waveform A
                      nlohmann::json{
                          {"type", "chain"},
                          {"children",
                           std::array{
                               nlohmann::json{
                                   {"type", wave_a.cmd_name},
                                   {"frequency", wave_a.frequency},
                                   {"phase", wave_a.offset},
                               },
                               nlohmann::json{
                                   {"type", "scale"},
                                   {"factor", 1.f - waveform_box_.lerp()},
                               },
                           }},
                      },
                      // Waveform B
                      nlohmann::json{
                          {"type", "chain"},
                          {"children",
                           std::array{
                               nlohmann::json{
                                   {"type", wave_b.cmd_name},
                                   {"frequency", wave_b.frequency},
                                   {"phase", wave_b.offset},
                               },
                               nlohmann::json{
                                   {"type", "scale"},
                                   {"factor", waveform_box_.lerp()},
                               },
                           }},
                      },
                  }},
             },
             nlohmann::json{
                 {"type", "scale"},
                 {"factor", amplitude},
             },
             nlohmann::json{
                 {"type", "bias"},
                 {"amount", bias},
             },
         }},
    }
        .dump();
}

auto ModulationPane::generate_command_string(std::string const &destination,
                                             float amplitude) -> std::string
{
    static auto const SCALE_BIAS_MAP = [] {
        struct ScaleBias
        {
            float scale, bias;
        };
        return std::map<std::string_view, ScaleBias>{
            {"velocity", {0.5f, 0.5f}},
            {"weights", {0.49f, 0.51f}},
            {"delay", {0.5f, 0.5f}},
            {"gate", {0.5f, 0.5f}},
        };
    }();

    auto const sb = SCALE_BIAS_MAP.at(destination);
    return "set " + destination + ' ' +
           this->generate_json(amplitude * sb.scale, sb.bias) + ';';
}

void ModulationPane::emit_all_active_destination_cmds()
{
    auto cmd_str = std::string{};
    if (auto const amp = destinations_.velocity.get_value(); amp)
    {
        cmd_str += this->generate_command_string("velocity", *amp);
    }
    if (auto const amp = destinations_.weight.get_value(); amp)
    {
        cmd_str += this->generate_command_string("weights", *amp);
    }
    if (auto const amp = destinations_.delay.get_value(); amp)
    {
        cmd_str += this->generate_command_string("delay", *amp);
    }
    if (auto const amp = destinations_.gate.get_value(); amp)
    {
        cmd_str += this->generate_command_string("gate", *amp);
    }
    // TODO pitch
    if (not cmd_str.empty())
    {
        this->on_change(cmd_str);
    }
}

} // namespace xen::gui