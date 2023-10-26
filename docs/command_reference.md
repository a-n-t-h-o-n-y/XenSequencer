# Command Reference

## `undo`

Revert the last action.

## `redo`

Reapply the last undone action.

## `copy`

Put the current selection in the copy buffer.

## `cut`

Put the current selection in the copy buffer and replace it with a Rest.

## `paste`

Overwrite the current selection with what is stored in the copy buffer.

## `duplicate`

Duplicate the current selection by placing it in the right-adjacent Cell.

## `mode [InputMode: mode]`

Change the input mode.

The mode determines the behavior of the up/down keys.

## `focus [String: component]`

Move the keyboard focus to the specified component.

## `load [String: filetype]`

### `state [Filepath: filepath]`

Load a full plugin State from a file.

### `keys`

Load keys.yml and user_keys.yml.

## `save [String: filetype]`

### `state [Filepath: filepath]`

Save the current plugin State to a file.

## `dataDirectory`

Display the path to the directory where user data is stored.

## `move [String: direction]`

### `left [Unsigned: amount=1]`

Move the selection left, or wrap around.

### `right [Unsigned: amount=1]`

Move the selection right, or wrap around.

### `up [Unsigned: amount=1]`

Move the selection up one level to a parent sequence.

### `down [Unsigned: amount=1]`

Move the selection down one level.

## `append [String: item="measure"]`

### `measure [TimeSignature: duration=4/4]`

Append a measure to the current phrase.

## `insert [String: item="measure"]`

### `measure [TimeSignature: duration=4/4]`

Insert a measure at the current location inside the current phrase.

## `note [Int: interval=0] [Float: velocity=0.8] [Float: delay=0] [Float: gate=1]`

Create a new Note, overwritting the current selection.

## `rest`

Create a new Rest, overwritting the current selection.

## `[Pattern] flip`

Flips Notes to Rests and Rests to Notes for the current selection. Works over sequences.

## `delete [String: item="selection"]`

### `selection`

Delete the current selection.

## `split [Unsigned: count=2]`

Duplicates the current selection into `count` equal parts, replacing the current selection.

## `lift`

Bring the current selection up one level, replacing its parent sequence with itself.

## `[Pattern] stretch [Unsigned: count=2]`

Duplicates items in the current selection `count` times, replacing the current selection.

This is similar to `split`, the difference is this does not split sequences, it will traverse until it finds a Note or Rest and will then duplicate it. This can also take a Pattern, whereas split cannot.

## `[Pattern] compress`

Keep items from the current selection that match the given Pattern, replacing the current selection.

## `[Pattern] fill [String: type]`

#### `note [Int: interval=0] [Float: velocity=0.8] [Float: delay=0] [Float: gate=1]`

Fill the current selection with Notes, this works specifically over sequences.

#### `rest`

Fill the current selection with Rests, this works specifically over sequences.

## `[Pattern] set [String: trait]`

#### `note [Int: interval=0]`

Set the note interval of any selected Notes.

#### `octave [Int: octave=0]`

Set the octave of any selected Notes.

#### `velocity [Float: velocity=0.8]`

Set the velocity of any selected Notes.

#### `delay [Float: delay=0]`

Set the delay of any selected Notes.

#### `gate [Float: gate=1]`

Set the gate of any selected Notes.

#### `timeSignature [TimeSignature: timesignature=4/4]`

Set the time signature of the current Measure. Ignores Pattern.

#### `baseFrequency [Float: freq=440]`

Set the base note (interval zero) frequency to `freq`.

## `[Pattern] shift [String: trait]`

#### `note [Int: amount=1]`

Increment/Decrement the note interval of any selected Notes.

#### `octave [Int: amount=1]`

Increment/Decrement the octave of any selected Notes.

#### `velocity [Float: amount=0.1]`

Increment/Decrement the velocity of any selected Notes.

#### `delay [Float: amount=0.1]`

Increment/Decrement the delay of any selected Notes.

#### `gate [Float: amount=0.1]`

Increment/Decrement the gate of any selected Notes.

## `[Pattern] humanize [InputMode: mode]`

#### `velocity [Float: amount=0.1]`

Apply a random shift to the velocity of any selected Notes.

#### `delay [Float: amount=0.1]`

Apply a random shift to the delay of any selected Notes.

#### `gate [Float: amount=0.1]`

Apply a random shift to the gate of any selected Notes.

## `[Pattern] randomize [InputMode: mode]`

#### `note [Int: min=-12] [Int: max=12]`

Set the note interval of any selected Notes to a random value.

#### `velocity [Float: min=0.01] [Float: max=1]`

Set the velocity of any selected Notes to a random value.

#### `delay [Float: min=0] [Float: max=0.95]`

Set the delay of any selected Notes to a random value.

#### `gate [Float: min=0] [Float: max=0.95]`

Set the gate of any selected Notes to a random value.

## `shuffle`

Randomly shuffle Notes and Rests in current selection.

## `rotate [Int: amount=1]`

Shift the Notes and Rests in the current selection by `amount`. Positive values shift right, negative values shift left.

## `reverse`

Reverse the order of all Notes and Rests in the current selection.

## `[Pattern] mirror [Int: centerNote=0]`

Mirror the note intervals of the current selection around `centerNote`.

## `[Pattern] quantize`

Set the delay to zero and gate to one for all Notes in the current selection.

## `swing [Float: amount=0.1]`

Set the delay of every other Note in the current selection to `amount`.

## `demo`

Reset the state to a demo Phrase.

