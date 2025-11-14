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

#include <xen/gui/sequence_bank.hpp>
#include <xen/gui/xen_slider.hpp>
#include <xen/modulator.hpp>

namespace xen::gui
{

// class ModulationButtons : public juce::Component
// {
//   public:
//     sl::Signal<void(std::size_t)> on_index_selected;

//   public:
//     ModulationButtons();

//   public:
//     void resized() override;

//   private:
//     std::array<SequenceSquare, 16> buttons_;
// };

// class ModulationParameters : public juce::Component
// {
//   public:
//     sl::Signal<void()> on_change;
//     sl::Signal<void()> on_commit;

//   public:
//     ModulationParameters(std::string const &mod_type,
//                          std::vector<XenSlider::Metadata> const &slider_data);

//   public:
//     void paint(juce::Graphics &g) override;

//     void resized() override;

//   public:
//     [[nodiscard]]
//     auto get_json() -> nlohmann::json;

//     /**
//      * Return true if the mod_type is empty.
//      */
//     [[nodiscard]]
//     auto empty() -> bool;

//     [[nodiscard]]
//     auto get_type() -> std::string const &;

//   private:
//     std::string type_;
//     std::vector<std::unique_ptr<XenSlider>> sliders_;
// };

// class ModulationPane_Previous : public juce::Component
// {
//   public:
//     sl::Signal<void(std::string const &)> on_change; // Emits command string

//   public:
//     ModulationPane_Previous();

//   public:
//     void resized() override;

//   private:
//     juce::ComboBox target_command_dropdown_;
//     juce::ComboBox modulator_dropdown_;
//     std::array<std::unique_ptr<ModulationParameters>, 16> parameter_uis_;
//     std::size_t current_selection_{0};
//     ModulationButtons buttons_;

//   private:
//     [[nodiscard]]
//     auto generate_json() -> std::string;

//     [[nodiscard]]
//     auto generate_command_string(bool commit) -> std::string;
// };

// ====================================

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
    static constexpr int RESOLUTION = 200;

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

    std::array<float, 25> frequency_grid_values_ = {
        0.f, 0.25f, 0.5f, 0.75f, 1.f / 3.f,       2.f / 3.f,

        1.f, 1.25f, 1.5f, 1.75f, 1.f + 1.f / 3.f, 1.f + 2.f / 3.f,

        2.f, 2.25f, 2.5f, 2.75f, 2.f + 1.f / 3.f, 2.f + 2.f / 3.f,

        3.f, 3.25f, 3.5f, 3.75f, 3.f + 1.f / 3.f, 3.f + 2.f / 3.f,

        4.f,
    };

    std::array<float, 7> offset_grid_values_ = {
        -0.5f, -0.25f, 0.f, 0.25f, 1.f / 3.f, -1.f / 3.f, 0.5f,
    };
};

class WaveformDestination : public juce::Component
{
  public:
    sl::Signal<void(float)> on_change;
    sl::Signal<void()> on_commit;

  public:
    WaveformDestination(juce::String name);

  public:
    void reset(); // TODO

    [[nodiscard]]
    auto get_value() const -> std::optional<float>
    {
        // TODO implement optional
        return std::optional{value_.get_value()};
    }

  public:
    void resized() override;

  private:
    juce::Label label_;
    XenSlider value_;
    bool is_active_;
};

class WaveformDestinations : public juce::Component
{
  public:
    WaveformDestinations();

  public:
    void resized() override;

  public:
    WaveformDestination velocity{"Velocity"};
    WaveformDestination weight{"Weight"};
    WaveformDestination delay{"Delay"};
    WaveformDestination gate{"Gate"};
    // TODO pitch
    // TODO Reset button
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
     * Builds a JSON string to represent the current state of the modulators.
     * @details This can be passed to `set velocity`, `set weight` commands etc...
     * @param amplitude - How much to scale the final output by.
     * @param bias - How much to add to the final output after amplitude.
     */
    [[nodiscard]]
    auto generate_json(float amplitude = 1.f, float bias = 0.f) -> std::string;

    /**
     * Builds a full command string to modify a single destination (velocity, weight...)
     * @details For something like WaveformBox on_change you can call this multiple
     * times, it already has a semi-colon for concat, then commit the results if needed
     * with a separate `commit` command.
     */
    [[nodiscard]]
    auto generate_command_string(std::string const &destination, float amplitude)
        -> std::string;

    /// Emit on_change cmd string for each of the destinations that is active.
    void emit_all_active_destination_cmds();
};

// TODO add waves to the combos, then figure on change of combos or lerp, redraw the
// waveform display. To draw the waveform display you have frequency and offset within
// the waveform display component.

// TODO then hook up any change to emit the proper command string with json

} // namespace xen::gui