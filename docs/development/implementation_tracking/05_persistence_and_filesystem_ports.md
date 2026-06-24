# Chunk 05: Persistence And Filesystem Ports

Breaking changes are fine. Do not preserve `juce::File` in domain/application
types for compatibility.

## Goal

Move filesystem access behind ports/adapters and use one persistence policy for
project-related file effects, workspace settings, keymap settings, copy buffer, and
library files.

## Implementable Work

- Replace `juce::File` in domain-facing state such as `WorkspaceSettings` with a
  plain path value such as `std::filesystem::path` or normalized UTF-8 strings.
- Replace command capabilities that expose `juce::File` with narrow read/write
  ports that take domain path values.
- Centralize atomic text-file writes so `SubmissionEffects`, workspace settings,
  and keymap settings use the same temporary/replace/rollback/error policy.
- Move user directory creation and embedded default-file installation out of model
  constructors/default field initializers.
- Make library file reads and copy buffer reads go through the same port system.
- Keep failures explicit and early; do not silently recreate or ignore corrupt user
  files unless the new task explicitly chooses that policy.

## Frontend Notes

Library payload paths may change formatting if path normalization changes. Update
`../xen-frontend` if it displays or compares `paths`, `relative_path`, `stem`, or
file-backed command strings. Since breaking changes are allowed, remove old path
normalization assumptions from the frontend.

## Acceptance Criteria

- Pure state construction does not create directories or touch the user filesystem.
- Domain/application tests can use fake file ports without JUCE.
- All persisted text files use a single atomic-write helper or adapter.
