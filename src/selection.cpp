#include <xen/selection.hpp>

#include <cstddef>
#include <stdexcept>
#include <variant>

#include <sequence/sequence.hpp>
#include <sequence/utility.hpp>

namespace xen
{

namespace
{

auto selected_sequence(sequence::Cell &cell) -> sequence::Sequence *
{
    if (cell.elements.size() != 1)
    {
        return nullptr;
    }

    return std::get_if<sequence::Sequence>(&cell.elements.front());
}

auto selected_sequence(sequence::Cell const &cell) -> sequence::Sequence const *
{
    if (cell.elements.size() != 1)
    {
        return nullptr;
    }

    return std::get_if<sequence::Sequence>(&cell.elements.front());
}

} // namespace

auto get_selected_cell(SequenceBank &bank, SelectedState const &selected)
    -> sequence::Cell &
{
    sequence::Cell *current_cell = &bank[selected.measure].cell;

    for (auto index : selected.cell)
    {
        auto *sequence = selected_sequence(*current_cell);
        if (sequence != nullptr)
        {
            current_cell = &sequence->cells[index];
        }
        else
        {
            throw std::invalid_argument("Invalid cell index path in selected state.");
        }
    }

    return *current_cell;
}

auto get_selected_cell_const(SequenceBank const &bank, SelectedState const &selected)
    -> sequence::Cell const &
{
    sequence::Cell const *current_cell = &bank[selected.measure].cell;

    for (auto index : selected.cell)
    {
        auto const *sequence = selected_sequence(*current_cell);
        if (sequence != nullptr)
        {
            current_cell = &sequence->cells[index];
        }
        else
        {
            throw std::invalid_argument("Invalid cell index path in selected state.");
        }
    }

    return *current_cell;
}

auto has_selected_element(SelectedState const &selected) -> bool
{
    return selected.element_index.has_value();
}

auto get_selected_element(SequenceBank &bank, SelectedState const &selected)
    -> sequence::MusicElement &
{
    if (!selected.element_index.has_value())
    {
        throw std::invalid_argument("No element is selected.");
    }

    auto &cell = get_selected_cell(bank, selected);
    return cell.elements.at(*selected.element_index);
}

auto get_selected_element_const(SequenceBank const &bank,
                                SelectedState const &selected)
    -> sequence::MusicElement const &
{
    if (!selected.element_index.has_value())
    {
        throw std::invalid_argument("No element is selected.");
    }

    auto const &cell = get_selected_cell_const(bank, selected);
    return cell.elements.at(*selected.element_index);
}

auto get_parent_of_selected(SequenceBank &bank, SelectedState const &selected)
    -> sequence::Cell *
{
    if (selected.cell.empty())
    {
        return nullptr;
    }

    sequence::Cell *current_cell = &bank[selected.measure].cell;

    for (auto i = std::size_t{0}; i + 1 < selected.cell.size(); ++i)
    {
        auto *sequence = selected_sequence(*current_cell);
        if (sequence == nullptr)
        {
            throw std::invalid_argument("Invalid cell index path in selected state.");
        }
        current_cell = &sequence->cells[selected.cell[i]];
    }

    return current_cell;
}

auto get_parent_of_selected_const(SequenceBank const &bank,
                                  SelectedState const &selected)
    -> sequence::Cell const *
{
    if (selected.cell.empty())
    {
        return nullptr;
    }

    sequence::Cell const *current_cell = &bank[selected.measure].cell;

    for (auto i = std::size_t{0}; i + 1 < selected.cell.size(); ++i)
    {
        auto const *sequence = selected_sequence(*current_cell);
        if (sequence == nullptr)
        {
            throw std::invalid_argument("Invalid cell index path in selected state.");
        }
        current_cell = &sequence->cells[selected.cell[i]];
    }

    return current_cell;
}

auto get_parent_cell_of_selection(SequenceBank &bank, SelectedState const &selected)
    -> sequence::Cell *
{
    if (has_selected_element(selected))
    {
        return &get_selected_cell(bank, selected);
    }

    return get_parent_of_selected(bank, selected);
}

auto get_parent_cell_of_selection_const(SequenceBank const &bank,
                                        SelectedState const &selected)
    -> sequence::Cell const *
{
    if (has_selected_element(selected))
    {
        return &get_selected_cell_const(bank, selected);
    }

    return get_parent_of_selected_const(bank, selected);
}

auto get_sibling_count(SequenceBank const &bank, SelectedState const &selected)
    -> std::size_t
{
    sequence::Cell const *parent = get_parent_of_selected_const(bank, selected);
    if (parent == nullptr)
    {
        throw std::runtime_error("Cannot get sibling count of top-level Cell.");
    }
    auto const *sequence = selected_sequence(*parent);
    if (sequence == nullptr)
    {
        throw std::runtime_error("Selected parent cell is not a navigable sequence.");
    }
    return sequence->cells.size();
}

auto move_left(SequenceBank const &bank, SelectedState selected, std::size_t amount)
    -> SelectedState
{
    selected.element_index.reset();
    if (!selected.cell.empty())
    {
        auto const parent_cells_size = get_sibling_count(bank, selected);
        amount = amount % parent_cells_size;

        selected.cell.back() =
            (selected.cell.back() >= amount)
                ? selected.cell.back() - amount
                : parent_cells_size - (amount - selected.cell.back());
    }
    return selected;
}

auto move_right(SequenceBank const &bank, SelectedState selected, std::size_t amount)
    -> SelectedState
{
    selected.element_index.reset();
    if (!selected.cell.empty())
    {
        auto const parent_cells_size = get_sibling_count(bank, selected);
        selected.cell.back() = (selected.cell.back() + amount) % parent_cells_size;
    }
    return selected;
}

auto move_up(SelectedState selected, std::size_t amount) -> SelectedState
{
    selected.element_index.reset();
    for (auto i = std::size_t{0}; i < amount && !selected.cell.empty(); ++i)
    {
        selected.cell.pop_back();
    }
    return selected;
}

auto move_down(SequenceBank const &bank, SelectedState selected, std::size_t amount)
    -> SelectedState
{
    selected.element_index.reset();
    for (auto i = std::size_t{0}; i < amount; ++i)
    {
        auto const &selected_cell = get_selected_cell_const(bank, selected);
        if (selected_sequence(selected_cell) != nullptr)
        {
            selected.cell.push_back(0);
        }
        else
        {
            break;
        }
    }

    return selected;
}

} // namespace xen
