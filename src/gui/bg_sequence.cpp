#include <xen/gui/bg_sequence.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numeric>
#include <utility>
#include <variant>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/utility.hpp>

namespace xen::gui
{

auto generate_ir(sequence::Cell const &head_cell, std::size_t tuning_length) -> IR
{
    using Rectangles = std::vector<juce::Rectangle<float>>;

    auto const impl = [tuning_length](auto const &self, sequence::Cell const &cell,
                                      float bounds_left,
                                      float bounds_right) -> Rectangles {
        return std::visit(
            sequence::utility::overload{
                [](sequence::Rest const &) -> Rectangles { return {}; },
                [&](sequence::Note const &n) -> Rectangles {
                    auto const rect_height = 1.f / (float)tuning_length;
                    auto const x = bounds_left;
                    auto const y = (n.pitch % (int)tuning_length + 1) * rect_height;
                    auto const width = bounds_right - bounds_left;
                    return {juce::Rectangle<float>{x, y, width, rect_height}};
                },
                [&](sequence::Sequence const &seq) -> Rectangles {
                    auto const total_weight = std::accumulate(
                        std::cbegin(seq.cells), std::cend(seq.cells), 0.f,
                        [](float sum, auto const &c) { return sum + c.weight; });
                    if (total_weight <= 0.f)
                    {
                        return {};
                    }
                    auto result = Rectangles{};
                    auto const width = bounds_right - bounds_left;
                    auto local_left = bounds_left;
                    auto local_right = bounds_right;
                    for (auto const &c : seq.cells)
                    {
                        local_right += (c.weight / total_weight) * width;
                        auto rects = self(self, c, local_left, local_right);
                        local_left = local_right;
                        result.insert(std::end(result),
                                      std::make_move_iterator(std::begin(rects)),
                                      std::make_move_iterator(std::end(rects)));
                    }
                    return result;
                },
            },
            cell.element);
    };

    return {
        .rectangles = impl(impl, head_cell, 0.f, 1.f),
    };
}

auto generate_window(Clock::time_point fg_start, Clock::duration fg_duration,
                     Clock::time_point bg_start, Clock::duration bg_duration,
                     Clock::time_point now) -> Window
{
    auto const bg_elapsed = now - bg_start;

    return {
        .offset =
            (float)(bg_elapsed % bg_duration).count() / (float)bg_duration.count(),
        .length =
            (float)(fg_duration - (bg_elapsed > fg_duration ? Clock::duration{0}
                                                            : bg_start - fg_start))
                .count() /
            (float)bg_duration.count(),
    };
}

auto apply_window(IR const &ir, Window const &window) -> IR
{
    auto result = IR{};
    auto const start_global = window.offset;
    auto const end_global = window.offset + window.length;

    // Determine which integer shifts of the base [0..1] period to consider
    auto const first_index = (int)std::floor(start_global);
    auto const last_index = (int)std::ceil(end_global);

    for (auto i = first_index; i < last_index; ++i)
    {
        for (auto const &rect : ir.rectangles)
        {
            auto const rect_start = rect.getX() + i;
            auto const rect_end = rect_start + rect.getWidth();

            // Compute overlap of [rect_start, rect_end] with [start_global, end_global]
            auto const overlap_start = std::max(rect_start, start_global);
            auto const overlap_end = std::min(rect_end, end_global);
            if (overlap_end > overlap_start)
            {
                // Map overlap into [0..1] of the window
                auto const new_x = (overlap_start - start_global) / window.length;
                auto const new_width = (overlap_end - overlap_start) / window.length;

                auto const new_rect = juce::Rectangle<float>{
                    new_x, rect.getY(), new_width, rect.getHeight()};
                result.rectangles.push_back(new_rect);
            }
        }
    }

    return result;
}

auto get_bg_offset_from_fg(Clock::time_point fg_start, Clock::duration fg_duration,
                           Clock::time_point bg_start, Clock::time_point now) -> float
{
    if (now - bg_start > fg_duration - (bg_start - fg_start))
    {
        return 0.f;
    }
    else
    {
        return (float)(now - fg_start).count() / (float)fg_duration.count();
    }
}

void paint_bg_active_sequence(IR const &ir, float bg_offset, juce::Graphics &g)
{
    auto const bounds = g.getClipBounds().toFloat();

    g.setColour(juce::Colours::lightblue.withAlpha(0.6f));

    for (auto const &rect : ir.rectangles)
    {
        auto const translated = rect.translated(-bg_offset, 0.0f);
        auto const pixel_rect = juce::Rectangle<float>{
            bounds.getX() + translated.getX() * bounds.getWidth(),
            bounds.getY() + translated.getY() * bounds.getHeight(),
            translated.getWidth() * bounds.getWidth(),
            translated.getHeight() * bounds.getHeight()};

        g.fillRect(pixel_rect);
    }
}

} // namespace xen::gui
