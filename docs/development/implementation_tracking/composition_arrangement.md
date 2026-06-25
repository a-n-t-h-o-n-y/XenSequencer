# Composition Architecture

## Overview

The composition system separates **musical content** (measures) from **arrangement** (composition). Measures are stored in a reusable bank, while a composition references those measures to construct a complete piece.

## Measure Bank

A persistent collection of reusable measures.

### Requirements

* Measures have a **stable unique ID**.
* Measures can be created, modified, duplicated, deleted, and queried through a well-defined API.
* Measures are serialized/deserialized as part of the project.
* Measures contain only musical data.
* **Measure length** (currently represented as *time signature* in the code) should be removed from the measure itself.

## Composition

A composition represents the arrangement of measures.

### Structure

* A **2D matrix** of optional measure references.

  * Each cell references a measure from the measure bank, or is empty.
* A single sequence of **column metadata**.

  * Each column stores its musical length (currently equivalent to the existing time signature concept).
  * Every row shares the same column metadata.

Conceptually:

```text
            Column Metadata
      [4/4] [3/4] [5/8] [4/4]
Row A   M1    M7    --    M2
Row B   M5    M3    M4    --
Row C   --    M1    M8    M6
```

The matrix describes *what* is played, while the column metadata describes *how long* each position lasts.

## Output Assignment

Each matrix row is assigned to a specific output/instance.

* Every row has exactly one assigned output.
* Multiple rows may target the same output.
* Output assignment is independent of the musical content.

## APIs

Provide a clean interface for manipulating both major data structures.

### Measure Bank

* Create/remove measures.
* Read/update measure contents.
* Duplicate measures.
* Query measures by ID.
* Enumerate all measures.

### Composition

* Insert/remove rows and columns.
* Move/reorder rows and columns.
* Assign or clear measure references.
* Change column length.
* Assign rows to outputs.
* Query the arrangement.

## MIDI Generation

During playback:

* Only rows assigned to the current plugin instance are rendered.
* The renderer walks the composition matrix and resolves measure references through the measure bank.
* Column lengths determine playback timing.

## Design Goals

* Measures are reusable throughout the project.
* Arrangement and musical content remain independent.
* Stable IDs allow reliable references across edits and serialization.
* Empty cells naturally represent rests or unused regions.
* The architecture supports efficient editing, composition, and future features such as reusable phrases, alternate arrangements, and shared measures without duplicating musical data.


------------

The above are recommendataions from a source that has not seen this codebase,
but the essential ideas are there, so if something contradicts how the actual
codebase works, defer to the codebase.

The general update is, a bank of Measure objects with IDs, those are referenced
in a componsition/arrangement matrix, time left to right, tracks top the bottom,
meta cells in the matrix, at the top of each column for the time
signature/length, but that position is just the UI, in code it can be whatever
it needs to be. The idea of outputs for each row is new, this is because in the
future this app will attempt to implement the ipc.md doc and each instance will
be able to control other instances, so in composition you get to pick which
instance that row is sent to, multiple rows can go to the same instance, the
current instance only renders midi for rows that point to itself.

This is all to be implemented for the backend first, the frontend will not make
any changes yet, but you should try to provide a comprehensive API surface for
the frontend to use in the future. I realise that the api will change just
because the interface datastructures will change, so the frontend will need some
updates, keep anything the frontend will need to know in a new document that can
be handed off later.

Because I am not plugging this into the frontend right away, i want the default
setup to be a single measure in a single row to the current instance for output
in 4/4 time. Basically trying to imitate the current setup. The default setup
should not be a special case in the code, it should just be the inital values, I
don't want a code path in the backend i have to update later because it was
hardcoded to a specific use of this system.

Selection will need to be updated as well, a single cell in the composition can
be selected at any given time, but I'm not really sure if selection is still a
part of the backend, that is mostly a frontend concept.

------

I truly trust you to make good design decisions without my input. Typically I
want clean design, and I do not need backwards compatability with old versions,
feel free to make breaking changes.

Anything that the separate frontend repo needs to know you should put in a doc
specificallly that i can hand off to the frontend agent.

Make sure to do an overall pass at the end of the entire project and make sure
best practices and loose ends are covered.

Think through any inconsistencies or anything that would make this composition
design the wrong design and think on that, fix obvious concerns, and note others
that need developer input to make a decision on.

Look for unnecessary heavy compute paths in this codebase and if anything is
glaringly obvious, fix it to be more efficient.

At the very end, do a pass on the midi rendering code and look for obvious
optimization wins and implement any that you find.
