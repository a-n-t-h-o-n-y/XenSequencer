# XenSequencer User Guide

## DAW Setup

Buckle up, this is the most complicated part of the process. XenSequencer is a VST plugin that works with MIDI and support across DAWs is varied. The following are some general steps to get the plugin up and running in your DAW.

### General Steps
- Add XenSequencer to a MIDI track in your DAW.
- Route the MIDI output from XenSequencer to a MIDI instrument.
   - If you're lucky enough to have a DAW that supports MIDI plugins directly, it can be placed inline with the instrument on the same track.
   - Otherwise the instrument will need to be on a separate track, and the MIDI output from XenSequencer can be routed to the instrument's input.
- Enable MPE support if offered by your DAW.


### Ableton
- Needs separate tracks for XenSequencer and instrument.
- Instrument track needs input from XenSequencer track, and to have monitoring set to 'In'.
- Right click on the XenSequencer plugin box and enable MPE.

### Bitwig
- Can be placed inline with the instrument on the same track.
- MPE button is available when plugin is selected, but have yet to get pitch bend data to work.

## Brief Tutorial

Start out by creating a new MIDI clip in your DAW with the C1 note held for a measure. Loop it.

Open the plugin and you'll see a blank timeline. Click on the middle element of the sequencer to give it focus, then press the colon key `:` to open the command bar at the bottom. Type 'note' and press enter. This will fill the sequence with a single note of pitch '0'. Press play.

Press `p` to enter pitch input mode. Use the up and down arrows to change the pitch. Press `v` to enter velocity input mode, and change the velocity. Press `d` to enter delay input mode, and change the delay. Press `g` to enter gate input mode, and change the gate.

If you go past an octave boundary, you'll notice the note wraps around back to the bottom with an icon to denote the octave. This allows you to see interval pitch relations and limits the vertical space needed.

Press `s` to split the current note into two notes. This is the main mechanism to create more notes. Now both notes are selected, press `p` to enter pitch input mode and change the pitch of both notes at once.

The `s` keybinding can also take an number parameter, you do this by first typing out the number, and then hitting the `s` key. This number will determine the number of divisions within the split. Be careful with large numbers here.

Press `Shift` and the down arrow to 'drop down' one layer into the sequence. This will allow you to select individual notes that were created by the split. Press `Shift` and the up arrow to move back up a layer. This is the recursive nature of the sequencer, every level operates the same as the one above it and is a complete sequence itself.

![Sequencer](img/guide-sequencer.png)

Try copy and pasting any selection with the common `ctrl+c` and `ctrl+v` keybindings. You'll notice that copying a sequence and pasting it into a shorter or longer selection will stretch or compress the sequence to fit the new length. This copy and paste buffer works across instances as well, so if you want to copy a drum rhythm into your synth track that is possible.

Open the Sequence Bank by clicking on the arrow icon. You will see the 16 available sequences. Click on one to select it, then create a new sequence. Go back to your DAW track and add the C# note just above the previous C note. The new sequence should start playing back.

From here try the Library view out by pressing `w`. This will show you the saved sequences, tunings, and scales. You can load a new scale or tuning by clicking on it. Be warned that scales only make sense for tunings with 12 notes per octave, and not even all of those too.

Check out the [command reference](command_reference.md) and the [keybindings reference](keybindings_reference.md) for more ideas.

## Scales
The current scale defines a subset of notes available. Any notes outside of this set will be transposed to the next valid note in the current translate direction. The easiest way to change scales is to enter the scale input mode with the `c` key and cycle through the available scales with the arrow keys. All scales are 12 tone scales, with any notes outside of the 12 tone scale being ignored.

The **Scale Mode** shifts the scale's interval pattern by some amount. For example, a major scale has the interval pattern `[2, 2, 1, 2, 2, 2, 1]`. If the scale mode is set to `3`, then the scale will start on the third note of the major scale, `[1, 2, 2, 2, 1, 2, 2]`. This can be cycled through by entering 'scale mode' input mode with the `m` key, then the arrow keys to cycle the mode.

## Commands

Commands are entered into the command bar at the bottom of the plugin window. The command bar will autocomplete commands and arguments as you type. Press the up and down arrows to cycle through the command history, and press `tab` to autocomplete any shown guide text. Some commands have default arguments that are used if not provided, these values are displayed in the guide text. `Enter` key will execute the typed command.
Multiple commands can be run at once by separating them with a semicolon `;`.

![Command Bar](img/guide-command-bar.png)

The `again` command will repeat the previous command, that has the `.` keybinding.

Commands can be bound to keyboard shortcuts in the `user_keys.yml` file in the user data directory. Possible locations of the user data directory:
- Windows: `\Users\username\AppData\Roaming\XenSequencer\`
- MacOS: `~/Library/XenSequencer/`
- Linux: `~/.config/XenSequencer/`

### Patterns

Some commands take a `Pattern` as an argument. Patterns have the following format: `[offset] intervals...` where `offset` is optional. `offset` is defined by putting a plus sign followed immediately by an integer. `intervals...` is a space separated list of integers. The patterns defines the selection of notes in the current sequence to run the command against. If no pattern is provided, then the entire selection will be used.

The `offset` is used to shift the pattern to the right by the given number of steps. For example, `+1` will apply the command to everything except the first note. `+1 2` will skip the first note, then skip every other note after that. `2 3` will apply to the first, third, sixth, eighth note, etc... An iterval cannot be less than one. The default Pattern, when one is not provided, is `+0 1`.

The command reference can be found [here](command_reference.md).

## Sequence Bank
Contains 16 monophonic sequences, each activated by a different MIDI note. Can be played simultaneously, up to 15 note polyphony. Switch between sequences with `Ctrl` + `Shift` + `Arrow` keys.

![Sequence Bank](img/guide-sequence-bank.png)

## Top Bar
Along the top of the plugin window you'll find a listing of some settings, these are all editable by double clicking and typing unless otherwise noted.

![Top Bar](img/guide-top-bar.png)

| Element | Description |
| ------- | ----------- |
| Time Signature | The time signature of the sequence in view. |
| Zero Frequency | The frequency of the zero-th pitch. |
| Key | A transposition parameter, applied to all notes. |
| Scale | The scale applied to all notes. |
| Scale Mode | The mode of the scale, if any; range of [1, scale size]. |
| Tuning | The current tuning name, not editable. |
| Sequence Name | The current sequence index and name. |

## Library
Press `w` to toggle the Library view. This view contains the following sections:
- Saved Sequences
- Tunings
- Scales
