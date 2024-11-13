# Keybindings (v0.2.0)

## SequenceView

| Input Mode | Key | Command |
|------------|-----|--------|
| --- | `escape` | `inputMode note` |
| --- | `n` | `inputMode note` |
| --- | `v` | `inputMode velocity` |
| --- | `d` | `inputMode delay` |
| --- | `g` | `inputMode gate` |
| --- | `c` | `inputMode scale` |
| --- | `m` | `inputMode scaleMode` |
| --- | `Shift + :` | `show commandbar;focus commandbar` |
| --- | `w` | `show LibraryView;focus SequencesList` |
| --- | `e` | `show MessageLog;focus MessageLog` |
| --- | `Cmd + c` | `copy` |
| --- | `Cmd + x` | `cut` |
| --- | `Cmd + v` | `paste` |
| --- | `Cmd + d` | `duplicate` |
| --- | `Cmd + z` | `undo` |
| --- | `Cmd + y` | `redo` |
| --- | `.` | `again` |
| --- | `h` | `move left` |
| --- | `ArrowLeft` | `move left` |
| --- | `l` | `move right` |
| --- | `ArrowRight` | `move right` |
| --- | `Shift + h` | `move left` |
| --- | `Shift + ArrowLeft` | `move left` |
| --- | `Shift + l` | `move right` |
| --- | `Shift + ArrowRight` | `move right` |
| --- | `Shift + j` | `move down` |
| --- | `Shift + ArrowDown` | `move down` |
| --- | `Shift + k` | `move up` |
| --- | `Shift + ArrowUp` | `move up` |
| --- | `Cmd + Shift + ArrowRight` | `shift selectedSequence +1` |
| --- | `Cmd + Shift + l` | `shift selectedSequence +1` |
| --- | `Cmd + Shift + ArrowLeft` | `shift selectedSequence -1` |
| --- | `Cmd + Shift + h` | `shift selectedSequence -1` |
| --- | `Cmd + Shift + ArrowUp` | `shift selectedSequence +4` |
| --- | `Cmd + Shift + k` | `shift selectedSequence +4` |
| --- | `Cmd + Shift + ArrowDown` | `shift selectedSequence -4` |
| --- | `Cmd + Shift + j` | `shift selectedSequence -4` |
| Note | `j` | `shift Note -1` |
| Note | `ArrowDown` | `shift Note -1` |
| Note | `k` | `shift Note +1` |
| Note | `ArrowUp` | `shift Note +1` |
| --- | `PageDown` | `shift Octave -1` |
| --- | `PageUp` | `shift Octave +1` |
| Velocity | `j` | `shift Velocity -0.05` |
| Velocity | `ArrowDown` | `shift Velocity -0.05` |
| Velocity | `k` | `shift Velocity +0.05` |
| Velocity | `ArrowUp` | `shift Velocity +0.05` |
| Velocity | `Cmd + j` | `shift Velocity -0.01` |
| Velocity | `Cmd + ArrowDown` | `shift Velocity -0.01` |
| Velocity | `Cmd + k` | `shift Velocity +0.01` |
| Velocity | `Cmd + ArrowUp` | `shift Velocity +0.01` |
| Delay | `j` | `shift Delay -0.05` |
| Delay | `ArrowDown` | `shift Delay -0.05` |
| Delay | `k` | `shift Delay +0.05` |
| Delay | `ArrowUp` | `shift Delay +0.05` |
| Delay | `Cmd + j` | `shift Delay -0.01` |
| Delay | `Cmd + ArrowDown` | `shift Delay -0.01` |
| Delay | `Cmd + k` | `shift Delay +0.01` |
| Delay | `Cmd + ArrowUp` | `shift Delay +0.01` |
| Gate | `j` | `shift Gate -0.05` |
| Gate | `ArrowDown` | `shift Gate -0.05` |
| Gate | `k` | `shift Gate +0.05` |
| Gate | `ArrowUp` | `shift Gate +0.05` |
| Gate | `Cmd + j` | `shift Gate -0.01` |
| Gate | `Cmd + ArrowDown` | `shift Gate -0.01` |
| Gate | `Cmd + k` | `shift Gate +0.01` |
| Gate | `Cmd + ArrowUp` | `shift Gate +0.01` |
| Scale | `j` | `shift scale -1` |
| Scale | `ArrowDown` | `shift scale -1` |
| Scale | `k` | `shift scale +1` |
| Scale | `ArrowUp` | `shift scale +1` |
| Scale Mode | `j` | `shift scaleMode -1` |
| Scale Mode | `ArrowDown` | `shift scaleMode -1` |
| Scale Mode | `k` | `shift scaleMode +1` |
| Scale Mode | `ArrowUp` | `shift scaleMode +1` |
| --- | `delete` | `delete selection` |
| --- | `s` | `split :N=2:` |
| Note | `r` | `randomize note 0 12` |
| Velocity | `r` | `randomize velocity 0.0 1.0` |
| Delay | `r` | `randomize delay 0.0 0.5` |
| Gate | `r` | `randomize gate 0.5 1.0` |
| Velocity | `t` | `humanize velocity 0.05` |
| Delay | `t` | `humanize delay 0.05` |
| Gate | `t` | `humanize gate 0.05` |

## SequencesList

| Input Mode | Key | Command |
|------------|-----|--------|
| --- | `escape` | `show SequenceView;focus SequenceView` |
| --- | `w` | `show SequenceView;focus SequenceView` |
| --- | `Shift + ArrowLeft` | `focus ScalesList` |
| --- | `Shift + h` | `focus ScalesList` |
| --- | `Shift + Tab` | `focus ScalesList` |
| --- | `Shift + ArrowRight` | `focus TuningsList` |
| --- | `Shift + l` | `focus TuningsList` |
| --- | `Tab` | `focus TuningsList` |

## TuningsList

| Input Mode | Key | Command |
|------------|-----|--------|
| --- | `escape` | `show SequenceView;focus SequenceView` |
| --- | `w` | `show SequenceView;focus SequenceView` |
| --- | `Shift + ArrowLeft` | `focus SequencesList` |
| --- | `Shift + h` | `focus SequencesList` |
| --- | `Shift + Tab` | `focus SequencesList` |
| --- | `Shift + ArrowRight` | `focus ScalesList` |
| --- | `Shift + l` | `focus ScalesList` |
| --- | `Tab` | `focus ScalesList` |

## ScalesList

| Input Mode | Key | Command |
|------------|-----|--------|
| --- | `escape` | `show SequenceView;focus SequenceView` |
| --- | `w` | `show SequenceView;focus SequenceView` |
| --- | `Shift + ArrowLeft` | `focus TuningsList` |
| --- | `Shift + h` | `focus TuningsList` |
| --- | `Shift + Tab` | `focus TuningsList` |
| --- | `Shift + ArrowRight` | `focus SequencesList` |
| --- | `Shift + l` | `focus SequencesList` |
| --- | `Tab` | `focus SequencesList` |

## MessageLog

| Input Mode | Key | Command |
|------------|-----|--------|
| --- | `e` | `show SequenceView;focus SequenceView` |
| --- | `escape` | `show SequenceView;focus SequenceView` |

