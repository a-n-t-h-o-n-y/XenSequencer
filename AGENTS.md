# XenSequencer Agent Rules

## Scope And Change Discipline

- Keep changes limited to the requested work; do not fold in unrelated refactors or formatting.
- Preserve existing user changes and work around a dirty worktree.
- Public headers live in `include/xen`, implementations in `src`, and tests in `test`; follow the existing module layout.
- When adding or removing source files, update the explicit source lists in `CMakeLists.txt`.
- `external/MicrotonalStepSequencer` is a git submodule. Do not edit it or change its pinned commit unless explicitly requested.
- The frontend is a separate sibling project (`../xen-frontend`). Do not modify it unless the request includes frontend work.

## Migrations And Cleanup

- Breaking changes are acceptable; prefer the cleanest current design over backward compatibility.
- Do not add fallback paths, compatibility shims, or parallel old/new behavior unless explicitly requested for a time-boxed transition.
- When requirements or dependencies are missing, fail fast with a clear error instead of silently degrading.
- Keep implementations minimal and remove obsolete logic, temporary diagnostics, workarounds, and abandoned mitigation code before finishing.

## Dependencies And Generated Inputs

- Keep third-party versions pinned in CMake; do not introduce local-path or system-package fallbacks.
- Treat files under `data` as runtime or embedded inputs. Preserve their formats and update consumers/tests when schemas or semantics change.
- Do not commit build outputs, fetched dependencies, or generated artifacts from `build` or `build-release`.

## Build And Verification

- Builds and tests may take a while; run them when they provide useful verification, not as a reflex after every change.
- Use the canonical dev workflow:
  - Configure: `./configure.sh`
  - Build: `cmake --build build`
  - Test: `ctest --test-dir build`
- Use the release workflow only for a requested release/plugin build:
  - Configure: `./configure.sh release`
  - Build: `cmake --build build-release --target XenSequencer_VST3`
- Pass local compiler or path changes as environment/CMake overrides, for example `CC=clang CXX=clang++ ./configure.sh` or `./configure.sh -DNAME=VALUE`.
- Do not invoke raw configure commands or create alternate build directories unless explicitly needed; use `configure.sh` and the existing preset directories.
- Do not pass explicit `-j` options to Ninja or CMake build commands.

## C++ Style (Beyond clang-format)

- Use east const consistently, including pointers and references: `Type const &value`, `auto const x = ...`.
- Prefer `auto` when the type is obvious or noisy to repeat; use an explicit type when it improves readability.
- Prefer trailing return types for non-trivial signatures: `auto fn(...) -> ReturnType`.
- Use `snake_case` for variables, functions, parameters, and files; `PascalCase` for types; and `UPPER_SNAKE_CASE` for constant-like macros and compile-time constants.
- Keep private member fields with a trailing underscore (`processor_`, `webview_host_`).
- Keep namespaces aligned with the module layout (`xen`, `xen::gui`) and avoid `using namespace` in production code.
- Match nearby code when these rules do not settle a style choice.

## Completion

- Review the final diff for accidental scope growth, stale comments, and temporary code.
- State what changed and what verification was or was not performed.
