#include <xen/command_catalog.hpp>

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <xen/string_manip.hpp>

#include "command_catalog_metadata_internal.hpp"

namespace xen
{
namespace
{

struct CatalogMetadataSeed
{
    std::string_view signature;
    std::string_view description;
};

constexpr auto metadata_seeds = std::to_array<CatalogMetadataSeed>({
    CatalogMetadataSeed{.signature = R"(welcome)", .description = R"(Display welcome message.)"},
    CatalogMetadataSeed{.signature = R"(version)", .description = R"(Print the current XenSequencer version.)"},
    CatalogMetadataSeed{.signature = R"(again)", .description = R"(Replay the previously executed command chain.)"},
    CatalogMetadataSeed{.signature = R"(commit)", .description = R"(Commit changes to history, mostly for internal use.)"},
    CatalogMetadataSeed{.signature = R"(reset)", .description = R"(Reset XenSequencer to its initial state.)"},
    CatalogMetadataSeed{.signature = R"(undo)", .description = R"(Revert state to before the last action.)"},
    CatalogMetadataSeed{.signature = R"(redo)", .description = R"(Reapply the last undone action.)"},
    CatalogMetadataSeed{.signature = R"(copy)", .description = R"(Copy the current selection into the shared copy buffer.)"},
    CatalogMetadataSeed{.signature = R"(cut)", .description = R"(Copy the current selection into the shared copy buffer and replace the selection with a Rest.)"},
    CatalogMetadataSeed{.signature = R"(paste)", .description = R"(Replace the current selection with the contents of the shared copy buffer.)"},
    CatalogMetadataSeed{.signature = R"(duplicate)", .description = R"(Duplicate the current selection to the next Cell.)"},
    CatalogMetadataSeed{.signature = R"(inputMode [InputMode: mode])", .description = R"(Change the input mode. This determines the behavior of the up/down keys.)"},
    CatalogMetadataSeed{.signature = R"(focus [String: component_id])", .description = R"(Deprecated. UI focus is handled by the UI adapter layer.)"},
    CatalogMetadataSeed{.signature = R"(show [String: component_id])", .description = R"(Deprecated. UI routing is handled by the UI adapter layer.)"},
    CatalogMetadataSeed{.signature = R"(load sequenceBank [String: filename])", .description = R"(Load the entire sequence bank into the plugin from file. filename must be located in the library's currently set sequence directory. Do not include the .xss extension in the filename you provide.)"},
    CatalogMetadataSeed{.signature = R"(load tuning [String: filename])", .description = R"(Load a tuning file (.scl) from the current `tunings` Library directory. Do not include the .scl extension in the filename you provide.)"},
    CatalogMetadataSeed{.signature = R"(load keys)", .description = R"(Deprecated command.)"},
    CatalogMetadataSeed{.signature = R"(load scales)", .description = R"(Load scales.yml and user_scales.yml.)"},
    CatalogMetadataSeed{.signature = R"(load chords)", .description = R"(Load chords.yml and user_chords.yml.)"},
    CatalogMetadataSeed{.signature = R"(save sequenceBank [String: filename])", .description = R"(Save the entire sequence bank to a file. The file will be located in the library's current sequence directory. Do not include the .xss extension in the filename you provide.)"},
    CatalogMetadataSeed{.signature = R"(libraryDirectory)", .description = R"(Display the path to the directory where the user library is stored.)"},
    CatalogMetadataSeed{.signature = R"(move left [Unsigned: amount=1])", .description = R"(Move the selection left, or wrap around.)"},
    CatalogMetadataSeed{.signature = R"(move right [Unsigned: amount=1])", .description = R"(Move the selection right, or wrap around.)"},
    CatalogMetadataSeed{.signature = R"(move up [Unsigned: amount=1])", .description = R"(Move the selection up one level to a parent sequence.)"},
    CatalogMetadataSeed{.signature = R"(move down [Unsigned: amount=1])", .description = R"(Move the selection down one level.)"},
    CatalogMetadataSeed{.signature = R"(note [Int: pitch=0] [Float: velocity=0.787402] [Float: delay=0] [Float: gate=1])", .description = R"(Create a new Note, overwritting the current selection.)"},
    CatalogMetadataSeed{.signature = R"(rest)", .description = R"(Create a new Rest, overwritting the current selection.)"},
    CatalogMetadataSeed{.signature = R"(delete)", .description = R"(Delete the current selection.)"},
    CatalogMetadataSeed{.signature = R"(split [Unsigned: count=2])", .description = R"(Duplicates the current selection into `count` equal parts, replacing the current selection.)"},
    CatalogMetadataSeed{.signature = R"(lift)", .description = R"(Bring the current selection up one level, replacing its parent sequence with itself.)"},
    CatalogMetadataSeed{.signature = R"([pattern] flip)", .description = R"(Flips Notes to Rests and Rests to Notes for the current selection. Works over sequences.)"},
    CatalogMetadataSeed{.signature = R"([pattern] fill note [Int: pitch=0] [Float: velocity=0.787402] [Float: delay=0] [Float: gate=1])", .description = R"(Fill the current selection with Notes, this works specifically over sequences.)"},
    CatalogMetadataSeed{.signature = R"([pattern] fill rest)", .description = R"(Fill the current selection with Rests, this works specifically over sequences.)"},
    CatalogMetadataSeed{.signature = R"(select sequence [Int: index])", .description = R"(Change the current sequence from the SequenceBank to `index`. Zero-based.)"},
    CatalogMetadataSeed{.signature = R"([pattern] set pitch [Int: pitch=0])", .description = R"(Set the pitch of all selected Notes.)"},
    CatalogMetadataSeed{.signature = R"([pattern] set octave [Int: octave=0])", .description = R"(Set the octave of all selected Notes.)"},
    CatalogMetadataSeed{.signature = R"([pattern] set velocity [Float: velocity=0.787402])", .description = R"(Set the velocity of all selected Notes.)"},
    CatalogMetadataSeed{.signature = R"([pattern] set delay [Float: delay=0])", .description = R"(Set the delay of all selected Notes.)"},
    CatalogMetadataSeed{.signature = R"([pattern] set gate [Float: gate=1])", .description = R"(Set the gate of all selected Notes.)"},
    CatalogMetadataSeed{.signature = R"(set sequence name [String: name] [Int: index=-1])", .description = R"(Set the name of a Sequence. If no index is given, set the name of the current Sequence.)"},
    CatalogMetadataSeed{.signature = R"(set sequence timeSignature [TimeSignature: timesignature=4/4] [Int: index=-1])", .description = R"(Set the time signature of a Sequence. If no index is given, set the time signature of the current Sequence.)"},
    CatalogMetadataSeed{.signature = R"(set baseFrequency [Float: freq=440])", .description = R"(Set the base note (pitch zero) frequency to `freq` Hz.)"},
    CatalogMetadataSeed{.signature = R"(set theme [String: name])", .description = R"(Deprecated. Theme changes are handled by the UI adapter layer.)"},
    CatalogMetadataSeed{.signature = R"(set scale [String: name])", .description = R"(Set the current scale by name.)"},
    CatalogMetadataSeed{.signature = R"(set mode [Unsigned: mode_index])", .description = R"(Set the mode of the current scale. [1, scale size].)"},
    CatalogMetadataSeed{.signature = R"(set translateDirection [String: direction])", .description = R"(Set the Scale's translate direction to either Up or Down.)"},
    CatalogMetadataSeed{.signature = R"(set key [Int: key=0])", .description = R"(Set the key to tranpose to, any integer value is valid.)"},
    CatalogMetadataSeed{.signature = R"(set weight [Float: value])", .description = R"(Set the weight of the selected cell.)"},
    CatalogMetadataSeed{.signature = R"([pattern] set weights [Float|Modulator: weight])", .description = R"(Set child-cell weights in the selected cell. This command defers commit.)"},
    CatalogMetadataSeed{.signature = R"(double sequence timeSignature [Int: index=-1])", .description = R"(Double the given Sequence's TimeSignature, or the currently selected Sequence's TimeSignature if index is -1.)"},
    CatalogMetadataSeed{.signature = R"(halve sequence timeSignature [Int: index=-1])", .description = R"(Halve the given Sequence's TimeSignature, or the currently selected Sequence's TimeSignature if index is -1.)"},
    CatalogMetadataSeed{.signature = R"([pattern] shift pitch [Int: amount=1])", .description = R"(Increment/Decrement the pitch of all selected Notes.)"},
    CatalogMetadataSeed{.signature = R"([pattern] shift octave [Int: amount=1])", .description = R"(Increment/Decrement the octave of all selected Notes.)"},
    CatalogMetadataSeed{.signature = R"([pattern] shift velocity [Float: amount=0.1])", .description = R"(Increment/Decrement the velocity of all selected Notes.)"},
    CatalogMetadataSeed{.signature = R"([pattern] shift delay [Float: amount=0.1])", .description = R"(Increment/Decrement the delay of all selected Notes.)"},
    CatalogMetadataSeed{.signature = R"([pattern] shift gate [Float: amount=0.1])", .description = R"(Increment/Decrement the gate of all selected Notes.)"},
    CatalogMetadataSeed{.signature = R"(shift selectedSequence [Int: amount])", .description = R"(Change the selected/displayed sequence by `amount`. This wraps around edges of the SequenceBank. `amount` can be positive or negative.)"},
    CatalogMetadataSeed{.signature = R"(shift scale [Int: amount=1])", .description = R"(Move Forward/Backward through the loaded Scales.)"},
    CatalogMetadataSeed{.signature = R"(shift scaleMode [Int: amount=1])", .description = R"(Increment/Decrement the mode of the current scale.)"},
    CatalogMetadataSeed{.signature = R"(shift translateDirection)", .description = R"(Flip the scale translate direction to the opposite of its current state.)"},
    CatalogMetadataSeed{.signature = R"(shift entireScale [Int: direction=1])", .description = R"(Shift translateDirection, mode, and scale in sequence. direction must be -1 or 1.)"},
    CatalogMetadataSeed{.signature = R"([pattern] randomize pitch [Int: min=-12] [Int: max=12])", .description = R"(Set the pitch of any selected Notes to a random value.)"},
    CatalogMetadataSeed{.signature = R"([pattern] randomize velocity [Float: min=0.01] [Float: max=1])", .description = R"(Set the velocity of any selected Notes to a random value.)"},
    CatalogMetadataSeed{.signature = R"([pattern] randomize delay [Float: min=0] [Float: max=0.95])", .description = R"(Set the delay of any selected Notes to a random value.)"},
    CatalogMetadataSeed{.signature = R"([pattern] randomize gate [Float: min=0] [Float: max=0.95])", .description = R"(Set the gate of any selected Notes to a random value.)"},
    CatalogMetadataSeed{.signature = R"([pattern] stretch [Unsigned: count=2])", .description = R"(Duplicates items in the current selection `count` times, replacing the current selection.<br><br>This is similar to `split`, the difference is this does not split sequences, it will traverse until it finds a Note or Rest and will then duplicate it. This can also take a Pattern, whereas split cannot.)"},
    CatalogMetadataSeed{.signature = R"([pattern] compress)", .description = R"(Keep items from the current selection that match the given Pattern, replacing the current selection.)"},
    CatalogMetadataSeed{.signature = R"(shuffle)", .description = R"(Randomly shuffle Notes and Rests in current selection.)"},
    CatalogMetadataSeed{.signature = R"(rotate [Int: amount=1])", .description = R"(Shift Cells in the current selection by `amount`.<br><br>Positive values shift right, negative values shift left.)"},
    CatalogMetadataSeed{.signature = R"(reverse)", .description = R"(Reverse the order of all Notes and Rests in the current selection.)"},
    CatalogMetadataSeed{.signature = R"([pattern] mirror [Int: centerPitch=0])", .description = R"(Mirror the note pitches of the current selection around `centerPitch`.)"},
    CatalogMetadataSeed{.signature = R"([pattern] quantize)", .description = R"(Set the delay to zero and gate to one for all Notes in the current selection.)"},
    CatalogMetadataSeed{.signature = R"(swing [Float: amount=0.1])", .description = R"(Set the delay of every other Note in the current selection to `amount`.)"},
    CatalogMetadataSeed{.signature = R"([pattern] step [Int: pitchDistance=1] [Float: velocityDistance=0])", .description = R"(Increments pitch and velocity of each child cell in the selection. If pattern is given, only adds to increments on cells that match the pattern.)"},
    CatalogMetadataSeed{.signature = R"([pattern] arp [String: chord="cycle"] [Int: inversion=-1])", .description = R"(Plays a given chord across the current selection, each interval in the chord is applied in order to child cells in the selection.)"},
    CatalogMetadataSeed{.signature = R"(drums [Unsigned: octaveSize=16] [Int: offset=1])", .description = R"(Enter 'Drum Mode' where the zero note becomes `offset` plus the lowest of the general midi drum notes and the number of notes displayed is increased to `octaveSize`.)"},
});

auto parse_argument(std::string const &argument_text) -> CatalogArgumentMetadata
{
    auto const colon_index = argument_text.find(':');
    if (colon_index == std::string::npos)
    {
        throw std::runtime_error("Invalid argument metadata: " + argument_text);
    }

    auto argument = CatalogArgumentMetadata{};
    argument.type = strip(argument_text.substr(0, colon_index));

    auto const payload = strip(argument_text.substr(colon_index + 1));
    auto const equals_index = payload.find('=');
    if (equals_index == std::string::npos)
    {
        argument.name = payload;
    }
    else
    {
        argument.name = strip(payload.substr(0, equals_index));
        argument.default_value = strip(payload.substr(equals_index + 1));
    }

    if (argument.type.empty() || argument.name.empty())
    {
        throw std::runtime_error("Invalid argument metadata: " + argument_text);
    }

    return argument;
}

auto parse_signature(CatalogMetadataSeed const &seed) -> CatalogCommandMetadata
{
    auto metadata = CatalogCommandMetadata{};
    metadata.description = std::string{seed.description};

    auto signature = strip(std::string{seed.signature});

    auto constexpr pattern_prefix = std::string_view{"[pattern]"};
    if (signature.rfind(pattern_prefix, 0) == 0)
    {
        metadata.accepts_pattern_prefix = true;
        signature = strip(signature.substr(pattern_prefix.size()));
    }

    auto args_begin = signature.find('[');
    auto const path_text = strip(signature.substr(0, args_begin));
    metadata.path = split(path_text, ' ');
    if (metadata.path.empty())
    {
        throw std::runtime_error("Command metadata is missing id path: " +
                                 std::string{seed.signature});
    }

    if (args_begin == std::string::npos)
    {
        return metadata;
    }

    auto cursor = args_begin;
    while (cursor < signature.size())
    {
        auto const open = signature.find('[', cursor);
        if (open == std::string::npos)
        {
            break;
        }

        auto const close = signature.find(']', open + 1);
        if (close == std::string::npos)
        {
            throw std::runtime_error("Unterminated argument metadata in signature: " +
                                     std::string{seed.signature});
        }

        auto const arg_text = strip(signature.substr(open + 1, close - open - 1));
        metadata.arguments.push_back(parse_argument(arg_text));
        cursor = close + 1;
    }

    return metadata;
}

auto build_metadata() -> std::vector<CatalogCommandMetadata>
{
    auto all = std::vector<CatalogCommandMetadata>{};
    all.reserve(metadata_seeds.size());

    auto seen_paths = std::unordered_set<std::string>{};
    seen_paths.reserve(metadata_seeds.size());

    for (auto const &seed : metadata_seeds)
    {
        auto metadata = parse_signature(seed);
        auto path_key = format_metadata_path(metadata);
        auto const path_key_lower = to_lower(path_key);

        if (!seen_paths.insert(path_key_lower).second)
        {
            throw std::runtime_error("Duplicate command metadata path: " + path_key);
        }

        all.push_back(std::move(metadata));
    }

    return all;
}

} // namespace

auto command_catalog_metadata_storage() -> std::vector<CatalogCommandMetadata> const &
{
    static auto const metadata = build_metadata();
    return metadata;
}

auto format_metadata_argument(CatalogArgumentMetadata const &argument) -> std::string
{
    auto result = std::string{"["};
    result += argument.type + ": " + argument.name;
    if (argument.default_value.has_value())
    {
        result += "=" + *argument.default_value;
    }
    result += "]";
    return result;
}

auto format_metadata_path(CatalogCommandMetadata const &metadata) -> std::string
{
    auto path = std::string{};
    auto separator = std::string{};
    for (auto const &token : metadata.path)
    {
        path += separator;
        path += token;
        separator = " ";
    }
    return path;
}

auto CommandCatalog::metadata() const -> std::vector<CatalogCommandMetadata> const &
{
    return command_catalog_metadata_storage();
}

auto command_metadata() -> std::vector<CatalogCommandMetadata> const &
{
    return default_command_catalog().metadata();
}

} // namespace xen
