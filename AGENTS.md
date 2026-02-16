# Repository Guidelines

## Project Structure & Module Organization
`XenSequencer` is a C++20 JUCE project built with CMake.
- Core implementation: `src/`
- Public headers: `include/xen/`
- GUI components: `src/gui/` and `include/xen/gui/`
- Tests (Catch2): `test/*.test.cpp`
- Embedded runtime assets (keys/scales/chords/fonts/demos): `data/`
- Developer utilities: `tools/` (Python scripts and small CMake targets)
- User/developer docs: `docs/`

`external/MicrotonalStepSequencer` is tracked as a git submodule; other third-party
dependencies are fetched via CMake `FetchContent`.

## Build, Test, and Development Commands
Run from repository root unless noted.
- `git submodule update --init --recursive external/MicrotonalStepSequencer`: fetch the editable sequencer dependency.
- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`: configure a local build.
- `cmake --build build --target XenSequencer_VST3`: build the VST3 plugin.
- `cmake --build build --target XenTests`: build the unit test binary.
- `./build/XenTests`: run tests directly (no `ctest` target is configured here).
- `cmake --build build --target cmd_reference KeyPress`: build helper tools.

## Coding Style & Naming Conventions
- Formatting is defined by `.clang-format`; run `clang-format` before committing.
- Style highlights: 4-space indentation, no tabs, 88-column limit, C++20.
- Prefer a functional style when practical: favor pure helper functions, explicit inputs/outputs, and minimal hidden mutable state.
- File names use `snake_case` (example: `xen_processor.cpp`).
- Keep header/source pairing consistent between `include/xen/...` and `src/...`.
- Prefer small, focused commits; follow existing naming/style in touched files.

## Testing Guidelines
- Test framework: Catch2 (fetched by CMake), with tests in `test/*.test.cpp`.
- Name tests by behavior, using descriptive `TEST_CASE` titles.
- Add or update tests for command parsing, timeline/state mutations, and MIDI-related logic when modified.
- Before opening a PR, ensure `XenTests` builds and runs cleanly in your build directory.

## Commit & Pull Request Guidelines
- Commit messages in history are short, imperative, and action-first (e.g., `Add ...`, `Fix ...`, `Update ...`).
- Use a concise subject line (aim for <= 72 chars) and include subsystem context when useful.
- Avoid `WIP` in final commits submitted for review.
- PRs should include: purpose, key changes, verification steps/commands, and linked issue(s) if applicable.
- For UI changes, include screenshots or short recordings from the plugin window.
