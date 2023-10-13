# XenSequencer User Guide

## Table of Contents

1. [Overview](#overview)
1. [Getting Started](#getting-started)
   - [User Interface](#user-interface)
   - [Load Demo](#load-demo)
1. [Keyboard Navigation](#keyboard-navigation)
   - [Navigating the UI](#navigating-the-ui)
   - [Editing Keyboard Shortcuts](#editing-keyboard-shortcuts)
1. [Commands](#commands)
   - [Patterns](#patterns)
   - [Command Bar](#command-bar)
   - [Common Commands](#common-commands)
1. [Contact and Support](#contact-and-support)

## Overview
- Monophonic step sequencer VST plugin.
- Unique sequence creation tools.

## Getting Started
- Build from source, or download the latest release from the [releases page](https://github.com/a-n-t-h-o-n-y/XenSequencer/releases).
- Place `XenSequencer.vst3` folder in your system's VST3 plugin directory.
   - If using the pre-built VST, MacOS will have an 'unverified developer' warning. To get around this, build from source, or google "macos unverified developer vst3".
- Open the plugin in your DAW with an instrument on a separate track.
- Route MIDI output from the XenSequencer into the instrument and enable monitoring of the instrument.

### User Interface
- Phrase Timeline
- Measure Editor
- Command Bar
   - Only visible when in focus via `:` key
- Status Bar
   - Input mode letter
   - Message from last executed command

### Load Demo
- Open the XenSequencer GUI, click on the middle element of the sequencer to give it focus.
- Press the colon key `:` to open the command bar.
- Type `demo` and press enter.
- The sequencer will now be filled with a demo sequence.
- Move around the sequence with the arrow keys.
   - Left/Right will move the selection within the current sequence.
   - Up/Down will move the selection between layers of sub-sequences.
- With a note or sequence selected, press the `n` key to switch from movement to note input mode. The Up/Down arrows will now control the note pitch, if a sequence is selected, the change will apply to all notes in that sequence.
- Move between measures in a phrase by pressing the up key until you are at the top most layer, then move left or right.
- Press the spacebar to play the sequence.
- Change the tempo in your DAW.

## Keyboard Navigation
Mouse input is not supported in this initial version. There are a few default key bindings for basic tasks and the rest happens via the command bar. Make sure the plugin is in focus (click on it) before using keyboard shortcuts.

All keyboard shortcuts can be viewed/edited in the `keys.yml` file in the user data directory.

### Navigating the UI
| Key Combination | Action |
| --- | --- |
| Left/Right Arrows or `h`/`l` | Move among a sequence |
| Up/Down Arrows or `k`/`j` | Move between layers of sequences |
| `:` | Open the command bar |
| `ctrl/cmd`+`z` | **Undo** the last action |
| `ctrl/cmd`+`y` | **Redo** the last action |
| `ctrl/cmd`+`c` | **Copy** the current selection |
| `ctrl/cmd`+`x` | **Cut** the current selection |
| `ctrl/cmd`+`v` | **Paste** over the current selection |
| `n` | Enable **Note** input mode, where the Up/Down arrows will change the note pitch |
| `v` | Enable **Velocity** input mode, where the Up/Down arrows will change the note velocity |
| `d` | Enable **Delay** input mode, where the Up/Down arrows will change the note delay |
| `g` | Enable **Gate** input mode, where the Up/Down arrows will change the note gate |
| `m` | Enable **Movement** input mode, where the Up/Down arrows will move the selection |
| `esc` | Exit the command bar or revert to movement input mode |
| `del` | Delete the current selection |

### Editing Keyboard Shortcuts
- Keyboard shortcuts can be edited in the `keys.yml` file in the user data directory.
- Possible locations of the user data directory:
   - Windows: `\Users\username\AppData\Roaming\XenSequencer\`
   - MacOS: `~/Library/XenSequencer/`
   - Linux: `~/.config/XenSequencer/`
- Each action in the `keys.yml` file is a string to be passed to the command bar.

## Commands
All actions in the plugin are defined by command strings. These follow the format of `[pattern] command_name [arguments...]` where square brackets delimit optional items. The command bar is the main UI element to enter commands.

The command reference can be found [here](command_reference.md).

### Patterns
Patterns follow the following format: `[offset] intervals...` where `offset` is optional. `offset` is defined by putting a plus sign followed immediately by an integer. `intervals...` is a space separated list of integers.

If a command takes a Pattern, the pattern will be used to select notes in the current sequence. If no pattern is provided, then the entire selection will be used.

The `offset` is used to shift the pattern to the right by the given number of steps, the default is `+0`. For example, `+1` will apply the command to everything except the first note. `+1 2` will skip the first note, then skip every other note after that. `2 3` will apply to the first, third, sixth, eighth note, etc... An iterval cannot be less than one. The default Pattern, when one is not provided, is `+0 1`.

### Command Bar
The colon key `:` opens the command bar. The command bar is used to type and execute commands. The command bar will autocomplete commands and arguments as you type. Press the up and down arrows to cycle through the command history, and press `tab` to autocomplete any shown guide text. Some commands have default arguments that are used if not provided, these values are displayed in the guide text. `Enter` key will execute the typed command.

### Common Commands
| Command | Description |
| --- | --- |
| `addMeasure [time signature=4/4]` | Append an empty measure to the current phrase. |
| `note [interval=0] [velocity=0.8] [delay=0] [gate=1]` | Create a new note, overwriting the current selection. |
| `rest` | Create a new rest, overwriting the current selection. |
| `split [count=2]` | Split the current selection into two sequences. |
| `[pattern] fill [note\|rest] ...` | Fill the current selection with the given note or rest. |
| `[pattern] randomize [note\|velocity\|delay\|gate] ...` | Randomize the given property of the current selection. |
| `save [filepath]` | Save the plugin state to a `json` file. |
| `load [filepath]` | Load the plugin state from a `json` file. |

## Contact and Support
Contact by opening an issue or starting a discussion at the [github page](https://github.com/a-n-t-h-o-n-y/XenSequencer) for this project.
