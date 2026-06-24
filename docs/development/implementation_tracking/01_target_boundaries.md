# Chunk 01: Target Boundaries

Breaking changes are fine. Do not preserve the current target layout for backwards
compatibility, and do not add compatibility adapter layers unless they are the new
clean boundary.

## Goal

Make the dependency direction explicit: JUCE/UI/plugin adapters depend inward on
application and domain code, while domain/application code does not depend on
WebView, editor, or plugin host details.

## Implementable Work

- Split the current `XenCore` source list into smaller targets:
  - domain: pure musical model, actions, selection, timeline, validation, and command
    parsing/binding where possible;
  - application: command execution, state/session coordination, and library/workspace
    orchestration;
  - bridge: WebView protocol DTO serialization and dispatch that depends on
    application interfaces;
  - JUCE adapters: processor, editor, WebView host, JUCE file stores, and embedded
    resources.
- Move `src/webview_bridge.cpp` out of the pure core target.
- Stop making JUCE GUI/audio targets public dependencies of code that only needs
  pure state or command behavior.
- Keep source lists explicit in `CMakeLists.txt`.

## Frontend Notes

No frontend behavior should change in this chunk. If a compile-time split forces a
bridge contract change, stop and move that contract change into the WebView bridge
chunk instead.

## Acceptance Criteria

- Pure domain/application tests can link without `XenSequencer`, `XenUI`, or WebView components.
- `XenSequencer` still builds by linking the new internal targets together.
- No source in the pure domain target includes `xen_processor.hpp`,
  `xen_editor.hpp`, or `xen/gui/webview_host.hpp`.
