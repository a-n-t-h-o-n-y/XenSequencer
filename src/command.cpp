#include <xen/command.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <sequence/pattern.hpp>

namespace xen
{
namespace
{

struct LexedToken
{
    std::string value{};
    SourceSpan source_span{};
    SourceSpan canonical_span{};
};

struct LexedSegment
{
    std::string canonical{};
    SourceSpan source_span{};
    std::vector<LexedToken> tokens{};
};

auto syntax_error(std::string const &message, std::size_t offset)
    -> std::invalid_argument
{
    return std::invalid_argument(message + " at offset " + std::to_string(offset));
}

auto lex_commands(std::string const &input) -> std::vector<LexedSegment>
{
    auto segments = std::vector<LexedSegment>{};
    auto segment = LexedSegment{};
    auto token_value = std::string{};
    auto token_source_begin = std::size_t{0};
    auto token_canonical_begin = std::size_t{0};
    auto token_active = false;
    auto pending_separator = false;
    auto in_quotes = false;
    auto escaped = false;
    auto quote_offset = std::size_t{0};
    auto brace_offsets = std::vector<std::size_t>{};
    auto segment_source_begin = std::size_t{0};
    auto segment_source_end = std::size_t{0};

    auto begin_token = [&](std::size_t source_offset) {
        if (token_active)
        {
            return;
        }
        if (pending_separator && !segment.canonical.empty())
        {
            segment.canonical.push_back(' ');
        }
        pending_separator = false;
        token_active = true;
        token_source_begin = source_offset;
        token_canonical_begin = segment.canonical.size();
        if (segment.tokens.empty())
        {
            segment_source_begin = source_offset;
        }
    };

    auto finish_token = [&](std::size_t source_end) {
        if (!token_active)
        {
            return;
        }
        segment.tokens.push_back(LexedToken{
            .value = std::move(token_value),
            .source_span = {token_source_begin, source_end},
            .canonical_span = {token_canonical_begin, segment.canonical.size()},
        });
        token_value.clear();
        token_active = false;
        segment_source_end = source_end;
    };

    auto finish_segment = [&](std::size_t source_end) {
        finish_token(source_end);
        if (!segment.tokens.empty())
        {
            segment.source_span = {segment_source_begin, segment_source_end};
            segments.push_back(std::move(segment));
            segment = LexedSegment{};
        }
        pending_separator = false;
    };

    for (auto offset = std::size_t{0}; offset < input.size(); ++offset)
    {
        auto const ch = input[offset];

        if (escaped)
        {
            begin_token(offset);
            segment.canonical.push_back(ch);
            token_value.push_back(ch);
            escaped = false;
            continue;
        }

        if (in_quotes && ch == '\\')
        {
            begin_token(offset);
            segment.canonical.push_back(ch);
            token_value.push_back(ch);
            escaped = true;
            continue;
        }

        if (ch == '"')
        {
            begin_token(offset);
            segment.canonical.push_back(ch);
            if (!in_quotes)
            {
                in_quotes = true;
                quote_offset = offset;
                if (!brace_offsets.empty())
                {
                    token_value.push_back(ch);
                }
            }
            else
            {
                in_quotes = false;
                if (!brace_offsets.empty())
                {
                    token_value.push_back(ch);
                }
            }
            continue;
        }

        if (!in_quotes && ch == '{')
        {
            begin_token(offset);
            brace_offsets.push_back(offset);
            segment.canonical.push_back(ch);
            token_value.push_back(ch);
            continue;
        }

        if (!in_quotes && ch == '}')
        {
            if (brace_offsets.empty())
            {
                throw syntax_error("Unexpected closing brace", offset);
            }
            else
            {
                brace_offsets.pop_back();
            }
            begin_token(offset);
            segment.canonical.push_back(ch);
            token_value.push_back(ch);
            continue;
        }

        auto const at_top_level = !in_quotes && brace_offsets.empty();
        if (at_top_level && ch == ';')
        {
            finish_segment(offset);
            continue;
        }

        if (at_top_level && std::isspace(static_cast<unsigned char>(ch)) != 0)
        {
            finish_token(offset);
            pending_separator = !segment.tokens.empty();
            continue;
        }

        begin_token(offset);
        segment.canonical.push_back(ch);
        token_value.push_back(ch);
    }

    if (escaped)
    {
        throw syntax_error("Dangling escape in quoted string", input.size() - 1);
    }
    if (in_quotes)
    {
        throw syntax_error("Unterminated quoted string", quote_offset);
    }
    if (!brace_offsets.empty())
    {
        throw syntax_error("Unmatched opening brace", brace_offsets.back());
    }

    finish_segment(input.size());
    return segments;
}

auto parsed_input(LexedSegment const &segment) -> ParsedCommandInput
{
    auto result = ParsedCommandInput{
        .pattern = {0, {1}},
        .has_pattern_prefix = false,
        .words = {},
        .word_spans = {},
    };

    auto pattern_token_count = std::size_t{0};
    if (sequence::contains_valid_pattern(segment.canonical))
    {
        auto const input_without_pattern =
            sequence::pop_pattern_chars(segment.canonical);
        auto const pattern_prefix_size =
            segment.canonical.size() - input_without_pattern.size();
        result.has_pattern_prefix = pattern_prefix_size != 0;
        result.pattern = sequence::parse_pattern(segment.canonical);
        pattern_token_count = static_cast<std::size_t>(
            std::count_if(segment.tokens.begin(), segment.tokens.end(),
                          [pattern_prefix_size](LexedToken const &token) {
                              return token.canonical_span.begin < pattern_prefix_size;
                          }));
    }

    result.words.reserve(segment.tokens.size() - pattern_token_count);
    result.word_spans.reserve(segment.tokens.size() - pattern_token_count);
    for (auto i = pattern_token_count; i < segment.tokens.size(); ++i)
    {
        result.words.push_back(segment.tokens[i].value);
        result.word_spans.push_back(segment.tokens[i].source_span);
    }
    return result;
}

} // namespace

auto parse_command_input(std::string const &input) -> ParsedCommandInput
{
    auto const segments = lex_commands(input);
    if (segments.empty())
    {
        return ParsedCommandInput{
            .pattern = {0, {1}},
            .has_pattern_prefix = false,
            .words = {},
            .word_spans = {},
        };
    }
    return parsed_input(segments.back());
}

auto parse_command_chain(std::string const &raw_command_string)
    -> std::vector<CommandInvocation>
{
    auto const segments = lex_commands(raw_command_string);
    auto chain = std::vector<CommandInvocation>{};
    chain.reserve(segments.size());
    for (auto const &segment : segments)
    {
        chain.push_back(CommandInvocation{
            .canonical_segment = segment.canonical,
            .source_span = segment.source_span,
            .input = parsed_input(segment),
        });
    }
    return chain;
}

} // namespace xen
