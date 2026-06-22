# 12: Workspace Settings Lifetime

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

- [ ] Project files and host state contain no workspace paths or preferences.
- [ ] New processor instances initialize from the same application-wide settings.
- [ ] Explicit setting changes are persisted and visible to later processor instances.
- [ ] Commands without workspace capability cannot access settings.
- [ ] Missing dependencies fail clearly; no local/system fallback is introduced.
- [ ] Project replacement and workspace mutation have independent lifecycle behavior.

## Verification

- [ ] Tests cover initialization, persistence, mutation, denied access, missing paths,
      and project/host serialization boundaries.
- [ ] `XenTests` builds and full `ctest` passes, or the blocker is recorded.
