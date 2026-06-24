#include <xen/submission_effects.hpp>

#include <filesystem>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>

#include <juce_core/juce_core.h>

#include <xen/text_file.hpp>

namespace xen
{
namespace
{

auto as_juce_file(std::filesystem::path const &path) -> juce::File
{
    return juce::File{path.string()};
}

auto unique_sibling(std::filesystem::path const &destination, std::string const &suffix)
    -> std::filesystem::path
{
    auto const file = as_juce_file(destination);
    return std::filesystem::path{file.getSiblingFile(file.getFileName() + "." + suffix +
                                                     "." + juce::Uuid{}.toString())
                                     .getFullPathName()
                                     .toStdString()};
}

} // namespace

SubmissionEffects::SubmissionEffects(FailurePoint failure_point)
    : failure_point_{failure_point}
{
}

SubmissionEffects::~SubmissionEffects()
{
    finalize();
}

void SubmissionEffects::write_text(std::filesystem::path destination,
                                   std::string content)
{
    auto const at = std::ranges::find(
        replacements_, destination,
        [](Replacement const &replacement) { return replacement.destination; });
    if (at != replacements_.end())
    {
        at->content = std::move(content);
        return;
    }
    replacements_.push_back(Replacement{.destination = std::move(destination),
                                        .content = std::move(content)});
}

auto SubmissionEffects::read_text(std::filesystem::path const &source) const
    -> std::optional<std::string>
{
    auto const at =
        std::ranges::find(replacements_, source, [](Replacement const &replacement) {
            return replacement.destination;
        });
    if (at != replacements_.end())
    {
        return at->content;
    }
    return read_text_file(source);
}

void SubmissionEffects::prepare()
{
    if (failure_point_ == FailurePoint::Prepare && !replacements_.empty())
    {
        throw std::runtime_error{"Injected effect prepare failure"};
    }
    for (auto &replacement : replacements_)
    {
        auto const destination = as_juce_file(replacement.destination);
        auto const parent = destination.getParentDirectory();
        if (!parent.isDirectory() && !parent.createDirectory())
        {
            throw std::runtime_error{"Failed to create effect destination directory: " +
                                     parent.getFullPathName().toStdString()};
        }
        replacement.temporary = unique_sibling(replacement.destination, "xen-tmp");
        replacement.backup = unique_sibling(replacement.destination, "xen-backup");
        atomic_write_text_file(replacement.temporary, replacement.content);
        if (!as_juce_file(replacement.temporary).existsAsFile())
        {
            throw std::runtime_error{"Failed to prepare file replacement: " +
                                     replacement.destination.string()};
        }
    }
}

void SubmissionEffects::apply()
{
    for (auto &replacement : replacements_)
    {
        auto destination = as_juce_file(replacement.destination);
        auto temporary = as_juce_file(replacement.temporary);
        auto backup = as_juce_file(replacement.backup);
        replacement.destination_existed = destination.existsAsFile();
        if (replacement.destination_existed && !destination.moveFileTo(backup))
        {
            throw std::runtime_error{"Failed to back up file replacement target: " +
                                     replacement.destination.string()};
        }
        replacement.applied = replacement.destination_existed;
        if (failure_point_ == FailurePoint::Apply ||
            failure_point_ == FailurePoint::ApplyAndRollback)
        {
            throw std::runtime_error{"Injected effect apply failure"};
        }
        if (!temporary.moveFileTo(destination))
        {
            throw std::runtime_error{"Failed to apply file replacement: " +
                                     replacement.destination.string()};
        }
        replacement.applied = true;
    }
}

void SubmissionEffects::finalize() noexcept
{
    for (auto &replacement : replacements_)
    {
        auto const temporary = as_juce_file(replacement.temporary);
        auto const backup = as_juce_file(replacement.backup);
        if (temporary.exists())
        {
            (void)temporary.deleteFile();
        }
        if (backup.exists())
        {
            (void)backup.deleteFile();
        }
    }
}

auto SubmissionEffects::rollback() noexcept -> std::string
{
    auto failures = std::string{};
    for (auto at = replacements_.rbegin(); at != replacements_.rend(); ++at)
    {
        if (!at->applied)
        {
            continue;
        }
        auto const destination = as_juce_file(at->destination);
        auto const backup = as_juce_file(at->backup);
        auto restored = true;
        if (destination.exists() && !destination.deleteFile())
        {
            restored = false;
        }
        if (at->destination_existed &&
            (!backup.existsAsFile() || !backup.moveFileTo(destination)))
        {
            restored = false;
        }
        if (!restored)
        {
            if (!failures.empty())
            {
                failures += "; ";
            }
            failures += at->destination.string();
        }
        else if (failure_point_ == FailurePoint::ApplyAndRollback)
        {
            if (!failures.empty())
            {
                failures += "; ";
            }
            failures += at->destination.string();
        }
        at->applied = false;
    }
    finalize();
    return failures;
}

} // namespace xen
