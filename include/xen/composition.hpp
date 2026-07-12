#pragma once

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <sequence/sequence.hpp>
#include <sequence/time_signature.hpp>

#include <xen/pitch_system.hpp>

namespace xen
{

using SequenceId = std::uint64_t;
using ChannelId = std::string;
using CompositionCoordinate = std::int32_t;

inline constexpr auto DEFAULT_SEQUENCE_ID = SequenceId{1};
inline constexpr auto DEFAULT_CHANNEL_ID = "channel-1";

struct CompositionPosition
{
    CompositionCoordinate row_coordinate{};
    CompositionCoordinate column_coordinate{};

    auto operator<=>(CompositionPosition const &) const = default;
};

struct CompositionCursor
{
    CompositionCoordinate row_coordinate{};
    CompositionCoordinate column_coordinate{};
    std::optional<SequenceId> sequence_id{DEFAULT_SEQUENCE_ID};

    auto operator==(CompositionCursor const &) const -> bool = default;
};

struct SequenceBankEntry
{
    SequenceId id{};
    std::optional<std::string> name{};
    sequence::Cell cell{.elements = {}, .weight = 1.f};

    auto operator==(SequenceBankEntry const &) const -> bool = default;
};

struct SequenceBank
{
    std::vector<SequenceBankEntry> sequences{};
    SequenceId next_id{DEFAULT_SEQUENCE_ID};

    auto operator==(SequenceBank const &) const -> bool = default;
};

struct CompositionColumn
{
    sequence::TimeSignature duration{4, 4};
    PitchSystem pitch{};

    auto operator==(CompositionColumn const &) const -> bool = default;
};

struct CompositionRow
{
    std::optional<std::string> name{};
    ChannelId channel_id{DEFAULT_CHANNEL_ID};

    auto operator==(CompositionRow const &) const -> bool = default;
};

struct LoopRegion
{
    CompositionCoordinate start_column{};
    CompositionCoordinate end_column{};

    auto operator==(LoopRegion const &) const -> bool = default;
};

struct Composition
{
    CompositionColumn default_column{};
    std::map<CompositionCoordinate, CompositionColumn> columns{};
    std::map<CompositionCoordinate, CompositionRow> rows{};
    std::map<CompositionPosition, SequenceId> placements{};
    LoopRegion loop_region{};

    auto operator==(Composition const &) const -> bool = default;
};

[[nodiscard]] auto make_default_sequence_bank() -> SequenceBank;
[[nodiscard]] auto make_default_composition() -> Composition;

[[nodiscard]] auto create_sequence(SequenceBank &bank, sequence::Cell cell)
    -> SequenceId;
auto remove_sequence(SequenceBank &bank, SequenceId id) -> bool;
[[nodiscard]] auto duplicate_sequence(SequenceBank &bank, SequenceId id) -> SequenceId;
[[nodiscard]] auto find_sequence(SequenceBank &bank, SequenceId id) -> sequence::Cell *;
[[nodiscard]] auto find_sequence(SequenceBank const &bank, SequenceId id)
    -> sequence::Cell const *;
auto update_sequence(SequenceBank &bank, SequenceId id, sequence::Cell cell) -> bool;
[[nodiscard]] auto all_sequences(SequenceBank const &bank)
    -> std::vector<SequenceBankEntry> const &;

[[nodiscard]] auto composition_row(Composition &composition,
                                   CompositionCoordinate coordinate)
    -> CompositionRow &;
[[nodiscard]] auto composition_row(Composition const &composition,
                                   CompositionCoordinate coordinate)
    -> CompositionRow const &;
[[nodiscard]] auto composition_column(Composition &composition,
                                      CompositionCoordinate coordinate)
    -> CompositionColumn &;
[[nodiscard]] auto composition_column(Composition const &composition,
                                      CompositionCoordinate coordinate)
    -> CompositionColumn const &;
auto ensure_composition_row(Composition &composition, CompositionCoordinate coordinate,
                            std::optional<ChannelId> channel_id = std::nullopt)
    -> CompositionRow &;
auto ensure_composition_column(Composition &composition,
                               CompositionCoordinate coordinate) -> CompositionColumn &;
auto assign_row_channel(Composition &composition, CompositionCoordinate row,
                        ChannelId channel_id) -> void;
auto set_column_duration(Composition &composition, CompositionCoordinate column,
                         sequence::TimeSignature duration) -> void;
auto set_loop_start(Composition &composition, CompositionCoordinate column) -> void;
auto set_loop_end(Composition &composition, CompositionCoordinate column) -> void;

auto assign_sequence_reference(Composition &composition, CompositionCoordinate row,
                               CompositionCoordinate column, SequenceId id) -> void;
auto clear_sequence_reference(Composition &composition, CompositionCoordinate row,
                              CompositionCoordinate column) -> void;
auto move_sequence_reference(Composition &composition, CompositionPosition from,
                             CompositionPosition to) -> void;
[[nodiscard]] auto sequence_reference_at(Composition const &composition,
                                         CompositionCoordinate row,
                                         CompositionCoordinate column)
    -> std::optional<SequenceId>;

[[nodiscard]] auto arranged_sequence(SequenceBank &bank, Composition const &composition,
                                     CompositionCursor const &cursor)
    -> sequence::Cell &;
[[nodiscard]] auto arranged_sequence(SequenceBank const &bank,
                                     Composition const &composition,
                                     CompositionCursor const &cursor)
    -> sequence::Cell const &;
[[nodiscard]] auto default_column_duration(Composition &composition)
    -> sequence::TimeSignature &;
[[nodiscard]] auto default_column_duration(Composition const &composition)
    -> sequence::TimeSignature const &;

} // namespace xen
