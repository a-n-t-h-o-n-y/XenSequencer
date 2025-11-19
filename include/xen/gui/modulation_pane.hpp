#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <xen/gui/modulation_slider.hpp>
#include <xen/gui/sequence_bank.hpp>
#include <xen/gui/xen_slider.hpp>
#include <xen/modulator.hpp>

namespace xen::gui
{

/// A combo box that has various LFO type waveforms in it.
class WaveformSelect : public juce::Component
{
  public:
    /// Emits json recognized command name of waveform/modulator selected.
    sl::Signal<void(std::string const &)> on_change;

  public:
    /// initial_selection cannot be 0.
    WaveformSelect(int initial_selection);

  public:
    void resized() override;

  private:
    juce::ComboBox combo_box_;
};

/// A UI element containing two waveform displays (A and B) and their linear interp.
class WaveformBox : public juce::Component
{
  private:
    // TODO what is the best value for this? test it, it depends on the sise of the boks
    // TODO move this to fn implementation or source file, only if samples array is no
    // longer used.
    static constexpr int RESOLUTION = 500;

  public:
    struct Waveform
    {
        float frequency; // [0, inf)
        float offset;
        std::string cmd_name;
        juce::Colour color;
        std::array<float, RESOLUTION> samples; // TODO remove once waves reimplemented
    };

    /// Emits on frequency, offset, lerp or waveshape change.
    sl::Signal<void()> on_change;

    /// Emits on mouse release events after on_change, to signal a cmd system commit.
    sl::Signal<void()> on_commit;

  public:
    // TODO remove this?
    void set_grid_color(juce::Colour c)
    {
        grid_color_ = c;
    }

    /// Update and redraw waveform A and LERP wave.
    void set_waveform_a(std::string const &waveform_cmd_name);

    /// Update and redraw waveform B and LERP wave.
    void set_waveform_b(std::string const &waveform_cmd_name);

    void set_lerp(float lerp);

    /// Return the current lerp value [0, 1]. 0 is all Wave A, 1 is all Wave B.
    [[nodiscard]]
    auto lerp() const -> float
    {
        return lerp_;
    }

    [[nodiscard]]
    auto waveform_a() const -> Waveform const &;

    [[nodiscard]]
    auto waveform_b() const -> Waveform const &;

  public:
    void paint(juce::Graphics &g) override;

    void mouseDown(juce::MouseEvent const &e) override;

    void mouseDrag(juce::MouseEvent const &e) override;

    void mouseUp(juce::MouseEvent const &e) override;

  private:
    [[nodiscard]]
    auto generate_samples(xen::Modulator const &modulator)
        -> std::array<float, RESOLUTION>;

    [[nodiscard]]
    auto generate_waveform_path(std::array<float, RESOLUTION> const &samples)
        -> juce::Path;

  private:
    std::array<float, RESOLUTION> waveform_lerp_samples_{};

    juce::Colour grid_color_{juce::Colours::grey};
    float lerp_{0.f}; // 0 = all A, 1 = all B
    static constexpr float MIN_FREQ = 0.1f;
    static constexpr float MAX_FREQ = 10.f;

    static constexpr std::size_t WAVE_A_INDEX = 0;
    static constexpr std::size_t WAVE_B_INDEX = 1;
    static constexpr float HANDLE_RADIUS = 4.f;
    std::array<Waveform, 2> waveforms_{{
        {
            .frequency = 1.f,
            .offset = 0.f,
            .cmd_name = "sine",
            .color = juce::Colour{0xFF61BAC0},
            .samples = {},
        },
        {
            .frequency = 1.f,
            .offset = 0.25f,
            .cmd_name = "triangle",
            .color = juce::Colour{0xFF9D83C5},
            .samples = {},
        },
    }};

    std::size_t selected_waveform_{WAVE_A_INDEX}; // current mouse selection

    bool is_dragging_ = false;

    // array of pair<frequency, pixel thickness>
    std::array<std::pair<float, float>, 12> frequency_grid_values_ = {{
        {MIN_FREQ, 0.f},
        {0.25f, 1.5f},
        {0.5f, 1.5f},
        {0.75f, 1.5f},
        {1.f / 3.f, 1.f},
        {2.f / 3.f, 1.f},
        {1.f, 2.f},
        {1.5f, 1.5f},
        {2.f, 1.5f},
        {4.f, 1.5f},
        {8.f, 1.5f},
        {MAX_FREQ, 0.f},
    }};

    // array of pair<frequency, pixel thickness>
    std::array<std::pair<float, float>, 7> offset_grid_values_ = {{
        {-0.5f, 0.f},
        {-0.25f, 1.5f},
        {0.f, 2.f},
        {0.25f, 1.5f},
        {1.f / 3.f, 1.f},
        {-1.f / 3.f, 1.f},
        {0.5f, 0.f},
    }};
};

class WaveformDestinations : public juce::Component
{
  public:
    WaveformDestinations();

  public:
    void resized() override;

  public:
    LFOModulationSlider velocity{
        "Velocity",
        {.bias_min = 0.f,
         .bias_max = 1.f,
         .initial_bias = 0.5f,
         .initial_amplitude = 0.f},
    };
    LFOModulationSlider weight{
        "Weight",
        {.bias_min = 0.05f,
         .bias_max = 2.f,
         .initial_bias = 1.025f, // TODO can this calculation be done automatically as a
                                 // default if this is an optional null?
         .initial_amplitude = 0.f},
    };
    LFOModulationSlider delay{
        "Delay",
        {.bias_min = 0.f,
         .bias_max = 1.f,
         .initial_bias = 0.5f,
         .initial_amplitude = 0.f},
    };
    LFOModulationSlider gate{
        "Gate",
        {.bias_min = 0.f,
         .bias_max = 1.f,
         .initial_bias = 0.5f,
         .initial_amplitude = 0.f},
    };
    LFOModulationSlider pitch{
        "Pitch",
        {.bias_min = -4.f * 12.f,
         .bias_max = 4.f * 12.f,
         .initial_bias = 0.f,
         .initial_amplitude = 0.f},
    };
};

class ModulationPane : public juce::Component
{
  public:
    sl::Signal<void(std::string const &)> on_change; // Emits command string

  public:
    ModulationPane();

  public:
    void resized() override;

  private:
    WaveformSelect waveform_a_selector_;
    XenSlider waveform_lerp_slider_;
    WaveformSelect waveform_b_selector_;

    WaveformBox waveform_box_;

    WaveformDestinations destinations_;

  private:
    /**
     * Builds a full command string to modify a single destination (velocity, weight...)
     * @details For something like WaveformBox on_change you can call this multiple
     * times, it already has a semi-colon for concat, then commit the results if needed
     * with a separate `commit` command.
     */
    [[nodiscard]]
    auto generate_command_string(std::string const &destination, float user_scale,
                                 float user_bias, float min, float max) const
        -> std::string;

    /// Emit on_change cmd string for each of the destinations that is active.
    void emit_all_active_destination_cmds();

    /**
     * This is the Modulator sent to each destination.
     * @param bias The combination of the destination and user input biases.
     * @param scale The combination of the destination and user input scales.
     * @param min The minimum to clamp to.
     * @param max The maximum to clamp to.
     */
    [[nodiscard]]
    auto build_destination_modulator(float bias, float scale, float min,
                                     float max) const -> Modulator;
};

} // namespace xen::gui