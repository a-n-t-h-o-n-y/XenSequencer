# Command Reference (v0.2.0)

name | signature | description
---- | --------- | -----------
welcome | `welcome` | Display welcome message.
reset | `reset` | Reset the timeline to a blank state.
undo | `undo` | Revert the last action.
redo | `redo` | Reapply the last undone action.
copy | `copy` | Put the current selection in the copy buffer.
cut | `cut` | Put the current selection in the copy buffer and replace it with a Rest.
paste | `paste` | Overwrite the current selection with what is stored in the copy buffer.
duplicate | `duplicate` | Duplicate the current selection by placing it in the right-adjacent Cell.
inputMode | `inputMode [InputMode: mode]` | Change the input mode. The input mode determines the behavior of the up/down keys.
focus | `focus [String: component_id]` | Move the keyboard focus to the specified component.
show | `show [String: component_id]` | Update the GUI to display the specified component.
load measure | `load measure [String: filename] [Int: index=-1]` | Load a Measure from a file in the current sequence directory. Do not include the .xenseq extension in the filename you provide.
load tuning | `load tuning [String: filename]` | Load a tuning file (.scl) from the current `tunings` Library directory. Do not include the .scl extension in the filename you provide.
load keys | `load keys` | Load keys.yml and user_keys.yml.
load scales | `load scales` | Load scales.yml and user_scales.yml from Library directory
save measure | `save measure [String: filename=""]` | Save the current measure to a file in the current sequence directory. Do not include any extension in the filename you provide. This will overwrite any existing file.
libraryDirectory | `libraryDirectory` | Display the path to the directory where the user library is stored.
move left | `move left [Unsigned: amount=1]` | Move the selection left, or wrap around.
move right | `move right [Unsigned: amount=1]` | Move the selection right, or wrap around.
move up | `move up [Unsigned: amount=1]` | Move the selection up one level to a parent sequence.
move down | `move down [Unsigned: amount=1]` | Move the selection down one level.
note | `note [Int: pitch=0] [Float: velocity=0.8] [Float: delay=0] [Float: gate=1]` | Create a new Note, overwritting the current selection.
rest | `rest` | Create a new Rest, overwritting the current selection.
flip | `[Pattern] flip` | Flips Notes to Rests and Rests to Notes for the current selection. Works over sequences.
delete selection | `delete selection` | Delete the current selection.
split | `split [Unsigned: count=2]` | Duplicates the current selection into `count` equal parts, replacing the current selection.
lift | `lift` | Bring the current selection up one level, replacing its parent sequence with itself.
stretch | `[Pattern] stretch [Unsigned: count=2]` | Duplicates items in the current selection `count` times, replacing the current selection.<br><br>This is similar to `split`, the difference is this does not split sequences, it will traverse until it finds a Note or Rest and will then duplicate it. This can also take a Pattern, whereas split cannot.
compress | `[Pattern] compress` | Keep items from the current selection that match the given Pattern, replacing the current selection.
fill note | `[Pattern] fill note [Int: pitch=0] [Float: velocity=0.8] [Float: delay=0] [Float: gate=1]` | Fill the current selection with Notes, this works specifically over sequences.
fill rest | `[Pattern] fill rest` | Fill the current selection with Rests, this works specifically over sequences.
select sequence | `select sequence [Int: index]` | Change the current sequence from the SequenceBank to `index`. Zero-based.
set pitch | `[Pattern] set pitch [Int: pitch=0]` | Set the pitch of any selected Notes.
set octave | `[Pattern] set octave [Int: octave=0]` | Set the octave of any selected Notes.
set velocity | `[Pattern] set velocity [Float: velocity=0.8]` | Set the velocity of any selected Notes.
set delay | `[Pattern] set delay [Float: delay=0]` | Set the delay of any selected Notes.
set gate | `[Pattern] set gate [Float: gate=1]` | Set the gate of any selected Notes.
set measure name | `[Pattern] set measure name [String: name] [Int: index=-1]` | Set the name of a Measure. If no index is given, set the name of the current Measure. Ignores Pattern.
set measure timeSignature | `[Pattern] set measure timeSignature [TimeSignature: timesignature=4/4] [Int: index=-1]` | Set the time signature of a Measure. If no index is given, set the time signature of the current Measure.
set baseFrequency | `[Pattern] set baseFrequency [Float: freq=440]` | Set the base note (pitch zero) frequency to `freq` Hz.
set theme | `[Pattern] set theme [String: name]` | Set the color theme of the app by name.
set scale | `[Pattern] set scale [String: name]` | Set the current scale by name.
set mode | `[Pattern] set mode [Unsigned: mode]` | Set the mode of the current scale. [1, scale size].
set translateDirection | `[Pattern] set translateDirection [String: Direction]` | Set the Scale's translate direction to either Up or Down.
set key | `[Pattern] set key [Int: zero offset]` | Set the key to tranpose to, any integer value is valid.
clear scale | `clear scale` | Remove the Current Scale, if any.
shift pitch | `[Pattern] shift pitch [Int: amount=1]` | Increment/Decrement the pitch of any selected Notes.
shift octave | `[Pattern] shift octave [Int: amount=1]` | Increment/Decrement the octave of any selected Notes.
shift velocity | `[Pattern] shift velocity [Float: amount=0.1]` | Increment/Decrement the velocity of any selected Notes.
shift delay | `[Pattern] shift delay [Float: amount=0.1]` | Increment/Decrement the delay of any selected Notes.
shift gate | `[Pattern] shift gate [Float: amount=0.1]` | Increment/Decrement the gate of any selected Notes.
shift selectedSequence | `[Pattern] shift selectedSequence [Int: amount]` | Change the selected/displayed sequence by `amount`. This wraps around edges of the SequenceBank. `amount` can be positive or negative. Pattern is ignored.
shift scale | `[Pattern] shift scale [Int: amount=1]` | Move Forward/Backward through the loaded Scales.
shift scaleMode | `[Pattern] shift scaleMode [Int: amount=1]` | Increment/Decrement the mode of the current scale.
humanize velocity | `[Pattern] humanize velocity [Float: amount=0.1]` | Apply a random shift to the velocity of any selected Notes.
humanize delay | `[Pattern] humanize delay [Float: amount=0.1]` | Apply a random shift to the delay of any selected Notes.
humanize gate | `[Pattern] humanize gate [Float: amount=0.1]` | Apply a random shift to the gate of any selected Notes.
randomize pitch | `[Pattern] randomize pitch [Int: min=-12] [Int: max=12]` | Set the pitch of any selected Notes to a random value.
randomize velocity | `[Pattern] randomize velocity [Float: min=0.01] [Float: max=1]` | Set the velocity of any selected Notes to a random value.
randomize delay | `[Pattern] randomize delay [Float: min=0] [Float: max=0.95]` | Set the delay of any selected Notes to a random value.
randomize gate | `[Pattern] randomize gate [Float: min=0] [Float: max=0.95]` | Set the gate of any selected Notes to a random value.
shuffle | `shuffle` | Randomly shuffle Notes and Rests in current selection.
rotate | `rotate [Int: amount=1]` | Shift the Notes and Rests in the current selection by `amount`. Positive values shift right, negative values shift left.
reverse | `reverse` | Reverse the order of all Notes and Rests in the current selection.
mirror | `[Pattern] mirror [Int: centerPitch=0]` | Mirror the note pitches of the current selection around `centerPitch`.
quantize | `[Pattern] quantize` | Set the delay to zero and gate to one for all Notes in the current selection.
swing | `swing [Float: amount=0.1]` | Set the delay of every other Note in the current selection to `amount`.
step | `step [Unsigned: count] [Int: pitch_distance] [Float: velocity_distance=0]` | Repeat the selected Cell with incrementing pitch and velocity applied.
version | `version` | Print the current version string.
