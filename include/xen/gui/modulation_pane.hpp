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
#include <xen/gui/xen_slider.hpp>

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
    sl::Signal<void()> on_change;
    sl::Signal<void()> on_commit;

  public:
    ModulationParameters(std::string const &mod_type,
                         std::vector<XenSlider::Metadata> const &slider_data);

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
    std::vector<std::unique_ptr<XenSlider>> sliders_;
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
    juce::ComboBox target_command_dropdown_;
    juce::ComboBox modulator_dropdown_;
    std::array<std::unique_ptr<ModulationParameters>, 16> parameter_uis_;
    std::size_t current_selection_{0};
    ModulationButtons buttons_;

  private:
    [[nodiscard]]
    auto generate_json() -> std::string;

    [[nodiscard]]
    auto generate_command_string(bool commit) -> std::string;
};

} // namespace xen::gui