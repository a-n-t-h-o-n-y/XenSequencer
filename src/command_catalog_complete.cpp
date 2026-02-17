#include <xen/command_catalog.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <xen/command.hpp>
#include <xen/string_manip.hpp>

#include "command_catalog_metadata_internal.hpp"

namespace xen
{
namespace
{

struct CompletionNode
{
    std::string token{};
    std::vector<std::size_t> children{};
    std::optional<std::size_t> metadata_index{};
};

struct CompletionTree
{
    std::vector<CompletionNode> nodes{};
};

auto iequals(std::string const &a, std::string const &b) -> bool
{
    return to_lower(a) == to_lower(b);
}

auto istarts_with(std::string const &full, std::string const &prefix) -> bool
{
    if (prefix.size() > full.size())
    {
        return false;
    }
    return iequals(full.substr(0, prefix.size()), prefix);
}

auto build_completion_tree(std::vector<CatalogCommandMetadata> const &metadata)
    -> CompletionTree
{
    auto tree = CompletionTree{};
    tree.nodes.push_back(CompletionNode{}); // root

    for (auto i = std::size_t{0}; i < metadata.size(); ++i)
    {
        auto node_index = std::size_t{0};

        for (auto const &token : metadata[i].path)
        {
            auto matched_child = std::optional<std::size_t>{};
            for (auto const child_index : tree.nodes[node_index].children)
            {
                if (iequals(tree.nodes[child_index].token, token))
                {
                    matched_child = child_index;
                    break;
                }
            }

            if (!matched_child.has_value())
            {
                auto const new_child_index = tree.nodes.size();
                tree.nodes.push_back(CompletionNode{.token = token});
                tree.nodes[node_index].children.push_back(new_child_index);
                node_index = new_child_index;
            }
            else
            {
                node_index = *matched_child;
            }
        }

        tree.nodes[node_index].metadata_index = i;
    }

    return tree;
}

auto completion_tree() -> CompletionTree const &
{
    static auto const tree = build_completion_tree(command_catalog_metadata_storage());
    return tree;
}

auto format_remaining_arguments(CatalogCommandMetadata const &metadata,
                                std::size_t supplied_count) -> std::string
{
    if (supplied_count >= metadata.arguments.size())
    {
        return "";
    }

    auto text = std::string{};
    auto separator = std::string{};

    for (auto i = supplied_count; i < metadata.arguments.size(); ++i)
    {
        text += separator;
        text += format_metadata_argument(metadata.arguments[i]);
        separator = " ";
    }

    return text;
}

auto complete_from_node(std::size_t node_index, std::vector<std::string> const &words,
                        std::size_t word_index) -> std::string
{
    auto const &metadata = command_catalog_metadata_storage();
    auto const &tree = completion_tree();
    auto const &node = tree.nodes[node_index];
    auto const leaf = node.metadata_index.has_value() ? &metadata[*node.metadata_index]
                                                      : nullptr;

    if (word_index >= words.size())
    {
        if (leaf != nullptr)
        {
            return format_remaining_arguments(*leaf, 0);
        }
        return "[next command]";
    }

    auto const &word = words[word_index];

    for (auto const child_index : node.children)
    {
        if (iequals(tree.nodes[child_index].token, word))
        {
            return complete_from_node(child_index, words, word_index + 1);
        }
    }

    if (leaf != nullptr)
    {
        return format_remaining_arguments(*leaf, words.size() - word_index);
    }

    for (auto const child_index : node.children)
    {
        auto const &token = tree.nodes[child_index].token;
        if (istarts_with(token, word))
        {
            return token.substr(word.size());
        }
    }

    return "";
}

} // namespace

auto CommandCatalog::complete_text(std::string const &partial_command) const
    -> std::string
{
    if (strip(partial_command).empty())
    {
        return "";
    }

    auto const split = split_input(partial_command);
    return complete_from_node(0, split.words, 0);
}

auto CommandCatalog::complete_id(std::string const &partial_command) const
    -> std::string
{
    auto const potential = get_first_word(complete_text(partial_command));
    if (!potential.empty() && potential.front() == '[')
    {
        return "";
    }
    return potential;
}

auto catalog_complete_text(std::string const &partial_command) -> std::string
{
    return default_command_catalog().complete_text(partial_command);
}

auto catalog_complete_id(std::string const &partial_command) -> std::string
{
    return default_command_catalog().complete_id(partial_command);
}

} // namespace xen
