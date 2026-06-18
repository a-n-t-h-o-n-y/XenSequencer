#pragma once

#include <variant>

#include <juce_core/juce_core.h>

#include <sequence/sequence.hpp>

namespace xen
{

using CopyBufferContent = std::variant<sequence::Cell, sequence::MusicElement>;

[[nodiscard]] auto copy_buffer_filepath() -> juce::File;

} // namespace xen
