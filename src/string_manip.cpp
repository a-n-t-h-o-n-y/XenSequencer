#include <xen/string_manip.hpp>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <string>
#include <string_view>

namespace xen
{

auto to_lower(std::string_view x) -> std::string
{
    auto result = std::string{};
    std::transform(std::cbegin(x), std::cend(x), std::back_inserter(result),
                   [](char c) { return static_cast<char>(::tolower(c)); });
    return result;
}

auto single_quote(std::string const &input) -> std::string
{
    return '\'' + input + '\'';
}

} // namespace xen
