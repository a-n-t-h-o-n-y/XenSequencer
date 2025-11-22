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

/**
 * @brief Convert normalized horizontal position to log-spaced frequency.
 * @param t Normalized position in [0, 1].
 * @param min_freq Minimum frequency (> 0).
 * @param max_freq Maximum frequency (> min_freq).
 * @return Frequency corresponding to t.
 * @invariant min_freq > 0
 * @invariant max_freq > min_freq
 * @invariant 0 <= t <= 1
 */
[[nodiscard]]
auto norm_to_freq(float t, float min_freq, float max_freq) -> float
{
    return min_freq * std::pow(max_freq / min_freq, t);
}

/**
 * @brief Convert a frequency to normalized log-space position.
 * @param freq Frequency value in range [min_freq, max_freq].
 * @param min_freq Minimum frequency (> 0).
 * @param max_freq Maximum frequency (> min_freq).
 * @return Normalized position t in [0, 1].
 * @invariant min_freq > 0
 * @invariant max_freq > min_freq
 * @invariant min_freq <= freq <= max_freq
 */
[[nodiscard]]
auto freq_to_norm(float freq, float min_freq, float max_freq) -> float
{
    return std::log(freq / min_freq) / std::log(max_freq / min_freq);
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
auto get_handle_position(float frequency, float min_freq, float max_freq, float offset,
                         juce::Rectangle<float> const &bounds) -> juce::Point<float>
{
    auto const t = freq_to_norm(frequency, min_freq, max_freq);
    return {
        bounds.getX() + t * bounds.getWidth(),
        bounds.getY() + (0.5f - offset) * bounds.getHeight(),
    };
}

/// Find and return the closes value in \p values to \p target
template <std::size_t N>
[[nodiscard]]
auto closest_value(std::array<std::pair<float, float>, N> const &values, float target)
    -> float
{
    auto closest = values.front().first;
    auto min_diff = std::numeric_limits<float>::max();

    for (auto const &v : values)
    {
        auto const diff = std::abs(v.first - target);
        if (diff < min_diff)
        {
            min_diff = diff;
            closest = v.first;
        }
    }

    return closest;
}

template <std::size_t N, std::size_t M>
void draw_grid(juce::Graphics &g, juce::Rectangle<float> bounds, juce::Colour color,
               std::array<std::pair<float, float>, N> const &frequency_grid_values,
               std::array<std::pair<float, float>, M> const &offset_grid_values,
               float min_freq, float max_freq)
{
    g.setColour(color);
    g.setOpacity(0.25f);

    const auto w = bounds.getWidth();
    const auto h = bounds.getHeight();

    // vertical
    for (auto const frequency : frequency_grid_values)
    {
        const float line_thickness = frequency.second;
        auto const t = freq_to_norm(frequency.first, min_freq, max_freq);
        auto const x = bounds.getX() + w * t - (line_thickness / 2.f);
        g.fillRect(x, bounds.getY(), line_thickness, h);
    }

    // horizontal
    for (auto const offset : offset_grid_values)
    {
        const float line_thickness = offset.second;
        auto const y =
            bounds.getY() + h * (0.5f - offset.first) - (line_thickness / 2.f);
        g.fillRect(bounds.getX(), y, w, line_thickness);
    }
}

[[nodiscard]]
auto generate_waveform_path(xen::Modulator const &modulator) -> juce::Path
{
    // TODO what is the best value for this? test it, it depends on the size of the box.
    constexpr auto RESOLUTION = 500;

    // TODO make a variant evaluate_buffer() that takes a modulator, a buffer and a step
    // sise and fills the buffer.

    // Samples are assumed to be in range x: [0, 1], y: [-1, 1]
    // We need to map y to [0, 1] for drawing
    auto path = juce::Path{};

    for (std::size_t i = 0; i < RESOLUTION; ++i)
    {
        float x = static_cast<float>(i) / static_cast<float>(RESOLUTION - 1);
        float y = 0.5f - (xen::evaluate(modulator, x) / 2.f); // Normalize to [0, 1]

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

[[nodiscard]]
auto build_lerp_modulator(xen::Modulator const &wave_a, xen::Modulator const &wave_b,
                          float lerp) -> xen::Modulator
{
    using namespace xen::modulator;
    return Blend{.children = {
                     Chain{.children =
                               {
                                   wave_a,
                                   Scale{.factor = 1.f - lerp},
                               }},
                     Chain{.children =
                               {
                                   wave_b,
                                   Scale{.factor = lerp},
                               }},
                 }};
}

} // namespace

namespace xen::gui
{

WaveshapeSelect::WaveshapeSelect(int initial_selection)
{
    this->addAndMakeVisible(combo_box_);
    combo_box_.setEditableText(false);
    combo_box_.setJustificationType(juce::Justification::centredLeft);
    combo_box_.onChange = [this] {
        if (auto const selected_id = combo_box_.getSelectedId(); selected_id != 0)
        {
            this->on_change.emit(WAVESHAPES.at(selected_id).make_modulator);
        }
    };

    for (auto const &wf : WAVESHAPES)
    {
        combo_box_.addItem(wf.second.display_name, wf.first);
    }
    combo_box_.setSelectedId(initial_selection, juce::dontSendNotification);
}

auto WaveshapeSelect::get_selected_fn() const -> MakeModulatorFn
{
    auto const id = this->get_selected_id();
    return WAVESHAPES.at(id).make_modulator;
}

auto WaveshapeSelect::get_selected_id() const -> int
{
    return combo_box_.getSelectedId();
}

void WaveshapeSelect::set_selected_id(int id)
{
    combo_box_.setSelectedId(id, juce::sendNotification);
}

void WaveshapeSelect::resized()
{
    combo_box_.setBounds(this->getLocalBounds());
}

// ================

WaveformBox::WaveformBox(WaveshapeSelect::MakeModulatorFn const &waveshape_a,
                         WaveshapeSelect::MakeModulatorFn const &waveshape_b)
                         :
    wave_a{
        .frequency = 1.f,
        .offset = 0.f,
        .make_modulator_fn = waveshape_a,
        .modulator = {},
        .path = {},
        .color = juce::Colour{0xFF61BAC0},
    },
    wave_b{
        .frequency = 1.f,
        .offset = 0.25f,
        .make_modulator_fn = waveshape_b,
        .modulator = {},
        .path = {},
        .color = juce::Colour{0xFF9D83C5},
    }
{
    this->update_calculated_state(wave_a);
    this->update_calculated_state(wave_b);
}

void WaveformBox::set_waveshape_a(WaveshapeSelect::MakeModulatorFn const &mk_mod_fn)
{
    wave_a.make_modulator_fn = mk_mod_fn;
    this->update_calculated_state(wave_a);
    this->repaint();
    this->on_change();
    this->on_commit();
}

void WaveformBox::set_waveshape_b(WaveshapeSelect::MakeModulatorFn const &mk_mod_fn)
{
    wave_b.make_modulator_fn = mk_mod_fn;
    this->update_calculated_state(wave_b);
    this->repaint();
    this->on_change();
    this->on_commit();
}

void WaveformBox::set_lerp(float lerp)
{
    lerp_ = lerp;
    lerp_modulator = build_lerp_modulator(wave_a.modulator, wave_b.modulator, lerp_);
    lerp_path = generate_waveform_path(lerp_modulator);

    this->repaint();
    this->on_change();
}

void WaveformBox::paint(juce::Graphics &g)
{
    auto bounds = this->getLocalBounds().toFloat();

    // Fill background
    g.fillAll(juce::Colours::black);

    // Draw border
    g.setColour(grid_color_);
    auto const border_width = 1.f;
    g.drawRect(bounds, border_width);

    bounds = bounds.reduced(border_width); // paint within border space

    // Draw grid
    draw_grid(g, bounds, grid_color_, FREQUENCY_GRID_VALUES, OFFSET_GRID_VALUES,
              MIN_FREQ, MAX_FREQ);

    auto const stroke_width = 3.f;
    auto const inner = bounds.reduced(stroke_width * 0.5f);

    // Waveform A
    g.setColour(wave_a.color.withAlpha(0.4f * std::pow(1.f - lerp_, 0.385f)));
    g.strokePath(wave_a.path, juce::PathStrokeType(stroke_width),
                 juce::AffineTransform::scale(inner.getWidth(), inner.getHeight())
                     .translated(inner.getX(), inner.getY()));

    // Waveform B
    g.setColour(wave_b.color.withAlpha(0.4f * std::pow(lerp_, 0.385f)));
    g.strokePath(wave_b.path, juce::PathStrokeType(stroke_width),
                 juce::AffineTransform::scale(inner.getWidth(), inner.getHeight())
                     .translated(inner.getX(), inner.getY()));

    { // Linear Interpolation
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
        g.strokePath(lerp_path, juce::PathStrokeType(stroke_width),
                     juce::AffineTransform::scale(inner.getWidth(), inner.getHeight())
                         .translated(inner.getX(), inner.getY()));
    }

    { // Mouse Handle - Waveform A
        auto const handle = get_handle_position(wave_a.frequency, MIN_FREQ, MAX_FREQ,
                                                wave_a.offset, bounds);

        g.setColour(wave_a.color);
        auto const x = handle.x - HANDLE_RADIUS;
        auto const y = handle.y - HANDLE_RADIUS;
        auto const wh = HANDLE_RADIUS * 2.f;
        g.fillEllipse(x, y, wh, wh);
    }

    { // Mouse Handle - Waveform A
        auto const handle = get_handle_position(wave_b.frequency, MIN_FREQ, MAX_FREQ,
                                                wave_b.offset, bounds);
        g.setColour(wave_b.color);
        auto const x = handle.x - HANDLE_RADIUS;
        auto const y = handle.y - HANDLE_RADIUS;
        auto const wh = HANDLE_RADIUS * 2.f;
        g.fillEllipse(x, y, wh, wh);
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

        auto const &non_selected = wave_a_selected_ ? wave_b : wave_a;
        auto non_selected_pos = get_handle_position(
            non_selected.frequency, MIN_FREQ, MAX_FREQ, non_selected.offset, bounds);

        // If non-selected is under the mouse
        if (is_within_target(e.position, non_selected_pos, HANDLE_RADIUS * 3.f))
        {
            auto const &selected = wave_a_selected_ ? wave_a : wave_b;
            auto selected_pos = get_handle_position(selected.frequency, MIN_FREQ,
                                                    MAX_FREQ, selected.offset, bounds);
            if (not is_within_target(e.position, selected_pos, HANDLE_RADIUS * 3.f))
            {
                // change selection
                wave_a_selected_ = !wave_a_selected_;
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
        auto &wave = wave_a_selected_ ? wave_a : wave_b;
        auto const bounds = this->getLocalBounds().toFloat();
        if (not e.mods.isShiftDown())
        {
            wave.frequency = norm_to_freq(
                (e.position.x - bounds.getX()) / bounds.getWidth(), MIN_FREQ, MAX_FREQ);
            wave.frequency = std::clamp(wave.frequency, MIN_FREQ, 4.f * MAX_FREQ);
        }
        if (not e.mods.isCommandDown())
        {
            wave.offset = 0.5f - ((e.position.y - bounds.getY()) / bounds.getHeight());
            wave.offset = std::clamp(wave.offset, -0.5f, +0.5f);
        }
        this->update_calculated_state(wave);
        this->repaint();
        this->on_change();
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

            auto frequency = norm_to_freq(
                (e.position.x - bounds.getX()) / bounds.getWidth(), MIN_FREQ, MAX_FREQ);
            frequency = std::clamp(frequency, MIN_FREQ, 4.f * MAX_FREQ);

            auto offset = 0.5f - ((e.position.y - bounds.getY()) / bounds.getHeight());
            offset = std::clamp(offset, -0.5f, +0.5f);

            auto &wave = wave_a_selected_ ? wave_a : wave_b;

            wave.frequency = closest_value(FREQUENCY_GRID_VALUES, frequency);
            wave.offset = closest_value(OFFSET_GRID_VALUES, offset);

            this->update_calculated_state(wave);
            this->repaint();
            this->on_change();
        }
        this->on_commit();
    }
}

void WaveformBox::update_calculated_state(Waveform &waveform)
{
    waveform.modulator =
        waveform.make_modulator_fn(waveform.frequency, waveform.offset);
    waveform.path = generate_waveform_path(waveform.modulator);

    lerp_modulator = build_lerp_modulator(wave_a.modulator, wave_b.modulator, lerp_);
    lerp_path = generate_waveform_path(lerp_modulator);
}

// ================

WaveformDestinations::WaveformDestinations()
{
    this->addAndMakeVisible(velocity);
    this->addAndMakeVisible(weight);
    this->addAndMakeVisible(delay);
    this->addAndMakeVisible(gate);
    this->addAndMakeVisible(pitch);
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
    fb.items.add(juce::FlexItem{pitch}.withHeight(23.f));

    fb.performLayout(bounds);
}

// ================

ModulationPane::ModulationPane()
    : waveform_box_{waveshape_a_selector_.get_selected_fn(),
                    waveshape_b_selector_.get_selected_fn()}
{
    this->addAndMakeVisible(waveform_box_);
    this->addAndMakeVisible(waveshape_a_selector_);
    this->addAndMakeVisible(waveshape_lerp_slider_);
    this->addAndMakeVisible(waveshape_b_selector_);
    this->addAndMakeVisible(destinations_);

    waveshape_a_selector_.on_change.connect(
        [this](WaveshapeSelect::MakeModulatorFn const &mk_fn) {
            waveform_box_.set_waveshape_a(mk_fn);
        });
    waveshape_b_selector_.on_change.connect(
        [this](WaveshapeSelect::MakeModulatorFn const &mk_fn) {
            waveform_box_.set_waveshape_b(mk_fn);
        });

    // TODO you should probably hold state in this parent class and handle cmd
    // generation etc.. all here and orchestrate on change and on commit from here.
    // depending on signal connection order isn't great.
    waveshape_lerp_slider_.on_change.connect(
        [this](float value) { waveform_box_.set_lerp(value); });
    waveshape_lerp_slider_.on_release.connect([this] { waveform_box_.on_commit(); });

    waveform_box_.on_change.connect(
        [this] { this->emit_all_active_destination_cmds(); });
    waveform_box_.on_commit.connect([this] { this->on_change("commit"); });

    // TODO can this be cleaned up by moving it to Destinations class?
    destinations_.velocity.on_change.connect([this](float bias, float amp) {
        auto const [min, max] = destinations_.velocity.get_bias_range();
        this->on_change(this->generate_command_string("velocity", bias, amp, min, max));
    });
    destinations_.velocity.on_commit.connect([this] { this->on_change("commit"); });

    destinations_.weight.on_change.connect([this](float bias, float amp) {
        auto const [min, max] = destinations_.weight.get_bias_range();
        this->on_change(this->generate_command_string("weights", bias, amp, min, max));
    });
    destinations_.weight.on_commit.connect([this] { this->on_change("commit"); });

    destinations_.delay.on_change.connect([this](float bias, float amp) {
        auto const [min, max] = destinations_.delay.get_bias_range();
        this->on_change(this->generate_command_string("delay", bias, amp, min, max));
    });
    destinations_.delay.on_commit.connect([this] { this->on_change("commit"); });

    destinations_.gate.on_change.connect([this](float bias, float amp) {
        auto const [min, max] = destinations_.gate.get_bias_range();
        this->on_change(this->generate_command_string("gate", bias, amp, min, max));
    });
    destinations_.gate.on_commit.connect([this] { this->on_change("commit"); });

    destinations_.pitch.on_change.connect([this](float bias, float amp) {
        auto const [min, max] = destinations_.pitch.get_bias_range();
        this->on_change(this->generate_command_string("pitch", bias, amp, min, max));
    });
    destinations_.pitch.on_commit.connect([this] { this->on_change("commit"); });
}

void ModulationPane::resized()
{
    auto bounds = this->getLocalBounds().reduced(10);

    // TODO make outer fb

    auto combo_width = bounds.getWidth() / 4.f;
    auto top_fb = juce::FlexBox{};

    top_fb.flexDirection = juce::FlexBox::Direction::row;
    top_fb.items.add(juce::FlexItem{waveshape_a_selector_}.withWidth(combo_width));
    top_fb.items.add(juce::FlexItem{waveshape_lerp_slider_}.withFlex(1.f).withMargin(
        juce::FlexItem::Margin{0.f, 6.f, 0.f, 6.f}));
    top_fb.items.add(juce::FlexItem{waveshape_b_selector_}.withWidth(combo_width));

    top_fb.performLayout(bounds.withHeight(23.f));

    // Waveform Display
    auto width = bounds.getWidth();
    waveform_box_.setBounds(bounds.withHeight(width / 2).withY(bounds.getY() + 30.f));

    // Destinations
    destinations_.setBounds(
        bounds.withY(bounds.getY() + width / 2 + 40.f).withHeight(100.f));
}

auto ModulationPane::generate_command_string(std::string const &destination, float bias,
                                             float scale, float min, float max) const
    -> std::string
{
    auto const mod = build_destination_modulator(bias, scale, min, max);
    return "set " + destination + ' ' + to_json(mod).dump() + ';';
}

void ModulationPane::emit_all_active_destination_cmds()
{
    // TODO can this be put in Destinations as well? Lots of repeated code.
    auto cmd_str = std::string{};
    if (auto const values = destinations_.velocity.get_values(); values)
    {
        auto const &[bias, amp] = *values;
        auto const [min, max] = destinations_.velocity.get_bias_range();
        cmd_str += this->generate_command_string("velocity", bias, amp, min, max);
    }
    if (auto const values = destinations_.weight.get_values(); values)
    {
        auto const &[bias, amp] = *values;
        auto const [min, max] = destinations_.weight.get_bias_range();
        cmd_str += this->generate_command_string("weights", bias, amp, min, max);
    }
    if (auto const values = destinations_.delay.get_values(); values)
    {
        auto const &[bias, amp] = *values;
        auto const [min, max] = destinations_.delay.get_bias_range();
        cmd_str += this->generate_command_string("delay", bias, amp, min, max);
    }
    if (auto const values = destinations_.gate.get_values(); values)
    {
        auto const &[bias, amp] = *values;
        auto const [min, max] = destinations_.gate.get_bias_range();
        cmd_str += this->generate_command_string("gate", bias, amp, min, max);
    }
    if (auto const values = destinations_.pitch.get_values(); values)
    {
        auto const &[bias, amp] = *values;
        auto const [min, max] = destinations_.pitch.get_bias_range();
        cmd_str += this->generate_command_string("pitch", bias, amp, min, max);
    }
    if (not cmd_str.empty())
    {
        this->on_change(cmd_str);
    }
}

auto ModulationPane::build_destination_modulator(float bias, float scale, float min,
                                                 float max) const -> Modulator
{
    using namespace xen::modulator;

    return Chain{.children = {
                     waveform_box_.lerp_modulator,
                     Scale{
                         .factor = scale,
                     },
                     Bias{
                         .amount = bias,
                     },
                     Clamp{
                         .min = min,
                         .max = max,
                     },
                 }};
}

} // namespace xen::gui