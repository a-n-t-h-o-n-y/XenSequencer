# 12: Workspace Settings Lifetime

Status: Complete.

## Objective

Move non-project paths and preferences into application-wide workspace settings with
an explicit lifetime and command capability.

## Prerequisites

Packages 05 and 11.

## Scope

- Add or isolate `WorkspaceSettings` as application-wide persisted preferences.
- Remove workspace paths/preferences from project and host state serialization.
- Initialize each processor from the application-wide preferences.
- Persist explicit workspace-setting changes.
- Ensure commands declare read or mutate workspace access.
- Keep ordinary measure/tuning imports governed by command policy rather than treating
  them as full project replacement.
- Add clear failure behavior for missing or invalid required paths.

## Acceptance criteria

- [x] Project files and host state contain no workspace paths or preferences.
- [x] New processor instances initialize from the same application-wide settings.
- [x] Explicit setting changes are persisted and visible to later processor instances.
- [x] Commands without workspace capability cannot access settings.
- [x] Missing dependencies fail clearly; no local/system fallback is introduced.
- [x] Project replacement and workspace mutation have independent lifecycle behavior.

## Verification

- [x] Tests cover initialization, persistence, mutation, denied access, missing paths,
      and project/host serialization boundaries.
- [x] `XenTests` builds and full `ctest` passes, or the blocker is recorded.
