#include <xen/composition.hpp>

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace xen
{
namespace
{

template <typename T>
auto checked_insert(std::vector<T> &items, std::size_t index, T item) -> void
{
    if (index > items.size())
        throw std::out_of_range{"Insert index is out of range."};
    items.insert(std::next(items.begin(), static_cast<std::ptrdiff_t>(index)),
                 std::move(item));
}

template <typename T>
auto checked_erase(std::vector<T> &items, std::size_t index) -> void
{
    if (index >= items.size())
        throw std::out_of_range{"Remove index is out of range."};
    items.erase(std::next(items.begin(), static_cast<std::ptrdiff_t>(index)));
}

template <typename T>
auto checked_move(std::vector<T> &items, std::size_t from, std::size_t to) -> void
{
    if (from >= items.size() || to >= items.size())
        throw std::out_of_range{"Move index is out of range."};
    if (from == to)
        return;
    auto item = std::move(items[from]);
    items.erase(std::next(items.begin(), static_cast<std::ptrdiff_t>(from)));
    items.insert(std::next(items.begin(), static_cast<std::ptrdiff_t>(to)),
                 std::move(item));
}

auto require_row(Composition &composition, std::size_t row) -> CompositionRow &
{
    if (row >= composition.rows.size())
        throw std::out_of_range{"Composition row index is out of range."};
    return composition.rows[row];
}

auto require_row(Composition const &composition, std::size_t row)
    -> CompositionRow const &
{
    if (row >= composition.rows.size())
        throw std::out_of_range{"Composition row index is out of range."};
    return composition.rows[row];
}

auto require_column(Composition &composition, std::size_t column) -> CompositionColumn &
{
    if (column >= composition.columns.size())
        throw std::out_of_range{"Composition column index is out of range."};
    return composition.columns[column];
}

auto require_column(Composition const &composition, std::size_t column)
    -> CompositionColumn const &
{
    if (column >= composition.columns.size())
        throw std::out_of_range{"Composition column index is out of range."};
    return composition.columns[column];
}

auto require_cell(Composition &composition, std::size_t row, std::size_t column)
    -> std::optional<SequenceId> &
{
    auto &target_row = require_row(composition, row);
    if (column >= target_row.cells.size())
        throw std::out_of_range{"Composition column index is out of range."};
    return target_row.cells[column];
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

auto adjust_loop_after_insert(LoopRegion &loop, std::size_t index) -> void
{
    auto const start = loop.start_column;
    auto const end = loop.end_column;
    if (start <= end)
    {
        if (index < start)
        {
            loop.start_column = start + 1;
            loop.end_column = end + 1;
        }
        else if (index <= end + 1)
        {
            loop.end_column = end + 1;
        }
        return;
    }
    if (index <= end)
        loop.end_column = end + 1;
    else if (index < start)
        loop.start_column = start + 1;
}

auto shifted_after_remove(std::size_t column, std::size_t index, std::size_t new_size)
    -> std::size_t
{
    if (column == index)
        return std::min(index, new_size - 1);
    return column > index ? column - 1 : column;
}

auto shifted_after_move(std::size_t column, std::size_t from, std::size_t to)
    -> std::size_t
{
    if (column == from)
        return to;
    if (from < to && column > from && column <= to)
        return column - 1;
    if (to < from && column >= to && column < from)
        return column + 1;
    return column;
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
        .columns = {CompositionColumn{}},
        .rows = {CompositionRow{.channel_id = DEFAULT_CHANNEL_ID,
                                .cells = {DEFAULT_SEQUENCE_ID}}},
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

auto insert_row(Composition &composition, std::size_t index, ChannelId channel_id)
    -> void
{
    checked_insert(composition.rows, index,
                   CompositionRow{.channel_id = std::move(channel_id),
                                  .cells = std::vector<std::optional<SequenceId>>(
                                      composition.columns.size(), std::nullopt)});
}

auto remove_row(Composition &composition, std::size_t index) -> void
{
    if (composition.rows.size() <= 1)
        throw std::invalid_argument{"Composition must contain at least one row."};
    checked_erase(composition.rows, index);
}

auto move_row(Composition &composition, std::size_t from, std::size_t to) -> void
{
    checked_move(composition.rows, from, to);
}

auto assign_row_channel(Composition &composition, std::size_t row, ChannelId channel_id)
    -> void
{
    if (channel_id.empty())
        throw std::invalid_argument{"Channel ID must not be empty."};
    require_row(composition, row).channel_id = std::move(channel_id);
}

auto insert_column(Composition &composition, std::size_t index,
                   CompositionColumn column) -> void
{
    checked_insert(composition.columns, index, std::move(column));
    for (auto &row : composition.rows)
        checked_insert(row.cells, index, std::optional<SequenceId>{});
    adjust_loop_after_insert(composition.loop_region, index);
}

auto insert_column(Composition &composition, std::size_t index,
                   sequence::TimeSignature duration) -> void
{
    insert_column(composition, index, CompositionColumn{.duration = duration});
}

auto duplicate_column(Composition &composition, std::size_t source, std::size_t index)
    -> void
{
    auto const column = require_column(composition, source);
    auto assignments = std::vector<std::optional<SequenceId>>{};
    assignments.reserve(composition.rows.size());
    for (auto const &row : composition.rows)
        assignments.push_back(row.cells.at(source));
    insert_column(composition, index, column);
    for (auto row = std::size_t{}; row < composition.rows.size(); ++row)
        composition.rows[row].cells[index] = assignments[row];
}

auto remove_column(Composition &composition, std::size_t index) -> void
{
    if (composition.columns.size() <= 1)
        throw std::invalid_argument{"Composition must contain at least one column."};
    checked_erase(composition.columns, index);
    for (auto &row : composition.rows)
        checked_erase(row.cells, index);
    auto const size = composition.columns.size();
    composition.loop_region.start_column =
        shifted_after_remove(composition.loop_region.start_column, index, size);
    composition.loop_region.end_column =
        shifted_after_remove(composition.loop_region.end_column, index, size);
}

auto move_column(Composition &composition, std::size_t from, std::size_t to) -> void
{
    checked_move(composition.columns, from, to);
    for (auto &row : composition.rows)
        checked_move(row.cells, from, to);
    composition.loop_region.start_column =
        shifted_after_move(composition.loop_region.start_column, from, to);
    composition.loop_region.end_column =
        shifted_after_move(composition.loop_region.end_column, from, to);
}

auto set_column_duration(Composition &composition, std::size_t column,
                         sequence::TimeSignature duration) -> void
{
    require_column(composition, column).duration = duration;
}

auto set_loop_start(Composition &composition, std::size_t column) -> void
{
    (void)require_column(composition, column);
    composition.loop_region.start_column = column;
}

auto set_loop_end(Composition &composition, std::size_t column) -> void
{
    (void)require_column(composition, column);
    composition.loop_region.end_column = column;
}

auto assign_sequence_reference(Composition &composition, std::size_t row,
                               std::size_t column, SequenceId id) -> void
{
    require_cell(composition, row, column) = id;
}

auto clear_sequence_reference(Composition &composition, std::size_t row,
                              std::size_t column) -> void
{
    require_cell(composition, row, column) = std::nullopt;
}

auto sequence_reference_at(Composition const &composition, std::size_t row,
                           std::size_t column) -> std::optional<SequenceId>
{
    auto const &target = require_row(composition, row);
    if (column >= target.cells.size())
        throw std::out_of_range{"Composition column index is out of range."};
    return target.cells[column];
}

auto arranged_sequence(SequenceBank &bank, Composition const &composition,
                       CompositionCursor const &cursor) -> sequence::Cell &
{
    auto const id =
        sequence_reference_at(composition, cursor.row_index, cursor.column_index);
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
    auto const id =
        sequence_reference_at(composition, cursor.row_index, cursor.column_index);
    if (!id.has_value())
        throw std::invalid_argument{"Active composition placement is empty."};
    if (cursor.sequence_id != id)
        throw std::invalid_argument{
            "Active composition placement does not reference the requested sequence."};
    return require_sequence(bank, *id);
}

auto default_column_duration(Composition &composition) -> sequence::TimeSignature &
{
    return require_column(composition, 0).duration;
}

auto default_column_duration(Composition const &composition)
    -> sequence::TimeSignature const &
{
    return require_column(composition, 0).duration;
}

} // namespace xen
