#include <xen/parse_args.hpp>

#include <sstream>
#include <string>
#include <vector>

namespace xen
{

auto split_args(std::string const &s) -> std::vector<std::string>
{
    auto iss = std::istringstream{s};
    auto params = std::vector<std::string>{};
    auto param = std::string{};
    auto inside_quotes = false;

    while (iss.peek() != std::istringstream::traits_type::eof())
    {
        auto ch = static_cast<char>(iss.get());

        if (ch == ' ' && !inside_quotes)
        {
            if (!param.empty())
            {
                params.push_back(param);
                param.clear();
            }
        }
        else if (ch == '"')
        {
            inside_quotes = !inside_quotes;
        }
        else
        {
            param += ch;
        }
    }

    if (!param.empty())
    {
        params.push_back(param);
    }

    return params;
}

} // namespace xen
