#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <sequence/pattern.hpp>

namespace xen
{
/**
 * A slightly more structured version of the input string.
 * @details Quoted strings are not split, they are a single 'word'.
 */
struct SourceSpan
{
    std::size_t begin{0};
    std::size_t end{0};
};

struct ParsedCommandInput
{
    sequence::Pattern pattern;
    bool has_pattern_prefix{false};
    std::vector<std::string> words;
    std::vector<SourceSpan> word_spans;
};

/**
 * Parsed command invocation used by the chain executor.
 */
struct CommandInvocation
{
    /**
     * Canonical, whitespace-normalized command segment text.
     */
    std::string canonical_segment{};

    /**
     * Source range occupied by the command segment in the original input.
     */
    SourceSpan source_span{};

    /**
     * Parsed command tokens and optional pattern prefix for catalog binding.
     */
    ParsedCommandInput input{};
};

/**
 * Parse the final command segment into a Pattern and a vector of words.
 *
 * @param input The input string to parse.
 * @exception std::invalid_argument Thrown when syntax is malformed.
 */
[[nodiscard]] auto parse_command_input(std::string const &input) -> ParsedCommandInput;

/**
 * Parse a raw command string into a canonical command chain.
 *
 * @details Splits on top-level semicolons, normalizes per-segment whitespace,
 * drops empty segments, and parses each segment into a command invocation.
 */
[[nodiscard]] auto parse_command_chain(std::string const &raw_command_string)
    -> std::vector<CommandInvocation>;

/**
 * Holds display information about a command signature for docs/completion.
 */
struct SignatureDisplay
{
    std::string id;
    std::vector<std::string> arguments;
    bool pattern_arg = false;
};

/**
 * Provides a textural description of a command for documentation purposes.
 */
struct Documentation
{
    SignatureDisplay signature;
    std::string description;
};

} // namespace xen
