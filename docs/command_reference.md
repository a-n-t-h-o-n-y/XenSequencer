# Command Reference (v0.3.0)

name | signature | description
---- | --------- | -----------
welcome | `welcome` | Display welcome message.
version | `version` | Print the current XenSequencer version.
reset | `reset` | Reset XenSequencer to its initial state.
undo | `undo` | Revert state to before the last action.
redo | `redo` | Reapply the last undone action.
copy | `copy` | Copy the current selection into the shared copy buffer.
cut | `cut` | Copy the current selection into the shared copy buffer and replace the selection with a Rest.
paste | `paste` | Replace the current selection with the contents of the shared copy buffer.
duplicate | `duplicate` | Duplicate the current selection to the next Cell.
inputMode | `inputMode [InputMode: mode]` | Change the input mode. This determines the behavior of the up/down keys.
focus | `focus [String: component_id]` | Focus on a specific component.
show | `show [String: component_id]` | Update the GUI to display the specified component.
load sequenceBank | `load sequenceBank [String: filename]` | Load the entire sequence bank into the plugin from file. filename must be located in the library's currently set sequence directory. Do not include the .xss extension in the filename you provide.
load tuning | `load tuning [String: filename]` | Load a tuning file (.scl) from the current `tunings` Library directory. Do not include the .scl extension in the filename you provide.
load keys | `load keys` | Load keys.yml and user_keys.yml.
load scales | `load scales` | Load scales.yml and user_scales.yml.
load chords | `load chords` | Load chords.yml and user_chords.yml.
save sequenceBank | `save sequenceBank [String: filename]` | Save the entire sequence bank to a file. The file will be located in the library's current sequence directory. Do not include the .xss extension in the filename you provide.
libraryDirectory | `libraryDirectory` | Display the path to the directory where the user library is stored.
move left | `move left [Unsigned: amount=1]` | Move the selection left, or wrap around.
move right | `move right [Unsigned: amount=1]` | Move the selection right, or wrap around.
move up | `move up [Unsigned: amount=1]` | Move the selection up one level to a parent sequence.
move down | `move down [Unsigned: amount=1]` | Move the selection down one level.
note | `note [Int: pitch=0] [Float: velocity=0.787402] [Float: delay=0] [Float: gate=1]` | Create a new Note, overwritting the current selection.
rest | `rest` | Create a new Rest, overwritting the current selection.
delete | `delete` | Delete the current selection.
split | `split [Unsigned: count=2]` | Duplicates the current selection into `count` equal parts, replacing the current selection.
lift | `lift` | Bring the current selection up one level, replacing its parent sequence with itself.
flip | `[pattern] flip` | Flips Notes to Rests and Rests to Notes for the current selection. Works over sequences.
fill note | `[pattern] fill note [Int: pitch=0] [Float: velocity=0.787402] [Float: delay=0] [Float: gate=1]` | Fill the current selection with Notes, this works specifically over sequences.
fill rest | `[pattern] fill rest` | Fill the current selection with Rests, this works specifically over sequences.
select sequence | `select sequence [Int: index]` | Change the current sequence from the SequenceBank to `index`. Zero-based.
set pitch | `[pattern] set pitch [Int: pitch=0]` | Set the pitch of all selected Notes.
set octave | `[pattern] set octave [Int: octave=0]` | Set the octave of all selected Notes.
set velocity | `[pattern] set velocity [Float: velocity=0.787402]` | Set the velocity of all selected Notes.
set delay | `[pattern] set delay [Float: delay=0]` | Set the delay of all selected Notes.
set gate | `[pattern] set gate [Float: gate=1]` | Set the gate of all selected Notes.
set sequence name | `set sequence name [String: name] [Int: index=-1]` | Set the name of a Sequence. If no index is given, set the name of the current Sequence.
set sequence timeSignature | `set sequence timeSignature [TimeSignature: timesignature=4/4] [Int: index=-1]` | Set the time signature of a Sequence. If no index is given, set the time signature of the current Sequence.
set baseFrequency | `set baseFrequency [Float: freq=440]` | Set the base note (pitch zero) frequency to `freq` Hz.
set theme | `set theme [String: name]` | Set the color theme of the app by name.
set scale | `set scale [String: name]` | Set the current scale by name.
set mode | `set mode [Unsigned: mode_index]` | Set the mode of the current scale. [1, scale size].
set translateDirection | `set translateDirection [String: direction]` | Set the Scale's translate direction to either Up or Down.
set key | `set key [Int: key=0]` | Set the key to tranpose to, any integer value is valid.
double sequence timeSignature | `double sequence timeSignature [Int: index=-1]` | Double the given Sequence's TimeSignature, or the currently selected Sequence's TimeSignature if index is -1.
halve sequence timeSignature | `halve sequence timeSignature [Int: index=-1]` | Halve the given Sequence's TimeSignature, or the currently selected Sequence's TimeSignature if index is -1.
shift pitch | `[pattern] shift pitch [Int: amount=1]` | Increment/Decrement the pitch of all selected Notes.
shift octave | `[pattern] shift octave [Int: amount=1]` | Increment/Decrement the octave of all selected Notes.
shift velocity | `[pattern] shift velocity [Float: amount=0.1]` | Increment/Decrement the velocity of all selected Notes.
shift delay | `[pattern] shift delay [Float: amount=0.1]` | Increment/Decrement the delay of all selected Notes.
shift gate | `[pattern] shift gate [Float: amount=0.1]` | Increment/Decrement the gate of all selected Notes.
shift selectedSequence | `shift selectedSequence [Int: amount]` | Change the selected/displayed sequence by `amount`. This wraps around edges of the SequenceBank. `amount` can be positive or negative.
shift scale | `shift scale [Int: amount=1]` | Move Forward/Backward through the loaded Scales.
shift scaleMode | `shift scaleMode [Int: amount=1]` | Increment/Decrement the mode of the current scale.
randomize pitch | `[pattern] randomize pitch [Int: min=-12] [Int: max=12]` | Set the pitch of any selected Notes to a random value.
randomize velocity | `[pattern] randomize velocity [Float: min=0.01] [Float: max=1]` | Set the velocity of any selected Notes to a random value.
randomize delay | `[pattern] randomize delay [Float: min=0] [Float: max=0.95]` | Set the delay of any selected Notes to a random value.
randomize gate | `[pattern] randomize gate [Float: min=0] [Float: max=0.95]` | Set the gate of any selected Notes to a random value.
stretch | `[pattern] stretch [Unsigned: count=2]` | Duplicates items in the current selection `count` times, replacing the current selection.<br><br>This is similar to `split`, the difference is this does not split sequences, it will traverse until it finds a Note or Rest and will then duplicate it. This can also take a Pattern, whereas split cannot.
compress | `[pattern] compress` | Keep items from the current selection that match the given Pattern, replacing the current selection.
shuffle | `shuffle` | Randomly shuffle Notes and Rests in current selection.
rotate | `rotate [Int: amount=1]` | Shift Cells in the current selection by `amount`.<br><br>Positive values shift right, negative values shift left.
reverse | `reverse` | Reverse the order of all Notes and Rests in the current selection.
mirror | `[pattern] mirror [Int: centerPitch=0]` | Mirror the note pitches of the current selection around `centerPitch`.
quantize | `[pattern] quantize` | Set the delay to zero and gate to one for all Notes in the current selection.
swing | `swing [Float: amount=0.1]` | Set the delay of every other Note in the current selection to `amount`.
step | `[pattern] step [Int: pitchDistance=1] [Float: velocityDistance=0]` | Increments pitch and velocity of each child cell in the selection. If pattern is given, only adds to increments on cells that match the pattern.
arp | `[pattern] arp [String: chord="cycle"] [Int: inversion=-1]` | Plays a given chord across the current selection, each interval in the chord is applied in order to child cells in the selection.
drums | `drums [Unsigned: octaveSize=16] [Int: offset=1]` | Enter 'Drum Mode' where the zero note becomes `offset` plus the lowest of the general midi drum notes and the number of notes displayed is increased to `octaveSize`.
