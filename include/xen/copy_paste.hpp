#pragma once

#include <variant>

#include <sequence/sequence.hpp>

namespace xen
{

using CopyBufferContent = std::variant<sequence::Cell, sequence::MusicElement>;

} // namespace xen
