#pragma once

#include <sequence/sequence.hpp>

namespace xen
{

struct Measure
{
    sequence::Cell cell{.elements = {}, .weight = 1.f};

    auto operator==(Measure const &) const -> bool = default;
    auto operator!=(Measure const &) const -> bool = default;
};

} // namespace xen
