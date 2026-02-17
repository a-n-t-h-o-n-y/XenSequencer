# XenSequencer Agent Rules

## Migration And Cleanup

- Do not add fallback paths, compatibility shims, or dual-path logic to preserve old integration patterns (for example, old CMake submodule/local-path fallbacks).
- Prefer clean breaks when migrating systems.
- If a migration requires an environment change, fail fast with a clear error rather than silently falling back.
- Treat fallback code as technical debt unless it is explicitly requested for a time-boxed transition.
