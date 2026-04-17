#include <xen/selection.hpp>

#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <variant>

#include <sequence/sequence.hpp>
#include <sequence/utility.hpp>

namespace xen
{

namespace
{

template <typename MeasureType>
auto resolve_path(MeasureType &measure, SelectedState const &selected)
{
    using CellPointer = std::conditional_t<std::is_const_v<MeasureType>,
                                           sequence::Cell const *, sequence::Cell *>;
    using ElementPointer =
        std::conditional_t<std::is_const_v<MeasureType>, sequence::MusicElement const *,
                           sequence::MusicElement *>;

    auto current_cell = CellPointer{&measure.cell};
    auto current_element = ElementPointer{nullptr};

    for (auto const &step : selected.path)
    {
        if (current_cell != nullptr)
        {
            if (step.kind != SelectionStepKind::Element)
            {
                throw std::invalid_argument("Invalid selection path.");
            }
            if (step.index >= current_cell->elements.size())
            {
                throw std::invalid_argument("Invalid selection path.");
            }

            current_element = &current_cell->elements[step.index];
            current_cell = nullptr;
            continue;
        }

        if (step.kind != SelectionStepKind::SequenceCell)
        {
            throw std::invalid_argument("Invalid selection path.");
        }

        auto *sequence = std::get_if<sequence::Sequence>(current_element);
        if (sequence == nullptr || step.index >= sequence->cells.size())
        {
            throw std::invalid_argument("Invalid selection path.");
        }

        current_cell = &sequence->cells[step.index];
        current_element = nullptr;
    }

    return std::pair{current_cell, current_element};
}

auto selection_parent(SelectedState selected) -> SelectedState
{
    if (!selected.path.empty())
    {
        selected.path.pop_back();
    }
    return selected;
}

} // namespace

auto selection_kind(SelectedState const &selected) -> SelectionKind
{
    if (selected.path.empty() ||
        selected.path.back().kind == SelectionStepKind::SequenceCell)
    {
        return SelectionKind::Cell;
    }

    return SelectionKind::Element;
}

auto select_element_in_cell(SelectedState selected, std::size_t index)
    -> SelectedState
{
    if (selection_kind(selected) != SelectionKind::Cell)
    {
        throw std::invalid_argument("Selection must be a Cell.");
    }

    selected.path.push_back({.kind = SelectionStepKind::Element, .index = index});
    return selected;
}

auto select_sequence_cell(SelectedState selected, std::size_t index)
    -> SelectedState
{
    if (selection_kind(selected) != SelectionKind::Element)
    {
        throw std::invalid_argument("Selection must be a MusicElement.");
    }

    selected.path.push_back({.kind = SelectionStepKind::SequenceCell, .index = index});
    return selected;
}

auto select_parent_cell(SelectedState selected) -> SelectedState
{
    if (selection_kind(selected) == SelectionKind::Element)
    {
        return selection_parent(std::move(selected));
    }

    if (selected.path.size() >= 2)
    {
        selected.path.pop_back();
        selected.path.pop_back();
    }
    else
    {
        selected.path.clear();
    }

    return selected;
}

auto get_selected_cell(Measure &measure, SelectedState const &selected)
    -> sequence::Cell &
{
    auto const [current_cell, current_element] = resolve_path(measure, selected);
    if (current_element != nullptr || current_cell == nullptr)
    {
        throw std::invalid_argument("Selection does not resolve to a Cell.");
    }
    return *current_cell;
}

auto get_selected_cell_const(Measure const &measure, SelectedState const &selected)
    -> sequence::Cell const &
{
    auto const [current_cell, current_element] = resolve_path(measure, selected);
    if (current_element != nullptr || current_cell == nullptr)
    {
        throw std::invalid_argument("Selection does not resolve to a Cell.");
    }
    return *current_cell;
}

auto get_selected_element(Measure &measure, SelectedState const &selected)
    -> sequence::MusicElement &
{
    auto const [current_cell, current_element] = resolve_path(measure, selected);
    if (current_cell != nullptr || current_element == nullptr)
    {
        throw std::invalid_argument("Selection does not resolve to a MusicElement.");
    }
    return *current_element;
}

auto get_selected_element_const(Measure const &measure,
                                SelectedState const &selected)
    -> sequence::MusicElement const &
{
    auto const [current_cell, current_element] = resolve_path(measure, selected);
    if (current_cell != nullptr || current_element == nullptr)
    {
        throw std::invalid_argument("Selection does not resolve to a MusicElement.");
    }
    return *current_element;
}

auto get_selected_sequence(Measure &measure, SelectedState const &selected)
    -> sequence::Sequence &
{
    auto &element = get_selected_element(measure, selected);
    auto *sequence = std::get_if<sequence::Sequence>(&element);
    if (sequence == nullptr)
    {
        throw std::invalid_argument("Selected MusicElement is not a Sequence.");
    }
    return *sequence;
}

auto get_selected_sequence_const(Measure const &measure,
                                 SelectedState const &selected)
    -> sequence::Sequence const &
{
    auto const &element = get_selected_element_const(measure, selected);
    auto const *sequence = std::get_if<sequence::Sequence>(&element);
    if (sequence == nullptr)
    {
        throw std::invalid_argument("Selected MusicElement is not a Sequence.");
    }
    return *sequence;
}

auto get_selected_element_index(SelectedState const &selected) -> std::size_t
{
    if (selection_kind(selected) != SelectionKind::Element || selected.path.empty())
    {
        throw std::invalid_argument("Selection does not resolve to a MusicElement.");
    }
    return selected.path.back().index;
}

auto get_selected_cell_index(SelectedState const &selected) -> std::size_t
{
    if (selection_kind(selected) != SelectionKind::Cell || selected.path.empty())
    {
        throw std::invalid_argument("Selection does not resolve to a child Cell.");
    }
    return selected.path.back().index;
}

auto get_parent_of_selected(Measure &measure, SelectedState const &selected)
    -> sequence::Cell *
{
    if (selection_kind(selected) != SelectionKind::Cell)
    {
        throw std::invalid_argument("Selection does not resolve to a Cell.");
    }

    if (selected.path.empty())
    {
        return nullptr;
    }

    return get_parent_cell_of_selection(measure, selection_parent(selected));
}

auto get_parent_of_selected_const(Measure const &measure,
                                  SelectedState const &selected)
    -> sequence::Cell const *
{
    if (selection_kind(selected) != SelectionKind::Cell)
    {
        throw std::invalid_argument("Selection does not resolve to a Cell.");
    }

    if (selected.path.empty())
    {
        return nullptr;
    }

    return get_parent_cell_of_selection_const(measure, selection_parent(selected));
}

auto get_parent_sequence_of_selected_cell(Measure &measure,
                                          SelectedState const &selected)
    -> sequence::Sequence *
{
    if (selection_kind(selected) != SelectionKind::Cell || selected.path.empty())
    {
        return nullptr;
    }

    auto parent_selection = selection_parent(selected);
    return &get_selected_sequence(measure, parent_selection);
}

auto get_parent_sequence_of_selected_cell_const(Measure const &measure,
                                                SelectedState const &selected)
    -> sequence::Sequence const *
{
    if (selection_kind(selected) != SelectionKind::Cell || selected.path.empty())
    {
        return nullptr;
    }

    auto parent_selection = selection_parent(selected);
    return &get_selected_sequence_const(measure, parent_selection);
}

auto get_parent_cell_of_selection(Measure &measure, SelectedState const &selected)
    -> sequence::Cell *
{
    if (selection_kind(selected) == SelectionKind::Element)
    {
        return &get_selected_cell(measure, selection_parent(selected));
    }

    return get_parent_of_selected(measure, selected);
}

auto get_parent_cell_of_selection_const(Measure const &measure,
                                        SelectedState const &selected)
    -> sequence::Cell const *
{
    if (selection_kind(selected) == SelectionKind::Element)
    {
        return &get_selected_cell_const(measure, selection_parent(selected));
    }

    return get_parent_of_selected_const(measure, selected);
}

auto get_sibling_count(Measure const &measure, SelectedState const &selected)
    -> std::size_t
{
    auto const *sequence = get_parent_sequence_of_selected_cell_const(measure, selected);
    if (sequence == nullptr)
    {
        throw std::runtime_error("Cannot get sibling count of top-level Cell.");
    }
    return sequence->cells.size();
}

auto move_left(Measure const &measure, SelectedState selected, std::size_t amount)
    -> SelectedState
{
    if (selected.path.empty())
    {
        return selected;
    }

    if (selection_kind(selected) == SelectionKind::Cell)
    {
        auto const parent_cells_size = get_sibling_count(measure, selected);
        if (parent_cells_size == 0)
        {
            return selected;
        }

        amount = amount % parent_cells_size;

        auto &index = selected.path.back().index;
        index = (index >= amount) ? index - amount
                                  : parent_cells_size - (amount - index);
    }
    else
    {
        auto const parent_cell = get_parent_cell_of_selection_const(measure, selected);
        if (parent_cell == nullptr || parent_cell->elements.empty())
        {
            return selected;
        }

        auto const element_count = parent_cell->elements.size();
        amount = amount % element_count;
        auto &index = selected.path.back().index;
        index = (index >= amount) ? index - amount
                                  : element_count - (amount - index);
    }
    return selected;
}

auto move_right(Measure const &measure, SelectedState selected, std::size_t amount)
    -> SelectedState
{
    if (selected.path.empty())
    {
        return selected;
    }

    if (selection_kind(selected) == SelectionKind::Cell)
    {
        auto const parent_cells_size = get_sibling_count(measure, selected);
        if (parent_cells_size == 0)
        {
            return selected;
        }

        selected.path.back().index =
            (selected.path.back().index + amount) % parent_cells_size;
    }
    else
    {
        auto const parent_cell = get_parent_cell_of_selection_const(measure, selected);
        if (parent_cell == nullptr || parent_cell->elements.empty())
        {
            return selected;
        }

        selected.path.back().index =
            (selected.path.back().index + amount) % parent_cell->elements.size();
    }
    return selected;
}

auto move_up(Measure const &measure, SelectedState selected, std::size_t amount)
    -> SelectedState
{
    for (auto i = std::size_t{0}; i < amount && !selected.path.empty(); ++i)
    {
        if (selection_kind(selected) == SelectionKind::Element)
        {
            selected.path.pop_back();
            continue;
        }

        auto const *parent_cell = get_parent_cell_of_selection_const(measure, selected);
        if (parent_cell != nullptr && parent_cell->elements.size() == 1 &&
            std::holds_alternative<sequence::Sequence>(parent_cell->elements.front()))
        {
            selected.path.pop_back();
            selected.path.pop_back();
            continue;
        }

        selected.path.pop_back();
    }
    return selected;
}

auto move_down(Measure const &measure, SelectedState selected, std::size_t amount)
    -> SelectedState
{
    for (auto i = std::size_t{0}; i < amount; ++i)
    {
        if (selection_kind(selected) == SelectionKind::Cell)
        {
            auto const &cell = get_selected_cell_const(measure, selected);
            if (cell.elements.empty())
            {
                break;
            }

            if (cell.elements.size() == 1)
            {
                auto const &element = cell.elements.front();
                if (auto const *sequence = std::get_if<sequence::Sequence>(&element);
                    sequence != nullptr && !sequence->cells.empty())
                {
                    selected = select_element_in_cell(std::move(selected), 0);
                    selected = select_sequence_cell(std::move(selected), 0);
                    continue;
                }

                break;
            }

            selected = select_element_in_cell(std::move(selected), 0);
            continue;
        }

        auto const &element = get_selected_element_const(measure, selected);
        auto const *sequence = std::get_if<sequence::Sequence>(&element);
        if (sequence == nullptr || sequence->cells.empty())
        {
            break;
        }

        selected = select_sequence_cell(std::move(selected), 0);
    }

    return selected;
}

} // namespace xen
