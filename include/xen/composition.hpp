#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <sequence/time_signature.hpp>

#include <sequence/sequence.hpp>

#include <xen/pitch_system.hpp>

namespace xen
{

using SequenceId = std::uint64_t;
using ChannelId = std::string;

inline constexpr auto DEFAULT_SEQUENCE_ID = SequenceId{1};
inline constexpr auto DEFAULT_CHANNEL_ID = "channel-1";

struct CompositionCursor
{
    std::size_t row_index{};
    std::size_t column_index{};
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
    std::vector<std::optional<SequenceId>> cells{};

    auto operator==(CompositionRow const &) const -> bool = default;
};

struct LoopRegion
{
    std::size_t start_column{0};
    std::size_t end_column{0};

    auto operator==(LoopRegion const &) const -> bool = default;
};

struct Composition
{
    std::vector<CompositionColumn> columns{};
    std::vector<CompositionRow> rows{};
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

auto insert_row(Composition &composition, std::size_t index,
                ChannelId channel_id = DEFAULT_CHANNEL_ID) -> void;
auto remove_row(Composition &composition, std::size_t index) -> void;
auto move_row(Composition &composition, std::size_t from, std::size_t to) -> void;
auto assign_row_channel(Composition &composition, std::size_t row, ChannelId channel_id)
    -> void;

auto insert_column(Composition &composition, std::size_t index,
                   CompositionColumn column = {}) -> void;
auto insert_column(Composition &composition, std::size_t index,
                   sequence::TimeSignature duration) -> void;
auto duplicate_column(Composition &composition, std::size_t source, std::size_t index)
    -> void;
auto remove_column(Composition &composition, std::size_t index) -> void;
auto move_column(Composition &composition, std::size_t from, std::size_t to) -> void;
auto set_column_duration(Composition &composition, std::size_t column,
                         sequence::TimeSignature duration) -> void;
auto set_loop_start(Composition &composition, std::size_t column) -> void;
auto set_loop_end(Composition &composition, std::size_t column) -> void;

auto assign_sequence_reference(Composition &composition, std::size_t row,
                               std::size_t column, SequenceId id) -> void;
auto clear_sequence_reference(Composition &composition, std::size_t row,
                              std::size_t column) -> void;
[[nodiscard]] auto sequence_reference_at(Composition const &composition,
                                         std::size_t row, std::size_t column)
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
