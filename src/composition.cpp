#include <xen/composition.hpp>

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace xen
{
namespace
{

template <typename Value>
[[nodiscard]] auto nearest_value(std::map<CompositionCoordinate, Value> const &items,
                                 CompositionCoordinate coordinate) -> Value const *
{
    auto const distance = [coordinate](CompositionCoordinate candidate) {
        return std::int64_t{candidate} - std::int64_t{coordinate};
    };
    auto const rank = [&](CompositionCoordinate candidate) {
        auto delta = distance(candidate);
        if (delta < 0)
            delta = -delta;
        auto origin_distance = std::int64_t{candidate};
        if (origin_distance < 0)
            origin_distance = -origin_distance;
        return std::tuple{delta, origin_distance, candidate};
    };

    auto best = items.end();
    for (auto at = items.begin(); at != items.end(); ++at)
    {
        if (best == items.end() || rank(at->first) < rank(best->first))
            best = at;
    }
    return best == items.end() ? nullptr : &best->second;
}

auto require_sequence(SequenceBank &bank, SequenceId id) -> sequence::Cell &
{
    auto *cell = find_sequence(bank, id);
    if (cell == nullptr)
        throw std::invalid_argument{"Sequence ID does not exist."};
    return *cell;
}

auto require_sequence(SequenceBank const &bank, SequenceId id) -> sequence::Cell const &
{
    auto const *cell = find_sequence(bank, id);
    if (cell == nullptr)
        throw std::invalid_argument{"Sequence ID does not exist."};
    return *cell;
}

[[nodiscard]] auto has_row_placement(Composition const &composition,
                                     CompositionCoordinate row) -> bool
{
    return std::ranges::any_of(composition.placements, [row](auto const &entry) {
        return entry.first.row_coordinate == row;
    });
}

[[nodiscard]] auto has_column_placement(Composition const &composition,
                                        CompositionCoordinate column) -> bool
{
    return std::ranges::any_of(composition.placements, [column](auto const &entry) {
        return entry.first.column_coordinate == column;
    });
}

auto prune_unused_axes(Composition &composition, CompositionPosition position) -> void
{
    if (!has_row_placement(composition, position.row_coordinate))
        composition.rows.erase(position.row_coordinate);
    if (!has_column_placement(composition, position.column_coordinate))
        composition.columns.erase(position.column_coordinate);
}

} // namespace

auto make_default_sequence_bank() -> SequenceBank
{
    return SequenceBank{
        .sequences = {{.id = DEFAULT_SEQUENCE_ID}},
        .next_id = DEFAULT_SEQUENCE_ID + 1,
    };
}

auto make_default_composition() -> Composition
{
    return Composition{
        .default_column = CompositionColumn{},
        .columns = {{0, CompositionColumn{}}},
        .rows = {{0, CompositionRow{.channel_id = DEFAULT_CHANNEL_ID}}},
        .placements = {{{.row_coordinate = 0, .column_coordinate = 0},
                        DEFAULT_SEQUENCE_ID}},
        .loop_region = {.start_column = 0, .end_column = 0},
    };
}

auto create_sequence(SequenceBank &bank, sequence::Cell cell) -> SequenceId
{
    if (bank.next_id == 0)
        throw std::invalid_argument{"Next sequence ID must be nonzero."};
    if (std::ranges::find(bank.sequences, bank.next_id, &SequenceBankEntry::id) !=
        bank.sequences.end())
        throw std::invalid_argument{"Next sequence ID is already in use."};
    auto const id = bank.next_id++;
    bank.sequences.push_back({.id = id, .cell = std::move(cell)});
    return id;
}

auto remove_sequence(SequenceBank &bank, SequenceId id) -> bool
{
    auto const at = std::ranges::find(bank.sequences, id, &SequenceBankEntry::id);
    if (at == bank.sequences.end())
        return false;
    bank.sequences.erase(at);
    return true;
}

auto duplicate_sequence(SequenceBank &bank, SequenceId id) -> SequenceId
{
    return create_sequence(bank, require_sequence(bank, id));
}

auto find_sequence(SequenceBank &bank, SequenceId id) -> sequence::Cell *
{
    auto const at = std::ranges::find(bank.sequences, id, &SequenceBankEntry::id);
    return at == bank.sequences.end() ? nullptr : &at->cell;
}

auto find_sequence(SequenceBank const &bank, SequenceId id) -> sequence::Cell const *
{
    auto const at = std::ranges::find(bank.sequences, id, &SequenceBankEntry::id);
    return at == bank.sequences.end() ? nullptr : &at->cell;
}

auto update_sequence(SequenceBank &bank, SequenceId id, sequence::Cell cell) -> bool
{
    auto *target = find_sequence(bank, id);
    if (target == nullptr)
        return false;
    *target = std::move(cell);
    return true;
}

auto all_sequences(SequenceBank const &bank) -> std::vector<SequenceBankEntry> const &
{
    return bank.sequences;
}

auto composition_row(Composition &composition, CompositionCoordinate coordinate)
    -> CompositionRow &
{
    auto const at = composition.rows.find(coordinate);
    if (at == composition.rows.end())
        throw std::out_of_range{"Composition row coordinate does not exist."};
    return at->second;
}

auto composition_row(Composition const &composition, CompositionCoordinate coordinate)
    -> CompositionRow const &
{
    auto const at = composition.rows.find(coordinate);
    if (at == composition.rows.end())
        throw std::out_of_range{"Composition row coordinate does not exist."};
    return at->second;
}

auto composition_column(Composition &composition, CompositionCoordinate coordinate)
    -> CompositionColumn &
{
    auto const at = composition.columns.find(coordinate);
    if (at == composition.columns.end())
        throw std::out_of_range{"Composition column coordinate does not exist."};
    return at->second;
}

auto composition_column(Composition const &composition,
                        CompositionCoordinate coordinate) -> CompositionColumn const &
{
    auto const at = composition.columns.find(coordinate);
    if (at == composition.columns.end())
        throw std::out_of_range{"Composition column coordinate does not exist."};
    return at->second;
}

auto ensure_composition_row(Composition &composition, CompositionCoordinate coordinate,
                            std::optional<ChannelId> channel_id) -> CompositionRow &
{
    if (auto const at = composition.rows.find(coordinate); at != composition.rows.end())
    {
        if (channel_id.has_value())
            at->second.channel_id = std::move(*channel_id);
        return at->second;
    }

    auto row = CompositionRow{};
    if (channel_id.has_value())
        row.channel_id = std::move(*channel_id);
    else if (auto const *nearest = nearest_value(composition.rows, coordinate))
        row.channel_id = nearest->channel_id;
    return composition.rows.emplace(coordinate, std::move(row)).first->second;
}

auto ensure_composition_column(Composition &composition,
                               CompositionCoordinate coordinate) -> CompositionColumn &
{
    if (auto const at = composition.columns.find(coordinate);
        at != composition.columns.end())
        return at->second;

    auto column = composition.default_column;
    if (auto const *nearest = nearest_value(composition.columns, coordinate))
        column = *nearest;
    return composition.columns.emplace(coordinate, std::move(column)).first->second;
}

auto assign_row_channel(Composition &composition, CompositionCoordinate row,
                        ChannelId channel_id) -> void
{
    if (channel_id.empty())
        throw std::invalid_argument{"Channel ID must not be empty."};
    composition_row(composition, row).channel_id = std::move(channel_id);
}

auto set_column_duration(Composition &composition, CompositionCoordinate column,
                         sequence::TimeSignature duration) -> void
{
    composition_column(composition, column).duration = duration;
}

auto set_loop_start(Composition &composition, CompositionCoordinate column) -> void
{
    if (column > composition.loop_region.end_column)
        throw std::invalid_argument{"Composition loop start must not exceed loop end."};
    composition.loop_region.start_column = column;
}

auto set_loop_end(Composition &composition, CompositionCoordinate column) -> void
{
    if (column < composition.loop_region.start_column)
        throw std::invalid_argument{
            "Composition loop end must not precede loop start."};
    composition.loop_region.end_column = column;
}

auto assign_sequence_reference(Composition &composition, CompositionCoordinate row,
                               CompositionCoordinate column, SequenceId id) -> void
{
    (void)ensure_composition_row(composition, row);
    (void)ensure_composition_column(composition, column);
    composition.placements[{.row_coordinate = row, .column_coordinate = column}] = id;
}

auto unassign_sequence_reference(Composition &composition, CompositionCoordinate row,
                                 CompositionCoordinate column) -> void
{
    auto const position =
        CompositionPosition{.row_coordinate = row, .column_coordinate = column};
    composition.placements.erase(position);
}

auto move_sequence_reference(Composition &composition, CompositionPosition from,
                             CompositionPosition to) -> void
{
    auto const source = composition.placements.find(from);
    if (source == composition.placements.end())
        throw std::invalid_argument{"Composition source placement does not exist."};
    if (composition.placements.contains(to))
        throw std::invalid_argument{"Composition destination placement is occupied."};

    auto const id = source->second;
    (void)ensure_composition_row(composition, to.row_coordinate);
    (void)ensure_composition_column(composition, to.column_coordinate);
    composition.placements.emplace(to, id);
    composition.placements.erase(source);
    prune_unused_axes(composition, from);
}

auto sequence_reference_at(Composition const &composition, CompositionCoordinate row,
                           CompositionCoordinate column) -> std::optional<SequenceId>
{
    auto const at = composition.placements.find(
        {.row_coordinate = row, .column_coordinate = column});
    return at == composition.placements.end() ? std::nullopt
                                              : std::optional{at->second};
}

auto arranged_sequence(SequenceBank &bank, Composition const &composition,
                       CompositionCursor const &cursor) -> sequence::Cell &
{
    auto const id = sequence_reference_at(composition, cursor.row_coordinate,
                                          cursor.column_coordinate);
    if (!id.has_value())
        throw std::invalid_argument{"Active composition placement is empty."};
    if (cursor.sequence_id != id)
        throw std::invalid_argument{
            "Active composition placement does not reference the requested sequence."};
    return require_sequence(bank, *id);
}

auto arranged_sequence(SequenceBank const &bank, Composition const &composition,
                       CompositionCursor const &cursor) -> sequence::Cell const &
{
    auto const id = sequence_reference_at(composition, cursor.row_coordinate,
                                          cursor.column_coordinate);
    if (!id.has_value())
        throw std::invalid_argument{"Active composition placement is empty."};
    if (cursor.sequence_id != id)
        throw std::invalid_argument{
            "Active composition placement does not reference the requested sequence."};
    return require_sequence(bank, *id);
}

auto default_column_duration(Composition &composition) -> sequence::TimeSignature &
{
    return composition.default_column.duration;
}

auto default_column_duration(Composition const &composition)
    -> sequence::TimeSignature const &
{
    return composition.default_column.duration;
}

} // namespace xen
