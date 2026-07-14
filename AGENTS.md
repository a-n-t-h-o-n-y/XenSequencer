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

- Never configure, compile, link, launch the application or plugins, or run tests. The
  user will perform all runtime verification and report the results.
- Static inspection is allowed, including focused searches, diff review,
  `git diff --check`, and formatting changed files.
- After any code-changing task, end the handoff with the briefest relevant commands for
  the user to compile and run the changed code or tests. Do not include unrelated or
  exhaustive verification steps.
- Use the repository's canonical workflows when suggesting commands: `./configure.sh`
  for development configuration and `./configure.sh release` only for requested
  release/plugin work. Keep build parallelism at six or fewer jobs and do not suggest
  explicit `-j` options.

## Token-Efficient Agent Workflow

Repository characteristics that matter for agent context:

- `include`, `src`, and `test` contain about 90 C++ files and 473 KB of text. Reading
  all of them costs roughly 120k tokens before reasoning or command output.
- Recent changes commonly span several files, but the relevant code is usually localized
  by symbol. Read search hits and nearby declarations first instead of whole modules.

Use the following operating rules:

- Start discovery with `rg -n` and `rg --files` scoped to likely directories and file
  types. Exclude `build`, `build-release`, `_deps`, the submodule, fonts, and images
  unless the task specifically concerns them.
- Read only relevant line ranges around matching symbols. Do not dump a complete large
  source file when a declaration, implementation, or test section is enough.
- Track files and symbols already inspected. Do not reread an unchanged file in full;
  use a narrower `rg`, `sed` range, or `git diff` to recover the needed context.
- Do not inspect generated build files such as `build.ninja`, `CMakeCache.txt`, embedded
  binary-data outputs, or fetched dependency sources to understand project behavior.
  Query CMake targets or project source lists instead.
- Inspect `CMakeLists.txt` by relevant section (`XEN_*_SOURCES`, target definition,
  tests, or dependency declaration), not by repeatedly printing the whole file.
- Run `clang-format` only on changed C++ files. In-place formatting normally emits no
  output; use `--dry-run --Werror` on those same files when a formatting check is
  useful. Do not format or check the whole tree for a localized change.
- Prefer `git diff -- <paths>` and `git diff --check` over rereading edited files.
  Review the complete final diff once, without repeatedly printing unchanged context.
- Keep progress updates to decisions, phase changes, blockers, and verification
  results. Do not narrate each search, file read, or successful command, and do not
  repeat code or logs already present in tool output.
- Preserve compact working notes as paths, symbols, decisions, and unresolved issues.
  Context accumulation multiplies the cost of repeated file contents, logs, plans, and
  status prose across later turns.

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
- State what changed and note that compilation and tests were left to the user.
- After code changes, give only the shortest relevant compile/run or test instructions
  for the user to execute and report back.
- Leave a short git commit message if the change is enough for a commit.
