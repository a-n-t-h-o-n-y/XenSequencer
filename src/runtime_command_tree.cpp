#include <xen/runtime_command_tree.hpp>

#include <algorithm>
#include <exception>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <xen/command.hpp>
#include <xen/gui/themes.hpp>
#include <xen/string_manip.hpp>

namespace
{
using xen::MessageLevel;
using xen::RuntimeState;

[[nodiscard]] auto words_for(std::string const &s) -> std::vector<std::string>
{
    return xen::split_input(s).words;
}

[[nodiscard]] auto lower(std::string s) -> std::string
{
    return xen::to_lower(std::move(s));
}

[[nodiscard]] auto completes(std::string const &candidate, std::string const &prefix)
    -> bool
{
    return lower(candidate.substr(0, prefix.size())) == lower(prefix);
}

[[nodiscard]] auto normalize_theme_name(std::string name) -> std::string
{
    name = lower(xen::strip(std::move(name)));
    if (name == "dark")
    {
        return "apollo";
    }
    if (name == "light")
    {
        return "coal";
    }
    return name;
}

} // namespace

namespace xen
{

auto RuntimeCommandTree::execute(RuntimeState &runtime_state,
                                 std::string const &command_string) const
    -> std::optional<std::pair<MessageLevel, std::string>>
{
    auto const words = words_for(command_string);
    if (words.empty())
    {
        return std::nullopt;
    }

    auto const id0 = lower(words[0]);

    if (id0 == "focus")
    {
        if (words.size() != 2)
        {
            return merror("Invalid arguments for focus. Expected: focus <component_id>");
        }
        runtime_state.on_focus_request(words[1]);
        return mdebug("Focused on " + words[1]);
    }

    if (id0 == "show")
    {
        if (words.size() != 2)
        {
            return merror("Invalid arguments for show. Expected: show <component_id>");
        }
        runtime_state.on_show_request(words[1]);
        return mdebug("Showing " + single_quote(words[1]));
    }

    if (id0 == "load")
    {
        if (words.size() >= 2 && lower(words[1]) == "keys")
        {
            try
            {
                auto const lock =
                    std::lock_guard{runtime_state.shared.on_load_keys_request_mtx};
                runtime_state.shared.on_load_keys_request();
                return minfo("Key Config Loaded");
            }
            catch (std::exception const &e)
            {
                return merror("Failed to Load Keys: " + std::string{e.what()});
            }
        }
        return std::nullopt;
    }

    if (id0 == "set")
    {
        if (words.size() >= 3 && lower(words[1]) == "theme")
        {
            try
            {
                auto const theme_name = normalize_theme_name(words[2]);
                auto const theme = gui::find_theme(theme_name);

                auto const lock = std::lock_guard{runtime_state.shared.theme_mtx};
                runtime_state.shared.theme = theme;
                runtime_state.shared.on_theme_update(runtime_state.shared.theme);
                return minfo("Theme Set");
            }
            catch (std::exception const &e)
            {
                return merror("Failed to Load Theme: " + std::string{e.what()});
            }
        }
        return std::nullopt;
    }

    return std::nullopt;
}

auto RuntimeCommandTree::guide_text(std::string const &partial_command) const
    -> std::string
{
    if (strip(partial_command).empty())
    {
        return "";
    }

    auto const words = words_for(partial_command);
    if (words.empty())
    {
        return "";
    }

    if (words.size() == 1)
    {
        auto const first = lower(words[0]);
        for (auto const &root : std::vector<std::string>{"focus", "show", "load", "set"})
        {
            if (completes(root, first))
            {
                return root.substr(words[0].size());
            }
        }
    }

    auto const id0 = lower(words[0]);
    if (id0 == "focus")
    {
        return words.size() == 1 ? "[component_id]" : "";
    }
    if (id0 == "show")
    {
        return words.size() == 1 ? "[component_id]" : "";
    }
    if (id0 == "load")
    {
        if (words.size() == 1)
        {
            return "keys";
        }
        if (words.size() == 2 && completes("keys", lower(words[1])))
        {
            return std::string{"keys"}.substr(words[1].size());
        }
        return "";
    }
    if (id0 == "set")
    {
        if (words.size() == 1)
        {
            return "theme";
        }
        if (words.size() == 2 && completes("theme", lower(words[1])))
        {
            return std::string{"theme"}.substr(words[1].size());
        }
        if (words.size() == 2 && lower(words[1]) == "theme")
        {
            return "[name]";
        }
        return "";
    }

    return "";
}

auto RuntimeCommandTree::complete_id(std::string const &partial_command) const
    -> std::string
{
    auto const potential = get_first_word(guide_text(partial_command));
    if (!potential.empty() && potential.front() == '[')
    {
        return "";
    }
    return potential;
}

} // namespace xen
