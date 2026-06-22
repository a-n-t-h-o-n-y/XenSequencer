#pragma once

#include <juce_core/juce_core.h>

#include <xen/state.hpp>

namespace xen
{

class WorkspaceSettingsStore
{
  public:
    explicit WorkspaceSettingsStore(juce::File file = default_file());

    [[nodiscard]] auto load_or_initialize() const -> WorkspaceSettings;
    void save(WorkspaceSettings const &settings) const;
    [[nodiscard]] auto file() const -> juce::File const &;

    [[nodiscard]] static auto default_file() -> juce::File;

  private:
    juce::File file_;
};

} // namespace xen
