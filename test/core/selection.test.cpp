#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include <sequence/sequence.hpp>

#include <xen/selection.hpp>

using namespace xen;

using SelectedState = SelectionPath;

namespace
{

auto element_step(std::size_t index) -> SelectionStep
{
    return SelectionStep{.kind = SelectionStepKind::Element, .index = index};
}

auto cell_step(std::size_t index) -> SelectionStep
{
    return SelectionStep{.kind = SelectionStepKind::SequenceCell, .index = index};
}

auto make_nested_measure() -> Measure
{
    auto measure = Measure{};
    measure.cell = sequence::Cell{
        .elements =
            {
                sequence::Note{0, 1.f, 0.f, 1.f},
                sequence::Sequence{
                    .cells =
                        {
                            sequence::Cell{
                                .elements = {
                                    sequence::Note{1, 1.f, 0.f, 1.f},
                                },
                                .weight = 1.f,
                            },
                            sequence::Cell{
                                .elements = {
                                    sequence::Note{2, 1.f, 0.f, 1.f},
                                    sequence::Sequence{
                                        .cells =
                                            {
                                                sequence::Cell{
                                                    .elements = {
                                                        sequence::Note{7, 0.5f, 0.1f, 0.8f},
                                                    },
                                                    .weight = 1.f,
                                                },
                                                sequence::Cell{
                                                    .elements = {},
                                                    .weight = 2.f,
                                                },
                                            },
                                    },
                                },
                                .weight = 1.f,
                            },
                        },
                },
                sequence::Sequence{
                    .cells =
                        {
                            sequence::Cell{
                                .elements = {
                                    sequence::Note{4, 1.f, 0.f, 1.f},
                                },
                                .weight = 1.f,
                            },
                            sequence::Cell{
                                .elements = {
                                    sequence::Note{5, 1.f, 0.f, 1.f},
                                },
                                .weight = 1.f,
                            },
                            sequence::Cell{
                                .elements = {
                                    sequence::Note{6, 1.f, 0.f, 1.f},
                                },
                                .weight = 1.f,
                            },
                            sequence::Cell{
                                .elements = {
                                    sequence::Note{8, 1.f, 0.f, 1.f},
                                    sequence::Sequence{
                                        .cells =
                                            {
                                                sequence::Cell{
                                                    .elements = {
                                                        sequence::Note{9, 1.f, 0.f, 1.f},
                                                    },
                                                    .weight = 1.f,
                                                },
                                                sequence::Cell{
                                                    .elements = {
                                                        sequence::Sequence{
                                                            .cells =
                                                                {
                                                                    sequence::Cell{
                                                                        .elements = {
                                                                            sequence::Note{10, 0.7f, 0.2f, 0.9f},
                                                                        },
                                                                        .weight = 1.f,
                                                                    },
                                                                },
                                                        },
                                                    },
                                                    .weight = 1.f,
                                                },
                                            },
                                    },
                                },
                                .weight = 1.f,
                            },
                        },
                },
            },
        .weight = 1.f,
    };
    return measure;
}

} // namespace

TEST_CASE("Empty selection path resolves to the root cell", "[core][selection]")
{
    auto const measure = make_nested_measure();
    auto const selected = SelectedState{};

    CHECK(selection_kind(selected) == SelectionKind::Cell);
    CHECK(&get_selected_cell_const(measure, selected) == &measure.cell);
    CHECK(get_parent_of_selected_const(measure, selected) == nullptr);
}

TEST_CASE("Typed selection path resolves nested sibling sequences explicitly",
          "[core][selection]")
{
    auto const measure = make_nested_measure();
    auto const selected = SelectedState{
        .path = {
            element_step(2),
            cell_step(3),
            element_step(1),
            cell_step(1),
            element_step(0),
            cell_step(0),
        },
    };

    CHECK(selection_kind(selected) == SelectionKind::Cell);

    auto const &cell = get_selected_cell_const(measure, selected);
    REQUIRE(cell.elements.size() == 1);
    REQUIRE(std::holds_alternative<sequence::Note>(cell.elements.front()));
    CHECK(std::get<sequence::Note>(cell.elements.front()).pitch == 10);

    auto const *parent_cell = get_parent_cell_of_selection_const(measure, selected);
    REQUIRE(parent_cell != nullptr);
    REQUIRE(parent_cell->elements.size() == 1);
    REQUIRE(std::holds_alternative<sequence::Sequence>(parent_cell->elements.front()));
}

TEST_CASE("Invalid typed selection paths fail clearly", "[core][selection]")
{
    auto const measure = make_nested_measure();

    CHECK_THROWS_AS(get_selected_cell_const(
                        measure, SelectedState{.path = {cell_step(0)}}),
                    std::invalid_argument);
    CHECK_THROWS_AS(get_selected_cell_const(
                        measure,
                        SelectedState{.path = {element_step(2), element_step(0)}}),
                    std::invalid_argument);
    CHECK_THROWS_AS(get_selected_cell_const(
                        measure,
                        SelectedState{.path = {element_step(0), cell_step(0)}}),
                    std::invalid_argument);
}

TEST_CASE("Move up pops one logical selection level", "[core][selection]")
{
    auto const measure = make_nested_measure();

    auto const cell_selection = SelectedState{
        .path = {element_step(2), cell_step(3), element_step(1), cell_step(1)},
    };
    CHECK(move_up(measure, cell_selection) ==
          SelectedState{.path = {element_step(2), cell_step(3), element_step(1)}});

    auto const element_selection = SelectedState{
        .path = {element_step(2), cell_step(3), element_step(1)},
    };
    CHECK(move_up(measure, element_selection) ==
          SelectedState{.path = {element_step(2), cell_step(3)}});

    auto const sequence_child_selection = SelectedState{
        .path = {
            element_step(2),
            cell_step(3),
            element_step(1),
            cell_step(1),
            element_step(0),
            cell_step(0),
        },
    };
    CHECK(move_up(measure, sequence_child_selection) ==
          SelectedState{.path = {
              element_step(2),
              cell_step(3),
              element_step(1),
              cell_step(1),
          }});
}

TEST_CASE("Move down enters the first element of a selected Cell",
          "[core][selection]")
{
    auto const measure = make_nested_measure();

    auto const sequence_element = SelectedState{
        .path = {element_step(2)},
    };
    CHECK(move_down(measure, sequence_element) ==
          SelectedState{.path = {element_step(2), cell_step(0)}});

    auto const cell_selection = SelectedState{
        .path = {element_step(2), cell_step(3)},
    };
    CHECK(move_down(measure, cell_selection) ==
          SelectedState{.path = {element_step(2), cell_step(3), element_step(0)}});

    auto const singleton_note_cell = SelectedState{
        .path = {element_step(2), cell_step(0)},
    };
    CHECK(move_down(measure, singleton_note_cell) == singleton_note_cell);

    auto const singleton_sequence_cell = SelectedState{
        .path = {
            element_step(2),
            cell_step(3),
            element_step(1),
            cell_step(1),
        },
    };
    CHECK(move_down(measure, singleton_sequence_cell) ==
          SelectedState{.path = {
              element_step(2),
              cell_step(3),
              element_step(1),
              cell_step(1),
              element_step(0),
              cell_step(0),
          }});

    auto const sequence_first_child = SelectedState{
        .path = {
            element_step(2),
            cell_step(3),
            element_step(1),
            cell_step(1),
            element_step(0),
            cell_step(0),
        },
    };
    CHECK(move_up(measure, sequence_first_child) ==
          SelectedState{.path = {
              element_step(2),
              cell_step(3),
              element_step(1),
              cell_step(1),
          }});

    auto const note_element = SelectedState{
        .path = {element_step(0)},
    };
    CHECK(move_down(measure, note_element) == note_element);
}

TEST_CASE("Move left and right operate on sibling cells or elements",
          "[core][selection]")
{
    auto const measure = make_nested_measure();
    auto const selected = SelectedState{
        .path = {element_step(2), cell_step(3)},
    };

    CHECK(move_left(measure, selected, 1) ==
          SelectedState{.path = {element_step(2), cell_step(2)}});
    CHECK(move_right(measure, selected, 2) ==
          SelectedState{.path = {element_step(2), cell_step(1)}});
    CHECK(move_left(measure, SelectedState{}, 1) == SelectedState{});
    CHECK(move_right(measure, SelectedState{}, 1) == SelectedState{});

    auto const element_selected = SelectedState{
        .path = {element_step(2), cell_step(3), element_step(0)},
    };

    CHECK(move_right(measure, element_selected, 1) ==
          SelectedState{.path = {element_step(2), cell_step(3), element_step(1)}});
    CHECK(move_left(measure, element_selected, 1) ==
          SelectedState{.path = {element_step(2), cell_step(3), element_step(1)}});
}
