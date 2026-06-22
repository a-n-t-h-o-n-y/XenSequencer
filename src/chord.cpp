#include <xen/chord.hpp>

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <xen/constants.hpp>
#include <xen/user_directory.hpp>

#include "numeric.hpp"

namespace YAML
{
template <>
struct convert<::xen::Chord>
{
    static auto decode(Node const &node, ::xen::Chord &chord) -> bool
    {
        if (!node["name"] || !node["intervals"])
        {
            return false;
        }
        chord.name = node["name"].as<std::string>();
        chord.intervals = node["intervals"].as<std::vector<int>>();
        return true;
    }
};
} // namespace YAML

namespace xen
{

auto load_chords_from_files() -> std::vector<Chord>
{
    return load_chords(get_system_chords_file().loadFileAsString().toStdString(),
                       get_user_chords_file().loadFileAsString().toStdString());
}

auto load_chords(std::string const &system_yaml, std::string const &user_yaml)
    -> std::vector<Chord>
{
    auto const system_node = YAML::Load(system_yaml);
    auto const user_node = YAML::Load(user_yaml);
    auto system_chords = system_node["chords"].as<std::vector<Chord>>();
    auto user_chords = std::vector<Chord>{};
    if (user_node["chords"])
    {
        user_chords = user_node["chords"].as<std::vector<Chord>>();
    }
    system_chords.insert(std::end(system_chords),
                         std::make_move_iterator(std::begin(user_chords)),
                         std::make_move_iterator(std::end(user_chords)));
    return system_chords;
}

auto find_chord(std::vector<Chord> const &chords, std::string const &name) -> Chord
{
    auto const it = std::ranges::find(chords, name, &Chord::name);
    if (it == std::end(chords))
    {
        throw std::runtime_error{"Chord not found."};
    }
    return *it;
}

auto find_next_chord(std::vector<Chord> const &chords, std::string const &name) -> Chord
{
    if (chords.empty())
    {
        throw std::runtime_error{"Chords parameter is empty."};
    }
    auto const it = std::ranges::find(chords, name, &Chord::name);
    if (it == std::end(chords)) // Not found
    {
        return chords.front();
    }
    auto const next_it = std::next(it);
    if (next_it == std::end(chords)) // Wrap around
    {
        return chords.front();
    }
    else
    {
        return *next_it;
    }
}

auto invert_chord(Chord const &chord, int inversion, std::size_t tuning_size)
    -> std::vector<int>
{
    if (chord.intervals.empty() || inversion < 0 ||
        static_cast<std::size_t>(inversion) >= chord.intervals.size())
    {
        throw std::runtime_error{"Invalid inversion."};
    }

    auto inverted = chord.intervals;
    auto const tuning_length =
        numeric::checked_cast<int>(tuning_size, "Tuning size exceeds int.");

    for (auto i = 0; i < inversion; ++i)
    {
        auto &interval = inverted[static_cast<std::size_t>(i)];
        interval = numeric::checked_add(interval, tuning_length,
                                        "Chord inversion exceeds int.");
    }

    std::rotate(std::begin(inverted), std::begin(inverted) + inversion,
                std::end(inverted));

    return inverted;
}

auto increment_inversion(Chord const &chord, int inversion) -> int
{
    if (chord.intervals.empty())
    {
        throw std::invalid_argument{"Chord intervals must not be empty."};
    }
    if (inversion < 0 || static_cast<std::size_t>(inversion) >= chord.intervals.size())
    {
        throw std::invalid_argument{"Invalid inversion."};
    }
    return static_cast<std::size_t>(inversion) + 1 == chord.intervals.size()
               ? 0
               : inversion + 1;
}

} // namespace xen
