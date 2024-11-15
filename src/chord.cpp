#include <xen/chord.hpp>

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <xen/constants.hpp>
#include <xen/user_directory.hpp>

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
    auto const system_node =
        YAML::LoadFile(get_system_chords_file().getFullPathName().toStdString());
    auto const user_node =
        YAML::LoadFile(get_user_chords_file().getFullPathName().toStdString());

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
    auto const it = std::ranges::find(chords, name, &Chord::name);
    if (it == std::end(chords))
    {
        throw std::runtime_error{"Chord not found."};
    }
    auto const next_it = std::next(it);
    if (next_it == std::end(chords))
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
    if (inversion < 0 || inversion >= (int)chord.intervals.size())
    {
        throw std::runtime_error{"Invalid inversion."};
    }

    auto inverted = chord.intervals;

    for (auto i = 0; i < inversion; ++i)
    {
        inverted[(std::size_t)i] += tuning_size;
    }

    std::rotate(std::begin(inverted), std::begin(inverted) + inversion,
                std::end(inverted));

    return inverted;
}

} // namespace xen
