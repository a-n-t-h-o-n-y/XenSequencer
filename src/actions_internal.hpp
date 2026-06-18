#pragma once

#include <sequence/pattern.hpp>
#include <sequence/sequence.hpp>

namespace xen::action
{

[[nodiscard]] auto shift_pitch(sequence::MusicElement element,
                               sequence::Pattern const &pattern, int amount)
    -> sequence::MusicElement;

[[nodiscard]] auto shift_pitch(sequence::Cell cell, sequence::Pattern const &pattern,
                               int amount) -> sequence::Cell;

} // namespace xen::action
