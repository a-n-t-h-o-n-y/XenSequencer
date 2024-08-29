#include <xen/parse_args.hpp>

#include <cstddef>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace xen
{

auto parse_int(std::string const &x) -> std::optional<int>
{
    try
    {
        auto pos = std::size_t{0};

        // Pass 0 as the base to auto-detect from the prefix
        auto const result = std::stoi(x, &pos, 0);

        // This verifies the entire string was parsed.
        if (pos != x.size())
        {
            return std::nullopt;
        }

        return result;
    }
    catch (std::invalid_argument const &)
    {
        return std::nullopt;
    }
    catch (std::out_of_range const &)
    {
        return std::nullopt;
    }
}

auto parse_bool(std::string const &x) -> std::optional<bool>
{
    auto const lower = to_lower(x);
    if (lower == "true")
    {
        return true;
    }
    else if (lower == "false")
    {
        return false;
    }
    else
    {
        return std::nullopt;
    }
}

auto parse_time_signature(std::string const &x) -> sequence::TimeSignature
{
    auto ss = std::istringstream{x};
    auto ts = sequence::TimeSignature{};
    ss >> ts;
    if (!ss.eof())
    {
        throw std::invalid_argument{"Invalid time signature format: " + x};
    }
    return ts;
}

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
