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

#include <xen/gui/sequence_bank.hpp> // TODO temp

namespace xen::gui
{

class ModulationButtons : public juce::Component
{
  public:
    sl::Signal<void(std::size_t)> on_index_selected;

  public:
    ModulationButtons();

  public:
    void resized() override;

  private:
    std::array<SequenceSquare, 16> buttons_;
};

class ModulationParameters : public juce::Component
{
  public:
    struct SliderMetadata
    {
        std::string id;
        std::string display_name;
        float initial;
        float min;
        float max;
        std::optional<float> midpoint = std::nullopt; // skew;
    };

  public:
    sl::Signal<void()> on_change;

  public:
    ModulationParameters(std::string const &mod_type,
                         std::vector<SliderMetadata> const &slider_data);

  public:
    void resized() override;

  public:
    [[nodiscard]]
    auto get_json() -> nlohmann::json;

    /**
     * Return true if the mod_type is empty.
     */
    [[nodiscard]]
    auto empty() -> bool;

    [[nodiscard]]
    auto get_type() -> std::string const &;

  private:
    std::string type_;
    std::vector<std::pair<std::unique_ptr<juce::Label>, std::unique_ptr<juce::Slider>>>
        sliders_;
};

class ModulationPane : public juce::Component
{
  public:
    // TODO or commit changes can call a different command or can append or prepend some
    // kind of string? probably just the different command or another parameter like a
    // bool could be appended to the command whether or not to commit, then you don't
    // need two signals
    sl::Signal<void(std::string const &)> on_change;        // Emits command string
    sl::Signal<void(std::string const &)> on_commit_change; // Emits command string

  public:
    // connect to signals from others that cue you to emit json.
    ModulationPane();

  public:
    void resized() override;

  private:
    juce::ComboBox target_command_dropdown_;
    juce::ComboBox modulator_dropdown_;
    std::array<std::unique_ptr<ModulationParameters>, 16> parameter_uis_;
    std::size_t current_selection_{0};
    ModulationButtons buttons_;

  private:
    [[nodiscard]]
    auto generate_json() -> std::string;

    [[nodiscard]]
    auto generate_command_string() -> std::string;
};

} // namespace xen::gui