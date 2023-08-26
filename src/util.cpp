#include "util.hpp"

#include <algorithm>
#include <iterator>
#include <string>

namespace xen
{

auto to_lower(std::string x) -> std::string
{
    std::transform(std::cbegin(x), std::cend(x), std::begin(x), ::tolower);
    return x;
}

} // namespace xen