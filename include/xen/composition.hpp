#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <sequence/time_signature.hpp>

#include <xen/measure.hpp>

namespace xen
{

using MeasureId = std::uint64_t;
using OutputId = std::string;

inline constexpr auto DEFAULT_MEASURE_ID = MeasureId{1};
inline constexpr auto CURRENT_INSTANCE_OUTPUT_ID = "current";

struct ActiveMeasureTarget
{
    std::size_t row_index{};
    std::size_t column_index{};
    MeasureId measure_id{};

    auto operator==(ActiveMeasureTarget const &) const -> bool = default;
};

struct MeasureBankEntry
{
    MeasureId id{};
    std::optional<std::string> name{};
    Measure measure{};

    auto operator==(MeasureBankEntry const &) const -> bool = default;
};

struct MeasureBank
{
    std::vector<MeasureBankEntry> measures{};
    MeasureId next_id{DEFAULT_MEASURE_ID};

    auto operator==(MeasureBank const &) const -> bool = default;
};

struct CompositionColumn
{
    sequence::TimeSignature length{4, 4};

    auto operator==(CompositionColumn const &) const -> bool = default;
};

struct CompositionRow
{
    std::optional<std::string> name{};
    OutputId output_id{CURRENT_INSTANCE_OUTPUT_ID};
    std::vector<std::optional<MeasureId>> cells{};

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

[[nodiscard]] auto make_default_measure_bank() -> MeasureBank;
[[nodiscard]] auto make_default_composition() -> Composition;

[[nodiscard]] auto create_measure(MeasureBank &bank, Measure measure) -> MeasureId;
auto remove_measure(MeasureBank &bank, MeasureId id) -> bool;
[[nodiscard]] auto duplicate_measure(MeasureBank &bank, MeasureId id) -> MeasureId;
[[nodiscard]] auto find_measure(MeasureBank &bank, MeasureId id) -> Measure *;
[[nodiscard]] auto find_measure(MeasureBank const &bank, MeasureId id)
    -> Measure const *;
auto update_measure(MeasureBank &bank, MeasureId id, Measure measure) -> bool;
[[nodiscard]] auto all_measures(MeasureBank const &bank)
    -> std::vector<MeasureBankEntry> const &;

auto insert_row(Composition &composition, std::size_t index,
                OutputId output_id = CURRENT_INSTANCE_OUTPUT_ID) -> void;
auto remove_row(Composition &composition, std::size_t index) -> void;
auto move_row(Composition &composition, std::size_t from, std::size_t to) -> void;
auto assign_row_output(Composition &composition, std::size_t row, OutputId output_id)
    -> void;

auto insert_column(Composition &composition, std::size_t index,
                   sequence::TimeSignature length = {4, 4}) -> void;
auto remove_column(Composition &composition, std::size_t index) -> void;
auto move_column(Composition &composition, std::size_t from, std::size_t to) -> void;
auto set_column_length(Composition &composition, std::size_t column,
                       sequence::TimeSignature length) -> void;
auto set_loop_start(Composition &composition, std::size_t column) -> void;
auto set_loop_end(Composition &composition, std::size_t column) -> void;

auto assign_measure_reference(Composition &composition, std::size_t row,
                              std::size_t column, MeasureId id) -> void;
auto clear_measure_reference(Composition &composition, std::size_t row,
                             std::size_t column) -> void;
[[nodiscard]] auto measure_reference_at(Composition const &composition, std::size_t row,
                                        std::size_t column) -> std::optional<MeasureId>;

[[nodiscard]] auto arranged_measure(MeasureBank &bank, Composition const &composition,
                                    ActiveMeasureTarget const &target) -> Measure &;
[[nodiscard]] auto arranged_measure(MeasureBank const &bank,
                                    Composition const &composition,
                                    ActiveMeasureTarget const &target)
    -> Measure const &;
[[nodiscard]] auto default_arranged_measure(MeasureBank &bank,
                                            Composition const &composition)
    -> Measure &;
[[nodiscard]] auto default_arranged_measure(MeasureBank const &bank,
                                            Composition const &composition)
    -> Measure const &;
[[nodiscard]] auto default_column_length(Composition &composition)
    -> sequence::TimeSignature &;
[[nodiscard]] auto default_column_length(Composition const &composition)
    -> sequence::TimeSignature const &;

} // namespace xen
