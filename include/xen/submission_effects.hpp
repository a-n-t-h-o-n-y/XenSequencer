#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

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

    void write_text(std::filesystem::path destination, std::string content);
    [[nodiscard]] auto read_text(std::filesystem::path const &source) const
        -> std::optional<std::string>;

    void prepare();
    void apply();
    void finalize() noexcept;
    [[nodiscard]] auto rollback() noexcept -> std::string;

  private:
    struct Replacement
    {
        std::filesystem::path destination{};
        std::string content{};
        std::filesystem::path temporary{};
        std::filesystem::path backup{};
        bool destination_existed{false};
        bool applied{false};
    };

    std::vector<Replacement> replacements_{};
    FailurePoint failure_point_{FailurePoint::None};
};

} // namespace xen
