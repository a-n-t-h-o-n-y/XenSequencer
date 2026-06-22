#include <xen/submission_effects.hpp>

#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>

namespace xen
{
namespace
{

auto unique_sibling(juce::File const &destination, std::string const &suffix)
    -> juce::File
{
    return destination.getSiblingFile(destination.getFileName() + "." + suffix + "." +
                                      juce::Uuid{}.toString());
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

void SubmissionEffects::write_text(juce::File const &destination, std::string content)
{
    auto const at = std::ranges::find(
        replacements_, destination,
        [](Replacement const &replacement) { return replacement.destination; });
    if (at != replacements_.end())
    {
        at->content = std::move(content);
        return;
    }
    replacements_.push_back(
        Replacement{.destination = destination, .content = std::move(content)});
}

auto SubmissionEffects::read_text(juce::File const &source) const
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
    if (!source.existsAsFile())
    {
        return std::nullopt;
    }
    return source.loadFileAsString().toStdString();
}

void SubmissionEffects::prepare()
{
    if (failure_point_ == FailurePoint::Prepare && !replacements_.empty())
    {
        throw std::runtime_error{"Injected effect prepare failure"};
    }
    for (auto &replacement : replacements_)
    {
        auto const parent = replacement.destination.getParentDirectory();
        if (!parent.isDirectory() && !parent.createDirectory())
        {
            throw std::runtime_error{"Failed to create effect destination directory: " +
                                     parent.getFullPathName().toStdString()};
        }
        replacement.temporary = unique_sibling(replacement.destination, "xen-tmp");
        replacement.backup = unique_sibling(replacement.destination, "xen-backup");
        if (!replacement.temporary.replaceWithText(replacement.content))
        {
            throw std::runtime_error{
                "Failed to prepare file replacement: " +
                replacement.destination.getFullPathName().toStdString()};
        }
    }
}

void SubmissionEffects::apply()
{
    for (auto &replacement : replacements_)
    {
        replacement.destination_existed = replacement.destination.existsAsFile();
        if (replacement.destination_existed &&
            !replacement.destination.moveFileTo(replacement.backup))
        {
            throw std::runtime_error{
                "Failed to back up file replacement target: " +
                replacement.destination.getFullPathName().toStdString()};
        }
        replacement.applied = replacement.destination_existed;
        if (failure_point_ == FailurePoint::Apply ||
            failure_point_ == FailurePoint::ApplyAndRollback)
        {
            throw std::runtime_error{"Injected effect apply failure"};
        }
        if (!replacement.temporary.moveFileTo(replacement.destination))
        {
            throw std::runtime_error{
                "Failed to apply file replacement: " +
                replacement.destination.getFullPathName().toStdString()};
        }
        replacement.applied = true;
    }
}

void SubmissionEffects::finalize() noexcept
{
    for (auto &replacement : replacements_)
    {
        if (replacement.temporary.exists())
        {
            (void)replacement.temporary.deleteFile();
        }
        if (replacement.backup.exists())
        {
            (void)replacement.backup.deleteFile();
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
        auto restored = true;
        if (at->destination.exists() && !at->destination.deleteFile())
        {
            restored = false;
        }
        if (at->destination_existed &&
            (!at->backup.existsAsFile() || !at->backup.moveFileTo(at->destination)))
        {
            restored = false;
        }
        if (!restored)
        {
            if (!failures.empty())
            {
                failures += "; ";
            }
            failures += at->destination.getFullPathName().toStdString();
        }
        else if (failure_point_ == FailurePoint::ApplyAndRollback)
        {
            if (!failures.empty())
            {
                failures += "; ";
            }
            failures += at->destination.getFullPathName().toStdString();
        }
        at->applied = false;
    }
    finalize();
    return failures;
}

} // namespace xen
