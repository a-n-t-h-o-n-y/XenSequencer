#include <xen/command.hpp>

#include <sequence/pattern.hpp>

#include <xen/string_manip.hpp>

namespace xen
{

auto split_input(std::string input) -> SplitInput
{
    auto split_input = SplitInput{
        .pattern = {0, {1}},
        .has_pattern_prefix = false,
        .words = {},
    };

    if (sequence::contains_valid_pattern(input))
    {
        auto const input_without_pattern = sequence::pop_pattern_chars(input);
        split_input.has_pattern_prefix = input_without_pattern != input;
        split_input.pattern = sequence::parse_pattern(input);
        input = input_without_pattern;
    }

    split_input.words = split_quoted_string(input);

    return split_input;
}

auto parse_command_chain(std::string const &raw_command_string)
    -> std::vector<CommandInvocation>
{
    auto chain = std::vector<CommandInvocation>{};
    auto segments = split_top_level(raw_command_string, ';');
    chain.reserve(segments.size());

    for (auto &segment : segments)
    {
        auto canonical_segment = minimize_spaces(segment);
        if (canonical_segment.empty())
        {
            continue;
        }

        chain.push_back(CommandInvocation{
            .canonical_segment = canonical_segment,
            .input = split_input(canonical_segment),
        });
    }

    return chain;
}

auto is_again_invocation(CommandInvocation const &invocation) -> bool
{
    return to_lower(invocation.canonical_segment) == "again";
}

} // namespace xen
