#pragma once

#include <optional>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>

namespace xen
{

class SubmissionEffects
{
  public:
    enum class FailurePoint
    {
        None,
        Prepare,
        Apply,
        ApplyAndRollback,
    };

    explicit SubmissionEffects(FailurePoint failure_point = FailurePoint::None);
    SubmissionEffects(SubmissionEffects const &) = delete;
    auto operator=(SubmissionEffects const &) -> SubmissionEffects & = delete;
    ~SubmissionEffects();

    void write_text(juce::File const &destination, std::string content);
    [[nodiscard]] auto read_text(juce::File const &source) const
        -> std::optional<std::string>;

    void prepare();
    void apply();
    void finalize() noexcept;
    [[nodiscard]] auto rollback() noexcept -> std::string;

  private:
    struct Replacement
    {
        juce::File destination{};
        std::string content{};
        juce::File temporary{};
        juce::File backup{};
        bool destination_existed{false};
        bool applied{false};
    };

    std::vector<Replacement> replacements_{};
    FailurePoint failure_point_{FailurePoint::None};
};

} // namespace xen
