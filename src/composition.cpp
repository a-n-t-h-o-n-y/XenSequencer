#include <xen/composition.hpp>

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace xen
{
namespace
{

auto require_row(Composition &composition, std::size_t row) -> CompositionRow &
{
    if (row >= composition.rows.size())
    {
        throw std::out_of_range{"Composition row index is out of range."};
    }
    return composition.rows[row];
}

auto require_row(Composition const &composition, std::size_t row)
    -> CompositionRow const &
{
    if (row >= composition.rows.size())
    {
        throw std::out_of_range{"Composition row index is out of range."};
    }
    return composition.rows[row];
}

auto require_column(Composition &composition, std::size_t column) -> CompositionColumn &
{
    if (column >= composition.columns.size())
    {
        throw std::out_of_range{"Composition column index is out of range."};
    }
    return composition.columns[column];
}

auto require_column(Composition const &composition, std::size_t column)
    -> CompositionColumn const &
{
    if (column >= composition.columns.size())
    {
        throw std::out_of_range{"Composition column index is out of range."};
    }
    return composition.columns[column];
}

auto require_cell(Composition &composition, std::size_t row, std::size_t column)
    -> std::optional<MeasureId> &
{
    auto &target_row = require_row(composition, row);
    if (column >= target_row.cells.size())
    {
        throw std::out_of_range{"Composition column index is out of range."};
    }
    return target_row.cells[column];
}

auto require_measure(MeasureBank &bank, MeasureId id) -> Measure &
{
    auto *measure = find_measure(bank, id);
    if (measure == nullptr)
    {
        throw std::invalid_argument{"Measure ID does not exist."};
    }
    return *measure;
}

auto require_measure(MeasureBank const &bank, MeasureId id) -> Measure const &
{
    auto const *measure = find_measure(bank, id);
    if (measure == nullptr)
    {
        throw std::invalid_argument{"Measure ID does not exist."};
    }
    return *measure;
}

template <typename T>
auto checked_insert(std::vector<T> &items, std::size_t index, T item) -> void
{
    if (index > items.size())
    {
        throw std::out_of_range{"Insert index is out of range."};
    }
    items.insert(std::next(items.begin(), static_cast<std::ptrdiff_t>(index)),
                 std::move(item));
}

template <typename T>
auto checked_erase(std::vector<T> &items, std::size_t index) -> void
{
    if (index >= items.size())
    {
        throw std::out_of_range{"Remove index is out of range."};
    }
    items.erase(std::next(items.begin(), static_cast<std::ptrdiff_t>(index)));
}

template <typename T>
auto checked_move(std::vector<T> &items, std::size_t from, std::size_t to) -> void
{
    if (from >= items.size() || to >= items.size())
    {
        throw std::out_of_range{"Move index is out of range."};
    }
    if (from == to)
    {
        return;
    }

    auto item = std::move(items[from]);
    items.erase(std::next(items.begin(), static_cast<std::ptrdiff_t>(from)));
    items.insert(std::next(items.begin(), static_cast<std::ptrdiff_t>(to)),
                 std::move(item));
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
            return;
        }
        if (index <= end + 1)
        {
            loop.end_column = end + 1;
        }
        return;
    }

    if (index <= end)
    {
        loop.end_column = end + 1;
        return;
    }
    if (index < start)
    {
        loop.start_column = start + 1;
        return;
    }
}

[[nodiscard]] auto shifted_after_remove(std::size_t column, std::size_t index,
                                        std::size_t new_size) -> std::size_t
{
    if (column == index)
    {
        return std::min(index, new_size - 1);
    }
    return column > index ? column - 1 : column;
}

[[nodiscard]] auto shifted_after_move(std::size_t column, std::size_t from,
                                      std::size_t to) -> std::size_t
{
    if (column == from)
    {
        return to;
    }
    if (from < to && column > from && column <= to)
    {
        return column - 1;
    }
    if (to < from && column >= to && column < from)
    {
        return column + 1;
    }
    return column;
}

} // namespace

auto make_default_measure_bank() -> MeasureBank
{
    return MeasureBank{
        .measures = {{.id = DEFAULT_MEASURE_ID, .measure = Measure{}}},
        .next_id = DEFAULT_MEASURE_ID + 1,
    };
}

auto make_default_composition() -> Composition
{
    return Composition{
        .columns = {CompositionColumn{}},
        .rows = {CompositionRow{
            .channel_id = DEFAULT_CHANNEL_ID,
            .cells = {DEFAULT_MEASURE_ID},
        }},
        .loop_region = LoopRegion{.start_column = 0, .end_column = 0},
    };
}

auto create_measure(MeasureBank &bank, Measure measure) -> MeasureId
{
    if (bank.next_id == 0)
    {
        throw std::invalid_argument{"Next measure ID must be nonzero."};
    }
    if (std::ranges::find(bank.measures, bank.next_id, &MeasureBankEntry::id) !=
        bank.measures.end())
    {
        throw std::invalid_argument{"Next measure ID is already in use."};
    }
    auto const id = bank.next_id++;
    bank.measures.push_back({.id = id, .measure = std::move(measure)});
    return id;
}

auto remove_measure(MeasureBank &bank, MeasureId id) -> bool
{
    auto const at = std::ranges::find(bank.measures, id, &MeasureBankEntry::id);
    if (at == bank.measures.end())
    {
        return false;
    }
    bank.measures.erase(at);
    return true;
}

auto duplicate_measure(MeasureBank &bank, MeasureId id) -> MeasureId
{
    auto measure = require_measure(bank, id);
    return create_measure(bank, std::move(measure));
}

auto find_measure(MeasureBank &bank, MeasureId id) -> Measure *
{
    auto const at = std::ranges::find(bank.measures, id, &MeasureBankEntry::id);
    return at == bank.measures.end() ? nullptr : &at->measure;
}

auto find_measure(MeasureBank const &bank, MeasureId id) -> Measure const *
{
    auto const at = std::ranges::find(bank.measures, id, &MeasureBankEntry::id);
    return at == bank.measures.end() ? nullptr : &at->measure;
}

auto update_measure(MeasureBank &bank, MeasureId id, Measure measure) -> bool
{
    auto *target = find_measure(bank, id);
    if (target == nullptr)
    {
        return false;
    }
    *target = std::move(measure);
    return true;
}

auto all_measures(MeasureBank const &bank) -> std::vector<MeasureBankEntry> const &
{
    return bank.measures;
}

auto insert_row(Composition &composition, std::size_t index, ChannelId channel_id)
    -> void
{
    auto row = CompositionRow{
        .channel_id = std::move(channel_id),
        .cells = std::vector<std::optional<MeasureId>>(composition.columns.size(),
                                                       std::nullopt),
    };
    checked_insert(composition.rows, index, std::move(row));
}

auto remove_row(Composition &composition, std::size_t index) -> void
{
    if (composition.rows.size() <= 1)
    {
        throw std::invalid_argument{"Composition must contain at least one row."};
    }
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
    {
        throw std::invalid_argument{"Channel ID must not be empty."};
    }
    require_row(composition, row).channel_id = std::move(channel_id);
}

auto insert_column(Composition &composition, std::size_t index,
                   sequence::TimeSignature length) -> void
{
    checked_insert(composition.columns, index, CompositionColumn{.length = length});
    for (auto &row : composition.rows)
    {
        checked_insert(row.cells, index, std::optional<MeasureId>{});
    }
    adjust_loop_after_insert(composition.loop_region, index);
}

auto remove_column(Composition &composition, std::size_t index) -> void
{
    if (composition.columns.size() <= 1)
    {
        throw std::invalid_argument{"Composition must contain at least one column."};
    }
    checked_erase(composition.columns, index);
    for (auto &row : composition.rows)
    {
        checked_erase(row.cells, index);
    }
    auto const new_size = composition.columns.size();
    composition.loop_region.start_column =
        shifted_after_remove(composition.loop_region.start_column, index, new_size);
    composition.loop_region.end_column =
        shifted_after_remove(composition.loop_region.end_column, index, new_size);
}

auto move_column(Composition &composition, std::size_t from, std::size_t to) -> void
{
    checked_move(composition.columns, from, to);
    for (auto &row : composition.rows)
    {
        checked_move(row.cells, from, to);
    }
    composition.loop_region.start_column =
        shifted_after_move(composition.loop_region.start_column, from, to);
    composition.loop_region.end_column =
        shifted_after_move(composition.loop_region.end_column, from, to);
}

auto set_column_length(Composition &composition, std::size_t column,
                       sequence::TimeSignature length) -> void
{
    require_column(composition, column).length = length;
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

auto assign_measure_reference(Composition &composition, std::size_t row,
                              std::size_t column, MeasureId id) -> void
{
    require_cell(composition, row, column) = id;
}

auto clear_measure_reference(Composition &composition, std::size_t row,
                             std::size_t column) -> void
{
    require_cell(composition, row, column) = std::nullopt;
}

auto measure_reference_at(Composition const &composition, std::size_t row,
                          std::size_t column) -> std::optional<MeasureId>
{
    auto const &target_row = require_row(composition, row);
    if (column >= target_row.cells.size())
    {
        throw std::out_of_range{"Composition column index is out of range."};
    }
    return target_row.cells[column];
}

auto arranged_measure(MeasureBank &bank, Composition const &composition,
                      ActiveMeasureTarget const &target) -> Measure &
{
    auto const measure_id =
        measure_reference_at(composition, target.row_index, target.column_index);
    if (!measure_id.has_value())
    {
        throw std::invalid_argument{"Active composition cell is empty."};
    }
    if (*measure_id != target.measure_id)
    {
        throw std::invalid_argument{
            "Active composition cell does not reference the requested measure."};
    }
    return require_measure(bank, target.measure_id);
}

auto arranged_measure(MeasureBank const &bank, Composition const &composition,
                      ActiveMeasureTarget const &target) -> Measure const &
{
    auto const measure_id =
        measure_reference_at(composition, target.row_index, target.column_index);
    if (!measure_id.has_value())
    {
        throw std::invalid_argument{"Active composition cell is empty."};
    }
    if (*measure_id != target.measure_id)
    {
        throw std::invalid_argument{
            "Active composition cell does not reference the requested measure."};
    }
    return require_measure(bank, target.measure_id);
}

auto default_arranged_measure(MeasureBank &bank, Composition const &composition)
    -> Measure &
{
    auto const measure_id = measure_reference_at(composition, 0, 0);
    if (!measure_id.has_value())
    {
        throw std::invalid_argument{"Default arrangement cell is empty."};
    }
    return require_measure(bank, *measure_id);
}

auto default_arranged_measure(MeasureBank const &bank, Composition const &composition)
    -> Measure const &
{
    auto const measure_id = measure_reference_at(composition, 0, 0);
    if (!measure_id.has_value())
    {
        throw std::invalid_argument{"Default arrangement cell is empty."};
    }
    return require_measure(bank, *measure_id);
}

auto default_column_length(Composition &composition) -> sequence::TimeSignature &
{
    return require_column(composition, 0).length;
}

auto default_column_length(Composition const &composition)
    -> sequence::TimeSignature const &
{
    return require_column(composition, 0).length;
}

} // namespace xen
