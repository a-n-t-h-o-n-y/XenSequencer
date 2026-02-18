# Command Reference (v0.3.0)

name | signature | description
---- | --------- | -----------
welcome | `welcome` | Display welcome message.
version | `version` | Print the current XenSequencer version.
again | `again` | Replay the previously executed command chain.
commit | `commit` | Force a commit on the current command chain.
reset | `reset` | Reset XenSequencer to its initial state.
undo | `undo` | Revert state to before the last action.
redo | `redo` | Reapply the last undone action.
copy | `copy` | Copy the current selection.
cut | `cut` | Cut the current selection.
paste | `paste` | Paste over the current selection.
duplicate | `duplicate` | Duplicate the current selection.
inputMode | `inputMode [InputMode: mode]` | Change the input mode used by editing commands.
load sequenceBank | `load sequenceBank [String: filename]` | Load the sequence bank from the current sequence directory.
load tuning | `load tuning [String: filename]` | Load a tuning from the current tuning directory.
load keys | `load keys` | Deprecated command.
load scales | `load scales` | Load scales from library files.
load chords | `load chords` | Load chords from library files.
save sequenceBank | `save sequenceBank [String: filename]` | Save the current sequence bank to file.
libraryDirectory | `libraryDirectory` | Display the user library directory path.
move left | `move left [Unsigned: amount=1]` | Move selection left.
move right | `move right [Unsigned: amount=1]` | Move selection right.
move up | `move up [Unsigned: amount=1]` | Move selection up one level.
move down | `move down [Unsigned: amount=1]` | Move selection down one level.
note | `note [Int: pitch=0] [Float: velocity=0.787402] [Float: delay=0] [Float: gate=1]` | Create a note at the current selection.
rest | `rest` | Create a rest at the current selection.
delete | `delete` | Delete the current selection.
split | `split [Unsigned: count=2]` | Split the current selection.
lift | `lift` | Lift the current selection up one level.
flip | `[pattern] flip` | Flip notes and rests in the selected pattern.
fill note | `[pattern] fill note [Int: pitch=0] [Float: velocity=0.787402] [Float: delay=0] [Float: gate=1]` | Fill the current selection with notes.
fill rest | `[pattern] fill rest` | Fill the current selection with rests.
select sequence | `select sequence [Int: index]` | Select a sequence index from the sequence bank.
set pitch | `[pattern] set pitch [Int|Modulator: pitch=0]` | Set selected note pitches.
set octave | `[pattern] set octave [Int: octave=0]` | Set selected note octaves.
set velocity | `[pattern] set velocity [Float|Modulator: velocity=0.787402]` | Set selected note velocities.
set delay | `[pattern] set delay [Float|Modulator: delay=0]` | Set selected note delays.
set gate | `[pattern] set gate [Float|Modulator: gate=1]` | Set selected note gates.
set sequence name | `set sequence name [String: name] [Int: index=-1]` | Set sequence name by index or current selection.
set sequence timeSignature | `set sequence timeSignature [TimeSignature: timesignature=4/4] [Int: index=-1]` | Set sequence time signature.
set baseFrequency | `set baseFrequency [Float: freq=440]` | Set base frequency in Hz.
set scale | `set scale [String: name]` | Set the active scale by name.
set mode | `set mode [Unsigned: mode_index]` | Set the active scale mode index.
set translateDirection | `set translateDirection [String: direction]` | Set scale translate direction.
set key | `set key [Int: key=0]` | Set transposition key.
set weight | `set weight [Float: value]` | Set selected cell weight.
set weights | `[pattern] set weights [Float|Modulator: weight]` | Set child weights in selected cell.
double sequence timeSignature | `double sequence timeSignature [Int: index=-1]` | Double sequence time signature.
halve sequence timeSignature | `halve sequence timeSignature [Int: index=-1]` | Halve sequence time signature.
shift pitch | `[pattern] shift pitch [Int: amount=1]` | Shift selected note pitches.
shift octave | `[pattern] shift octave [Int: amount=1]` | Shift selected note octaves.
shift velocity | `[pattern] shift velocity [Float: amount=0.1]` | Shift selected note velocities.
shift delay | `[pattern] shift delay [Float: amount=0.1]` | Shift selected note delays.
shift gate | `[pattern] shift gate [Float: amount=0.1]` | Shift selected note gates.
shift selectedSequence | `shift selectedSequence [Int: amount]` | Shift selected sequence index.
shift scale | `shift scale [Int: amount=1]` | Shift loaded scale index.
shift scaleMode | `shift scaleMode [Int: amount=1]` | Shift scale mode.
shift translateDirection | `shift translateDirection` | Flip translate direction.
shift entireScale | `shift entireScale [Int: direction=1]` | Shift direction, mode, and scale together.
randomize pitch | `[pattern] randomize pitch [Int: min=-12] [Int: max=12]` | Randomize note pitches.
randomize velocity | `[pattern] randomize velocity [Float: min=0.01] [Float: max=1]` | Randomize note velocities.
randomize delay | `[pattern] randomize delay [Float: min=0] [Float: max=0.95]` | Randomize note delays.
randomize gate | `[pattern] randomize gate [Float: min=0] [Float: max=0.95]` | Randomize note gates.
stretch | `[pattern] stretch [Unsigned: count=2]` | Stretch selected pattern.
compress | `[pattern] compress` | Compress selected pattern.
shuffle | `shuffle` | Shuffle selected content.
rotate | `rotate [Int: amount=1]` | Rotate selected content.
reverse | `reverse` | Reverse selected content.
mirror | `[pattern] mirror [Int: centerPitch=0]` | Mirror selected notes around center pitch.
quantize | `[pattern] quantize` | Quantize selected note timing.
swing | `swing [Float: amount=0.1]` | Apply swing to selection.
step | `[pattern] step [Int: pitchDistance=1] [Float: velocityDistance=0]` | Apply incremental pitch/velocity offsets to selected sequence.
arp | `[pattern] arp [String: chord="cycle"] [Int: inversion=-1]` | Apply chord arpeggiation to selection.
drums | `drums [Unsigned: octaveSize=16] [Int: offset=1]` | Switch to drum-oriented tuning.
