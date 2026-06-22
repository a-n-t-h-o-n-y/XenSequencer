# 02: Command Context and Explicit Policy

## Objective

Make request context and every command's resource requirements explicit, validated,
and authoritative.

## Prerequisite

Package 01.

## Scope

- Add typed `CommandContext` carrying optional selection and expected project revision.
- Add the policy enums and `CommandPolicy` described in the tracker.
- Require every catalog registration to initialize every policy field.
- Validate incoherent policy combinations during catalog construction.
- Make project-aware internal callers use the current typed revision.
- Centralize the decision of which commands require an expected revision.
- Add policy tests for representative project, library, workspace, file, history, and
  informational commands.

Do not yet move selection ownership or replace handler contexts.

## Acceptance criteria

- [x] Every command definition has one complete explicit policy.
- [x] Catalog construction fails fast for all incoherent combinations listed in the
      tracker.
- [x] Every project read, edit, replacement, or history-navigation request requires an
      expected current revision.
- [x] Non-project commands can execute without a project revision.
- [x] Policies correctly describe mixed effects such as project edit plus file access.
- [x] No single legacy effect enum remains authoritative for capability decisions.

## Verification

- [x] Tests cover policy validation and representative command classifications.
- [x] Processor tests cover missing, current, and stale revisions.
- [x] `XenTests` builds and full `ctest` passes, or the blocker is recorded.
