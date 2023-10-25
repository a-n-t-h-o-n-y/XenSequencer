#include <xen/selection.hpp>

#include <cstddef>
#include <stdexcept>
#include <variant>

#include <sequence/measure.hpp>
#include <sequence/sequence.hpp>
#include <sequence/utility.hpp>

namespace xen
{

auto get_selected_cell(sequence::Phrase &phrase, SelectedState const &selected)
    -> sequence::Cell *
{
    if (phrase.empty())
    {
        return nullptr;
    }
    // Start with the top-level Cell in the specified measure
    auto *current_cell = &phrase[selected.measure].cell;

    for (auto index : selected.cell)
    {
        // Assumption: current_cell must be a Sequence to navigate further
        if (std::holds_alternative<sequence::Sequence>(*current_cell))
        {
            current_cell = &std::get<sequence::Sequence>(*current_cell).cells[index];
        }
        else
        {
            throw std::invalid_argument("Invalid cell index path in selected state.");
        }
    }

    return current_cell;
}

auto get_selected_cell_const(sequence::Phrase const &phrase,
                             SelectedState const &selected) -> sequence::Cell const *
{
    if (phrase.empty())
    {
        return nullptr;
    }

    // Start with the top-level Cell in the specified measure
    sequence::Cell const *current_cell = &phrase[selected.measure].cell;

    for (auto index : selected.cell)
    {
        // Assumption: current_cell must be a Sequence to navigate further
        if (std::holds_alternative<sequence::Sequence>(*current_cell))
        {
            current_cell = &std::get<sequence::Sequence>(*current_cell).cells[index];
        }
        else
        {
            throw std::invalid_argument("Invalid cell index path in selected state.");
        }
    }

    return current_cell;
}

auto get_parent_of_selected(sequence::Phrase &phrase, SelectedState const &selected)
    -> sequence::Cell *
{
    if (selected.cell.empty())
    {
        return nullptr;
    }

    sequence::Cell *current_cell = &phrase[selected.measure].cell;

    for (std::size_t i = 0; i + 1 < selected.cell.size(); ++i)
    {
        current_cell =
            &(std::get<sequence::Sequence>(*current_cell).cells[selected.cell[i]]);
    }

    return current_cell;
}

auto get_parent_of_selected_const(sequence::Phrase const &phrase,
                                  SelectedState const &selected)
    -> sequence::Cell const *
{
    if (selected.cell.empty())
    {
        return nullptr;
    }

    sequence::Cell const *current_cell = &phrase[selected.measure].cell;

    for (std::size_t i = 0; i + 1 < selected.cell.size(); ++i)
    {
        current_cell =
            &(std::get<sequence::Sequence>(*current_cell).cells[selected.cell[i]]);
    }

    return current_cell;
}

auto get_sibling_count(sequence::Phrase const &phrase, SelectedState const &selected)
    -> std::size_t
{
    sequence::Cell const *parent = get_parent_of_selected_const(phrase, selected);
    if (parent == nullptr)
    {
        throw std::runtime_error("Cannot get sibling count of top-level Cell.");
    }
    return std::get<sequence::Sequence>(*parent).cells.size();
}

auto move_left(sequence::Phrase const &phrase, SelectedState selected,
               std::size_t amount) -> SelectedState
{

    if (selected.cell.empty()) // Change Measure
    {
        amount = amount % phrase.size();
        selected.measure = (selected.measure >= amount)
                               ? selected.measure - amount
                               : phrase.size() - (amount - selected.measure);
    }
    else // Change Cell
    {
        auto const parent_cells_size = get_sibling_count(phrase, selected);
        amount = amount % parent_cells_size;

        selected.cell.back() =
            (selected.cell.back() >= amount)
                ? selected.cell.back() - amount
                : parent_cells_size - (amount - selected.cell.back());
    }
    return selected;
}

auto move_right(sequence::Phrase const &phrase, SelectedState selected,
                std::size_t amount) -> SelectedState
{
    if (selected.cell.empty()) // Change Measure
    {
        selected.measure = (selected.measure + amount) % phrase.size();
    }
    else // Change Cell
    {
        auto const parent_cells_size = get_sibling_count(phrase, selected);
        selected.cell.back() = (selected.cell.back() + amount) % parent_cells_size;
    }
    return selected;
}

auto move_up(SelectedState selected, std::size_t amount) -> SelectedState
{
    for (auto i = std::size_t{0}; i < amount && !selected.cell.empty(); ++i)
    {
        selected.cell.pop_back();
    }
    return selected;
}

auto move_down(sequence::Phrase const &phrase, SelectedState selected,
               std::size_t amount) -> SelectedState
{
    for (auto i = std::size_t{0}; i < amount; ++i)
    {
        auto const *selected_cell = get_selected_cell_const(phrase, selected);
        if (selected_cell && std::holds_alternative<sequence::Sequence>(*selected_cell))
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