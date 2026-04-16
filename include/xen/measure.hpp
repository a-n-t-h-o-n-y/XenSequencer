#pragma once

#include <sequence/sequence.hpp>
#include <sequence/time_signature.hpp>

namespace xen
{

struct Measure
{
    sequence::Cell cell{.elements = {}, .weight = 1.f};
    sequence::TimeSignature time_signature{4, 4};

    auto operator==(Measure const &) const -> bool = default;
    auto operator!=(Measure const &) const -> bool = default;
};

} // namespace xen
