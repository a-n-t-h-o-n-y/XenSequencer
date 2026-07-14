# Command Reference (v0.3.1)

name | signature | description
---- | --------- | -----------
version | `version` | Print the current XenSequencer version.
again | `again` | Replay the previously executed command chain.
project new | `project new` | Create a new Project document.
undo | `undo` | Revert state to before the last action.
redo | `redo` | Reapply the last undone action.
copy | `copy` | Copy the current selection.
cut | `cut` | Cut the current selection.
paste | `paste` | Paste over the current selection.
duplicate | `duplicate` | Duplicate the current selection.
load cell | `load cell [cell_name: filename]` | Load a Cell into a new selected Sequence.
project open | `project open [project_name: filename]` | Open a Project document.
load tuning | `load tuning [tuning_name: filename]` | Load a tuning from the current tuning directory.
load scales | `load scales` | Load scales from library files.
load chords | `load chords` | Load chords from library files.
save cell | `save cell [cell_name: filename]` | Save the selected Cell to file.
project save | `project save [project_name: filename]` | Save the current Project document.
libraryDirectory | `libraryDirectory` | Display the user library directory path.
set contentDirectory | `set contentDirectory [directory_path: path]` | Set the content directory.
set tuningDirectory | `set tuningDirectory [directory_path: path]` | Set the tuning library directory.
composition loop start | `composition loop start [integer: column_coordinate]` | Set the composition loop start column.
composition loop end | `composition loop end [integer: column_coordinate]` | Set the composition loop end column.
composition row rename | `composition row rename [integer: row_coordinate] [string: name]` | Rename a composition row.
composition row channel | `composition row channel [integer: row_coordinate] [string: channel_id]` | Set a composition row channel ID.
composition cell assign | `composition cell assign [integer: row_coordinate] [integer: column_coordinate] [string: sequence_name]` | Assign a sequence to a composition cell.
composition cell unassign | `composition cell unassign [integer: row_coordinate] [integer: column_coordinate]` | Unassign a sequence from a composition cell.
sequence clear | `sequence clear` | Clear the active sequence's contents.
composition cell move | `composition cell move [integer: from_row] [integer: from_column] [integer: to_row] [integer: to_column]` | Move a composition cell.
note | `note [pitch: pitch=0] [velocity: velocity=0.787402] [delay: delay=0] [gate: gate=1]` | Create a note at the current selection.
delete | `delete` | Delete the current selection.
split | `split [repeat_count: count=2]` | Split the current selection.
lift | `lift` | Lift the current selection up one level.
set pitch | `[pattern] set pitch [pitch | modulator: pitch=0]` | Set selected note pitches.
set octave | `[pattern] set octave [octave: octave=0]` | Set selected note octaves.
set velocity | `[pattern] set velocity [velocity | modulator: velocity=0.787402]` | Set selected note velocities.
set delay | `[pattern] set delay [delay | modulator: delay=0]` | Set selected note delays.
set gate | `[pattern] set gate [gate | modulator: gate=1]` | Set selected note gates.
set duration | `set duration [time_signature: timesignature=4/4]` | Set selected column duration.
set baseFrequency | `set baseFrequency [frequency_hz: freq=440]` | Set base frequency in Hz.
set scale | `set scale [scale_id: source_id]` | Set the active scale by source ID.
set mode | `set mode [scale_mode: mode_index]` | Set the active scale mode index.
set translateDirection | `set translateDirection [translate_direction: direction]` | Set scale translate direction.
set key | `set key [transpose_key: key=0]` | Set transposition key.
set weight | `set weight [cell_weight: value]` | Set selected cell weight.
set weights | `[pattern] set weights [cell_weight | modulator: weight]` | Set child weights in selected cell.
double duration | `double duration` | Double selected column duration.
halve duration | `halve duration` | Halve selected column duration.
shift pitch | `[pattern] shift pitch [pitch_offset: amount=1]` | Shift selected note pitches.
shift octave | `[pattern] shift octave [octave_offset: amount=1]` | Shift selected note octaves.
shift velocity | `[pattern] shift velocity [velocity_offset: amount=0.1]` | Shift selected note velocities.
shift delay | `[pattern] shift delay [delay_offset: amount=0.1]` | Shift selected note delays.
shift gate | `[pattern] shift gate [gate_offset: amount=0.1]` | Shift selected note gates.
shift scale | `shift scale [scale_offset: amount=1]` | Shift loaded scale index.
shift scaleMode | `shift scaleMode [scale_mode_offset: amount=1]` | Shift scale mode.
shift translateDirection | `shift translateDirection` | Flip translate direction.
shift entireScale | `shift entireScale [direction: direction=1]` | Shift direction, mode, and scale together.
randomize pitch | `[pattern] randomize pitch [pitch: min=-12] [pitch: max=12]` | Randomize note pitches.
randomize velocity | `[pattern] randomize velocity [velocity: min=0.01] [velocity: max=1]` | Randomize note velocities.
randomize delay | `[pattern] randomize delay [delay: min=0] [delay: max=0.95]` | Randomize note delays.
randomize gate | `[pattern] randomize gate [gate: min=0] [gate: max=0.95]` | Randomize note gates.
stretch | `[pattern] stretch [repeat_count: count=2]` | Stretch selected pattern.
compress | `[pattern] compress` | Compress selected pattern.
shuffle | `shuffle` | Shuffle selected content.
rotate | `rotate [rotation_offset: amount=1]` | Rotate selected content.
reverse | `reverse` | Reverse selected content.
mirror | `[pattern] mirror [pitch: centerPitch=0]` | Mirror selected notes around center pitch.
step | `[pattern] step [pitch_offset: pitchDistance=1] [velocity_offset: velocityDistance=0]` | Apply incremental pitch/velocity offsets to selected sequence.
arp | `[pattern] arp [chord_name: chord="cycle"] [chord_inversion: inversion=-1]` | Apply chord arpeggiation to selection.
chord | `chord [chord_name: chord="cycle"] [chord_inversion: inversion=-1]` | Apply chord offsets across elements in the selected cell.
drums | `drums [octave_size: octaveSize=16] [pitch_offset: offset=1]` | Switch to drum-oriented tuning.
