#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <xen/gui/modulation_slider.hpp>
#include <xen/gui/sequence_bank.hpp>
#include <xen/gui/tab_group.hpp>
#include <xen/gui/xen_slider.hpp>
#include <xen/modulator.hpp>

namespace xen::gui
{

/// A combo box that has various LFO type waveshapes in it.
class WaveshapeSelect : public juce::Component
{
  public:
    /// Modulator fn(float frequency, float offset)
    using MakeModulatorFn = std::function<Modulator(float, float)>;

    struct WaveshapeMetadata
    {
        std::string display_name;
        MakeModulatorFn make_modulator;
    };

    // TODO add to this list
    // <ComboBox ID, Metadata>, ID cannot be zero.
    inline static auto const WAVESHAPES = std::map<int, WaveshapeMetadata>{
        {1,
         {
             .display_name = "Sine",
             .make_modulator =
                 [](float frequency, float offset) {
                     return modulator::Sine{
                         .frequency = frequency,
                         .amplitude = 1.f,
                         .phase = offset,
                     };
                 },
         }},
        {2,
         {
             .display_name = "Triangle",
             .make_modulator =
                 [](float frequency, float offset) {
                     return modulator::Triangle{
                         .frequency = frequency,
                         .amplitude = 1.f,
                         .phase = offset,
                     };
                 },
         }},
        {3,
         {
             .display_name = "SawtoothUp",
             .make_modulator =
                 [](float frequency, float offset) {
                     return modulator::SawtoothUp{
                         .frequency = frequency,
                         .amplitude = 1.f,
                         .phase = offset,
                     };
                 },
         }},
        {4,
         {
             .display_name = "SawtoothDown",
             .make_modulator =
                 [](float frequency, float offset) {
                     return modulator::SawtoothDown{
                         .frequency = frequency,
                         .amplitude = 1.f,
                         .phase = offset,
                     };
                 },
         }},
        {5,
         {
             .display_name = "Square",
             .make_modulator =
                 [](float frequency, float offset) {
                     return modulator::Square{
                         .frequency = frequency,
                         .amplitude = 1.f,
                         .phase = offset,
                         .pulse_width = 0.5f,
                     };
                 },
         }},
    };

  public:
    /// Emits the MakeModulatorFn of the newly selected waveform.
    sl::Signal<void(MakeModulatorFn const &)> on_change;

  public:
    /// initial_selection_id must be an ID key from WAVESHAPES map.
    WaveshapeSelect(int initial_selection_id);

    [[nodiscard]]
    auto get_selected_fn() const -> MakeModulatorFn;

    [[nodiscard]]
    auto get_selected_id() const -> int;

    /// Sets the current selection to \p id which should only come from the
    /// `get_selected_id` function. This will emit the `on_change` signal.
    void set_selected_id(int id);

  public:
    void resized() override;

  private:
    juce::ComboBox combo_box_;
};

/// A UI element containing two waveform displays (A and B) and their linear interp.
class WaveformBox : public juce::Component
{
  private:
    inline static auto const GRID_COLOR = juce::Colour{juce::Colours::grey};
    static constexpr auto MIN_FREQ = 0.1f;
    static constexpr auto MAX_FREQ = 10.f;

    static constexpr auto HANDLE_RADIUS = 4.f;

    // array of pair<frequency, pixel thickness>
    static constexpr auto FREQUENCY_GRID_VALUES =
        std::array<std::pair<float, float>, 12>{{
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
    static constexpr auto OFFSET_GRID_VALUES = std::array<std::pair<float, float>, 7>{{
        {-0.5f, 0.f},
        {-0.25f, 1.5f},
        {0.f, 2.f},
        {0.25f, 1.5f},
        {1.f / 3.f, 1.f},
        {-1.f / 3.f, 1.f},
        {0.5f, 0.f},
    }};

  public:
    struct Waveform
    {
        float frequency; // [0, inf)
        float offset;
        WaveshapeSelect::MakeModulatorFn make_modulator_fn;
        Modulator modulator;
        juce::Path path; // normalized 0..1
        juce::Colour color;
    };

    Waveform wave_a;
    Waveform wave_b;

    Modulator lerp_modulator;
    juce::Path lerp_path;

    /// Emits on frequency, offset, lerp or waveshape change.
    sl::Signal<void()> on_change;

    /// Emits on mouse release events after on_change, to signal a cmd system commit.
    sl::Signal<void()> on_commit;

  public:
    WaveformBox(WaveshapeSelect::MakeModulatorFn const &waveshape_a,
                WaveshapeSelect::MakeModulatorFn const &waveshape_b);

    /**
     * Change the waveshape for waveform A; redraw waveform A and LERP wave.
     * \p mk_mod_fn A fn to generate a modulator that represents a waveshape.
     */
    void set_waveshape_a(WaveshapeSelect::MakeModulatorFn const &mk_mod_fn);

    /**
     * Change the waveshape for waveform B; redraw waveform B and LERP wave.
     * \p mk_mod_fn A fn to generate a modulator that represents a waveshape.
     */
    void set_waveshape_b(WaveshapeSelect::MakeModulatorFn const &mk_mod_fn);

    void set_lerp(float lerp);

    /// Return the current lerp value [0, 1]. 0 is all Wave A, 1 is all Wave B.
    [[nodiscard]]
    auto lerp() const -> float
    {
        return lerp_;
    }

  public:
    void paint(juce::Graphics &g) override;

    void mouseDown(juce::MouseEvent const &e) override;

    void mouseDrag(juce::MouseEvent const &e) override;

    void mouseUp(juce::MouseEvent const &e) override;

  private:
    /// Update modulator and path for the given wave assuming frequency, offset, or
    /// make_modulator_fn has been updated already. This also update lerp_modulator and
    /// lerp_path.
    void update_calculated_state(Waveform &waveform);

  private:
    float lerp_{0.f};             // 0 = all A, 1 = all B
    bool wave_a_selected_ = true; // false is wave b is selected
    bool is_dragging_ = false;
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
        {.bias_min = 0.f, .bias_max = 1.f},
    };
    LFOModulationSlider weight{
        "Weight",
        {.bias_min = 0.05f, .bias_max = 2.f},
    };
    LFOModulationSlider delay{
        "Delay",
        {.bias_min = 0.f, .bias_max = 1.f},
    };
    LFOModulationSlider gate{
        "Gate",
        {.bias_min = 0.f, .bias_max = 1.f},
    };
    LFOModulationSlider pitch{
        "Pitch",
        {.bias_min = -4.f, .bias_max = 4.f},
    };
};

class ModulationWindow : public juce::Component
{
  public:
    sl::Signal<void(std::string const &)> on_change; // Emits command string

  public:
    ModulationWindow();

  public:
    void update(std::size_t tuning_length);

  public:
    void resized() override;

  private:
    WaveshapeSelect waveshape_a_selector_{1};

    XenSlider waveshape_lerp_slider_{
        {.initial = 0.f, .min = 0.f, .max = 1.f},
        juce::Slider::LinearHorizontal,
    };

    WaveshapeSelect waveshape_b_selector_{2};

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
    auto generate_command_string(std::string const &destination, float bias,
                                 float scale, float min, float max) const
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

  private:
    std::size_t tuning_length_ = 12;
};

/// A tab group a four modulation windows
class ModulationPane : public TabGroup<ModulationWindow, ModulationWindow,
                                       ModulationWindow, ModulationWindow>
{
  public:
    sl::Signal<void(std::string const &)> on_change; // Emits command string

  public:
    ModulationPane();

  public:
    void update(std::size_t tuning_length);
};

} // namespace xen::gui