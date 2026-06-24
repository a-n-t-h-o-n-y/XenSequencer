# Chunk 04: Realtime MIDI Runtime

Breaking changes are fine. Do not preserve current audio-thread behavior if a
cleaner realtime-safe design requires changing internal APIs.

## Goal

Make `processBlock` allocation-free and non-throwing in normal operation by moving
rendering, validation, and dynamic memory work off the audio callback path.

## Implementable Work

- Split MIDI rendering into a control-side render step and a realtime playback step.
- Publish prevalidated, immutable render snapshots to the audio thread.
- Replace audio-thread `std::vector` construction/sorting and throwing overflow
  paths with bounded/preallocated storage or precomputed data.
- Ensure the audio callback handles invalid or missing state by producing silence or
  pass-through according to the chosen current behavior, without throwing.
- Replace or redesign `DoubleBuffer<AudioThreadStateForGUI>` if needed so GUI
  transport reads cannot race with audio writes.
- Add focused tests for transport jumps, note-off reconciliation, project updates
  while playing, and invalid render inputs being rejected before publication.

## Frontend Notes

Frontend payloads should not change unless transport event semantics change. If
`transport.phase.sync` timing, stop events, or BPM/phase fields change, update
`../xen-frontend` and `frontend_backend_contract.md` in the same change.

## Acceptance Criteria

- `processBlock` does not allocate dynamic containers on the steady-state path.
- `processBlock` does not throw for bad project data because bad data is rejected
  before it reaches the audio thread.
- Audio-thread state transfer primitives are covered by concurrency-focused tests.
