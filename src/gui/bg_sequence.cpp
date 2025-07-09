#include <xen/gui/bg_sequence.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <utility>
#include <variant>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/measure.hpp>
#include <sequence/utility.hpp>

#include <xen/gui/cell.hpp>
#include <xen/utility.hpp>

namespace xen::gui
{

auto generate_ir(sequence::Cell const &head_cell, std::size_t tuning_length) -> IR
{
    auto const impl = [tuning_length](auto const &self, sequence::Cell const &cell,
                                      float bounds_left, float bounds_right) -> IR {
        return std::visit(
            sequence::utility::overload{
                [](sequence::Rest const &) -> IR { return {}; },
                [&](sequence::Note const &n) -> IR {
                    auto const x = bounds_left;
                    auto const width = bounds_right - bounds_left;
                    return {NoteIR{n, x, width}};
                },
                [&](sequence::Sequence const &seq) -> IR {
                    auto const total_weight = std::accumulate(
                        std::cbegin(seq.cells), std::cend(seq.cells), 0.f,
                        [](float sum, auto const &c) { return sum + c.weight; });
                    if (total_weight <= 0.f)
                    {
                        return {};
                    }
                    auto result = IR{};
                    auto const width = bounds_right - bounds_left;
                    auto local_left = bounds_left;
                    auto local_right = local_left;
                    for (auto const &c : seq.cells)
                    {
                        local_right += (c.weight / total_weight) * width;
                        auto irs = self(self, c, local_left, local_right);
                        local_left = local_right;
                        result.insert(std::end(result),
                                      std::make_move_iterator(std::begin(irs)),
                                      std::make_move_iterator(std::end(irs)));
                    }
                    return result;
                },
            },
            cell.element);
    };

    return impl(impl, head_cell, 0.f, 1.f);
}

auto generate_window(Clock::duration fg_duration, Clock::time_point bg_start,
                     Clock::duration bg_duration, Clock::time_point now) -> IRWindow
{
    auto const iteration = (now - bg_start).count() / fg_duration.count();

    return {
        .offset = std::fmod(
            iteration * (fg_duration.count() / (float)bg_duration.count()), 1.f),
        .length = fg_duration.count() / (float)bg_duration.count(),
    };
}

auto apply_window(IR const &ir, IRWindow const &window, float trigger_offset) -> IR
{
    auto result = IR{};

    auto loop_count = 0.f;
    while (loop_count < window.length)
    {
        for (auto note_ir : ir)
        {
            auto const begin = std::clamp(loop_count + note_ir.x, window.offset,
                                          window.offset + window.length);
            auto const end = std::clamp(loop_count + note_ir.x + note_ir.width,
                                        window.offset, window.offset + window.length);

            if (begin < end)
            {
                // Normalize, Shift, and Wrap
                auto const left = std::fmod(
                    (begin - window.offset + trigger_offset) / window.length, 1.f);
                auto const right = [&window, &end, &trigger_offset] {
                    auto x = (end - window.offset + trigger_offset) / window.length;
                    // right is one past the end, and fmod will make 1 (valid) into a 0.
                    return x > 1.f ? std::fmod(x, 1.f) : x;
                }();

                if (left < right)
                {
                    note_ir.x = left;
                    note_ir.width = right - left;
                    result.push_back(note_ir);
                }
                else
                { // Split into two at boundary (wrap).
                    note_ir.x = left;
                    note_ir.width = 1.f - left;
                    result.push_back(note_ir);
                    note_ir.x = 0.f;
                    note_ir.width = right;
                    result.push_back(note_ir);
                }
            }
        }
        loop_count += 1.f;
    }

    return result;
}

auto get_bg_trigger_offset(Clock::time_point fg_start, Clock::time_point bg_start,
                           Clock::duration bg_duration) -> float
{
    auto const delta = (bg_start - fg_start).count();
    auto const offset = (float)delta / (float)bg_duration.count();
    return offset >= 0.0f ? offset : offset - (std::ceil(offset) * 2) + 1.f;
}

void paint_bg_active_sequence(IR const &ir, juce::Graphics &g,
                              juce::Rectangle<int> const &bounds,
                              std::size_t pitch_count, juce::Colour color)
{
    g.setColour(color);

    for (auto const &note_ir : ir)
    {
        auto const cell_bounds = juce::Rectangle<int>{
            bounds.getX() + (int)std::floor(note_ir.x * (float)bounds.getWidth()),
            bounds.getY(),
            (int)std::floor(note_ir.width * (float)bounds.getWidth()),
            bounds.getHeight(),
        };

        auto note_bounds = compute_note_bounds(cell_bounds, note_ir.note, pitch_count);

        auto const height_percent = 0.75f;
        auto const corner_percent = 0.2f;
        auto const original_height = note_bounds.getHeight();
        auto const new_height = (int)std::round(height_percent * original_height);
        auto const corner = corner_percent * original_height;

        auto const y_offset = (int)std::round((original_height - new_height) * 0.5f);
        note_bounds.setHeight(new_height);
        note_bounds.setY(note_bounds.getY() + y_offset);

        g.fillRoundedRectangle(note_bounds.toFloat(), corner);
    }
}

void paint_trigger_line(juce::Graphics &g, float percent_location, juce::Colour color)
{
    auto const bounds = g.getClipBounds().reduced(2, 7);
    auto const x =
        bounds.getX() + (int)std::floor(bounds.getWidth() * percent_location);
    g.setColour(color);
    g.drawRect(x, bounds.getY(), 1, bounds.getHeight(), 1);
}

auto calculate_duration(sequence::Measure const &m, DAWState const &daw)
    -> Clock::duration
{
    auto const sample_count = sequence::samples_count(m, daw.sample_rate, daw.bpm);
    return std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>((double)sample_count / (double)daw.sample_rate));
}

} // namespace xen::gui
