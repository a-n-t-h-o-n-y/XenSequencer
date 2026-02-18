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

- Do not pass explicit `-j` options to ninja/cmake build commands.
