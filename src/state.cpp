#include <xen/state.hpp>

namespace xen
{

auto init_state() -> SequencerState
{
    auto const init_measure = sequence::Measure{
        .cell = sequence::Rest{},
        .time_signature = {4, 4},
    };

    auto init = SequencerState{
        .sequence_bank = {},
        .measure_names = {"Init Test"},
        .tuning =
            {
                .intervals = {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000,
                              1100},
                .octave = 1200,
            },
        .tuning_name = "12EDO",
        .base_frequency = 440.f,
    };

    for (auto &measure : init.sequence_bank)
    {
        measure = init_measure;
    }

    return init;
}

} // namespace xen