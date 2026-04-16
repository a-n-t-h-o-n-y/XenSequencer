# Polyphony Refactor Tracker

This file tracks the plugin-side refactor required after the sequencer library API change that replaces single-payload `Cell` data with polyphonic `Cell.elements`.

It is intended to stay current during the migration and should be updated as decisions change or work lands.

## Current decisions

- `Rest` is removed. Silence is represented by an empty `Cell.elements` vector.
- Selection will support both whole-cell selection and individual-element selection.
- The internal polyphony plumbing should land first, before new user-facing controls are exposed.
- Removed commands/actions:
  - `rest`
  - `flip`
  - `fill note`
  - `fill rest`
  - `quantize`
  - `swing`
- `delete` replaces `rest` for individual selected elements.
- Copy/paste rules:
  - Copying a `Cell` and pasting overwrites the target cell.
  - If an element is selected, pasting a copied `Cell` overwrites the parent cell of that element.
  - Copying a `MusicElement` and pasting onto a selected cell appends to that cell's `elements`.
  - Copying a `MusicElement` and pasting onto a selected element inserts as a sibling in the parent cell.
- The sequence bank is likely to be removed later because `Cell.elements` replaces the need for multiple concurrently playing top-level sequences.
- MIDI note trigger playback is also likely to be removed later in favor of transport-locked playback, so related code should not be overdesigned during this refactor.

## Goals for the first pass

- Update the plugin to the new core data model.
- Get serialization, bridge state, selection plumbing, and MIDI rendering working with polyphonic cells.
- Keep the implementation clean and direct.
- Avoid compatibility shims or fallback paths.
- Do not expose unfinished polyphonic interaction until the internal plumbing is stable.

## Main unresolved product question

The remaining design question is the navigation model for moving between:

- whole-cell selection
- element selection within a cell
- descending into a selected `Sequence` element

This should be resolved before adding keybindings or UI affordances, but it does not block internal plumbing work.

## Work tracker

### 1. Core data model migration

- [x] Replace all plugin-side uses of `cell.element` with logic over `cell.elements`.
- [x] Remove all plugin-side references to `Rest`.
- [x] Update default silent cells to `Cell{.elements = {}, .weight = 1.f}`.
- [ ] Review all code that assumed one event/object per step.

Primary files already known to be affected:

- `src/actions.cpp`
- `src/selection.cpp`
- `src/midi.cpp`
- `src/serialize.cpp`
- `src/bridge_serialize.cpp`
- `src/command_action.cpp`
- `src/midi_engine.cpp`
- `include/xen/measure.hpp`

### 2. Selection model

- [x] Extend selection state so it can represent either:
  - a selected `Cell`
  - a selected `MusicElement` within a `Cell`
- [x] Define the internal representation for element selection.
- [x] Update navigation helpers to work with element selection.
- [x] Define how descent into a `Sequence` element works.
- [ ] Keep the initial implementation internal until the interaction model is ready to expose.

Suggested implementation direction:

- represent selection as current cell path plus optional selected element index
- avoid designing a more complicated recursive element-path model unless it becomes necessary

### 3. Editing semantics

- [x] Make edit commands distinguish between whole-cell operations and single-element operations.
- [x] Update `delete` so it removes the selected element when an element is selected.
- [x] Define delete behavior for emptying the last remaining element in a cell.
- [x] Update `copy`, `cut`, `paste`, `duplicate`, and `lift` to respect selection kind.
- [ ] Ensure per-element operations use `MusicElement` modify overloads where appropriate.
- [ ] Ensure whole-step operations use `Cell` overloads where appropriate.

### 4. Command surface cleanup

- [x] Remove deleted commands from typed actions, parsing, execution, docs, and metadata:
  - `rest`
  - `flip`
  - `fill note`
  - `fill rest`
  - `quantize`
  - `swing`
- [x] Replace any logic that still depends on deleted `sequence::modify` APIs.
- [ ] Revisit status messages and command descriptions that still talk about rests.

Known command-side follow-up:

- `sequence::modify::note(...)` now returns `MusicElement`, not `Cell`.
- `repeat(MusicElement, count)` returns a `Sequence` as `MusicElement`.
- `repeat(Cell, count)` returns a `Cell` containing a `Sequence` in `elements`.

### 5. Serialization and clipboard

- [x] Redesign plugin serialization for `Cell.elements`.
- [x] Redesign bridge/webview JSON for `Cell.elements`.
- [x] Update clipboard serialization to support copying either `Cell` or `MusicElement`.
- [ ] Update demo files, fixtures, and generators to the new format.
- [x] Decide whether old saved files should fail fast or be migrated explicitly.

Current preferred direction:

- prefer a clean file format break over compatibility fallback logic

### 6. MIDI and timing

- [x] Stop passing cell/sequence content into `samples_count(...)`.
- [ ] Compute total measure duration from:
  - `samples_count(TimeSignature const&, std::uint32_t sample_rate, float bpm)`
- [x] Replace old MIDI flattening with:
  - `flatten_to_midi(cell.elements, sample_offset, sample_count, tuning, base_frequency, pb_range)`
- [x] Rewrite any recursive note transforms so they visit every `MusicElement` inside `Cell.elements`.
- [ ] Confirm empty cells consume time but emit no notes.

### 7. Tests and docs

- [x] Update tests that still expect `Rest` or single-payload cells.
- [x] Update command documentation to remove deleted commands.
- [ ] Update user-facing documentation that still describes rests or sequence-bank-centric behavior that is no longer accurate.
- [x] Keep this tracker updated as implementation decisions change.

## Things not to over-invest in during this refactor

- Sequence-bank-specific behavior that will likely be removed soon.
- MIDI note trigger behavior that will likely be replaced by transport-locked playback.
- Polished keybinding/UI work before the internal selection and data plumbing are stable.

## Notes

- Pattern traversal still targets child cells inside a `Sequence`.
- Pattern traversal does not target positions inside `Cell.elements`.
- Per-element editing and pattern-based sequence traversal should stay conceptually separate.
