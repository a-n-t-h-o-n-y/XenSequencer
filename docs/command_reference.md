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
load measure | `load measure [String: filename]` | Load a measure from the current sequence directory.
load tuning | `load tuning [String: filename]` | Load a tuning from the current tuning directory.
load scales | `load scales` | Load scales from library files.
load chords | `load chords` | Load chords from library files.
save measure | `save measure [String: filename]` | Save the current measure to file.
libraryDirectory | `libraryDirectory` | Display the user library directory path.
move left | `move left [Unsigned: amount=1]` | Move selection left.
move right | `move right [Unsigned: amount=1]` | Move selection right.
move up | `move up [Unsigned: amount=1]` | Move selection up one level.
move down | `move down [Unsigned: amount=1]` | Move selection down one level.
note | `note [Int: pitch=0] [Float: velocity=0.787402] [Float: delay=0] [Float: gate=1]` | Create a note at the current selection.
delete | `delete` | Delete the current selection.
split | `split [Unsigned: count=2]` | Split the current selection.
lift | `lift` | Lift the current selection up one level.
set pitch | `[pattern] set pitch [Int|Modulator: pitch=0]` | Set selected note pitches.
set octave | `[pattern] set octave [Int: octave=0]` | Set selected note octaves.
set velocity | `[pattern] set velocity [Float|Modulator: velocity=0.787402]` | Set selected note velocities.
set delay | `[pattern] set delay [Float|Modulator: delay=0]` | Set selected note delays.
set gate | `[pattern] set gate [Float|Modulator: gate=1]` | Set selected note gates.
set measure timeSignature | `set measure timeSignature [TimeSignature: timesignature=4/4]` | Set measure time signature.
set baseFrequency | `set baseFrequency [Float: freq=440]` | Set base frequency in Hz.
set scale | `set scale [String: name]` | Set the active scale by name.
set mode | `set mode [Unsigned: mode_index]` | Set the active scale mode index.
set translateDirection | `set translateDirection [String: direction]` | Set scale translate direction.
set key | `set key [Int: key=0]` | Set transposition key.
set weight | `set weight [Float: value]` | Set selected cell weight.
set weights | `[pattern] set weights [Float|Modulator: weight]` | Set child weights in selected cell.
double measure timeSignature | `double measure timeSignature` | Double measure time signature.
halve measure timeSignature | `halve measure timeSignature` | Halve measure time signature.
shift pitch | `[pattern] shift pitch [Int: amount=1]` | Shift selected note pitches.
shift octave | `[pattern] shift octave [Int: amount=1]` | Shift selected note octaves.
shift velocity | `[pattern] shift velocity [Float: amount=0.1]` | Shift selected note velocities.
shift delay | `[pattern] shift delay [Float: amount=0.1]` | Shift selected note delays.
shift gate | `[pattern] shift gate [Float: amount=0.1]` | Shift selected note gates.
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
step | `[pattern] step [Int: pitchDistance=1] [Float: velocityDistance=0]` | Apply incremental pitch/velocity offsets to selected content.
arp | `[pattern] arp [String: chord="cycle"] [Int: inversion=-1]` | Apply chord arpeggiation to selection.
chord | `chord [String: chord="cycle"] [Int: inversion=-1]` | Apply chord offsets across elements in the selected cell.
drums | `drums [Unsigned: octaveSize=16] [Int: offset=1]` | Switch to drum-oriented tuning.
