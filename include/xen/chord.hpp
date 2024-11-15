#pragma once

#include <string>
#include <vector>

namespace xen
{

/**
 * Struct to hold musical chord data.
 */
struct Chord
{
    std::string name; // Is case sensitive.
    std::vector<int> intervals;
};

/**
 * Loads in Chords from library directory's chords.yml and user_chords.yml files.
 */
[[nodiscard]] auto load_chords_from_files() -> std::vector<Chord>;

/**
 * Find a chord in the list of chords with the given name.
 *
 * @param chords The list of chords to search.
 * @param name The name of the chord to find. Case sensitive.
 * @return The chord with the given name.
 * @throws std::runtime_error If no chord with the given name is found.
 */
[[nodiscard]] auto find_chord(std::vector<Chord> const &chords, std::string const &name)
    -> Chord;

/**
 * Find the next chord in the list of chords with the given name, wrapping around \p
 * chords if necessary. Returns the first entry if the given name is not found.
 *
 * @param chords The list of chords to search.
 * @param name The name of the chord to find. Case sensitive.
 * @return The chord at the index after the chord with the given name.
 * @throws std::runtime_error chords is empty.
 */
[[nodiscard]] auto find_next_chord(std::vector<Chord> const &chords,
                                   std::string const &name) -> Chord;

/**
 * Applies an inversion to the given chord and returns the intervals.
 *
 * @param chord The chord to invert.
 * @param inversion The number of times to invert the chord.
 * @param tuning_size The size of the tuning.
 * @return The intervals of the inverted chord.
 * @throws std::runtime_error If the inversion is out of bounds.
 */
[[nodiscard]] auto invert_chord(Chord const &chord, int inversion,
                                std::size_t tuning_size) -> std::vector<int>;

/**
 * Adds one to the inversion of the given chord, wraps on chord interval size.
 *
 * @param chord The chord to increment the inversion of.
 * @param inversion The current inversion.
 * @return The new inversion.
 */
[[nodiscard]] auto increment_inversion(Chord const &chord, int inversion) -> int;

} // namespace xen
