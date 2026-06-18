# XenSequencer Agent Rules

## Migration And Cleanup

- Do not add fallback paths, compatibility shims, or dual-path logic to preserve old integration patterns (for example, old CMake submodule/local-path fallbacks).
- Prefer clean breaks when migrating systems.
- If a migration requires an environment change, fail fast with a clear error rather than silently falling back.
- Treat fallback code as technical debt unless it is explicitly requested for a time-boxed transition.
- This project is in active development and breaking changes are acceptable; prioritize the cleanest current implementation over backward compatibility.

## Clean Code And Debug Changes

- Keep code extremely clean and minimal; avoid speculative or defensive additions unless they are clearly required.
- Any debugging/instrumentation/workaround change that does not directly fix the issue must be removed before finishing.
- After attempted fixes, prune leftover mitigation code, temporary conditionals, and extra branches that are no longer necessary.
- Do not leave fallback paths or legacy behavior “just in case.” If a fix requires a behavior change, implement the new path directly and delete obsolete logic.

## Build Commands

- Do not run builds, tests, or other compile commands unless the user explicitly asks for them.
- If build, test, or executable-run verification would be useful, list the exact commands at the end of the final message so the user can run them manually.
- Use the canonical dev build directory unless the user asks otherwise:
  - Configure: `./configure.sh`
  - Build: `cmake --build build`
  - Test: `ctest --test-dir build`
- Use the release build only when the user asks for a release/plugin build:
  - Configure: `./configure.sh release`
  - Build: `cmake --build build-release --target XenSequencer_VST3`
- Local compiler or path customizations should be passed as CMake/env overrides, for example `CC=clang CXX=clang++ ./configure.sh` or `./configure.sh -DNAME=VALUE`.
- Do not create alternate build directories unless explicitly needed.
- Do not pass explicit `-j` options to ninja/cmake build commands.

## C++ Style (Beyond clang-format)

- Use east const consistently (`Type const &value`, `auto const x = ...`), including pointers/references.
- Prefer `auto` for local variables when the type is obvious from the initializer or would be noisy to repeat; avoid `auto` when it hurts readability.
- Prefer trailing return types for non-trivial function signatures (`auto fn(...) -> ReturnType`), matching existing headers/sources.
- Use `snake_case` for variables, functions, parameters, and file names.
- Use `PascalCase` for type names (`struct`, `class`, `enum class`, aliases).
- Use `UPPER_SNAKE_CASE` for compile-time constants/macros that are intended as constants (for example `VERSION`).
- Keep private member fields with a trailing underscore (`processor_`, `webview_host_`).
- Keep namespaces explicit and consistent with folder/module layout (for example `namespace xen` and `namespace xen::gui`).
- In production code, avoid `using namespace`; keep qualified names explicit. (Using-directives are acceptable in tests when they improve readability.)
