I have a JUCE vst app. I want to be able to communicate between all other instances of the vst/app. The thrust of it is this: It is a sequencer vst, it emits midi, each instance outputs midi for a track in your DAW. I'd like to be able to use any instance of the vst to edit the sequence in any other instance. Basically one instance of the vst can turn itself into any other instance, and changes made in one instance will change the other instance's midi output for instance. This is so you don't have to jump between vst instances in the DAW, you just do it from within one, also it has a composition feature where you can arrange tracks and each track has an output to a specific instance. It is decentralized, message passing so they all have consistent state. What C++ libraries would help with this? Or just what general direction would you go? All instances will need the same application state, so you can jump between instances and it is all the same. What is the best way to implement this?

I would **not** make the VST instances fully peer-to-peer. I would use an **auto-launched coordinator process** plus message passing.

Not a special broker plugin the user launches. The VST binary can start a small helper executable when needed. Every plugin instance connects to it. If it is already running, they just join. If it dies, an instance can restart it and re-seed it from the newest local state.

Best architecture:

```text
VST instance A ┐
VST instance B ├── local IPC ── session coordinator/helper ── shared project state/event log
VST instance C ┘
```

Each instance keeps:

```text
instance_id
track_id / output target
local read-only snapshot of global sequencer state
last_seen_revision
```

Edits flow like:

```text
UI edit in any instance
→ send Command to coordinator
→ coordinator validates/applies command
→ increments revision
→ broadcasts Event/Diff to all instances
→ each instance updates its local snapshot
→ audio thread reads its own track from immutable snapshot
```

The important design point: **the audio thread never talks to IPC, never locks, never waits**. It only reads an already-prepared immutable or double-buffered snapshot. IPC happens on a background/message thread.

Libraries I would consider:

| Library                              | Use it for                     | Notes                                                                                                                                                                                                                                        |
| ------------------------------------ | ------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **JUCE `InterprocessConnection`**    | Simplest JUCE-native local IPC | Named pipe/socket messaging; probably enough for a helper-process architecture. JUCE documents it as two-way messaging over socket or named pipe. ([JUCE Documentation][1])                                                                  |
| **NNG**                              | Clean message-pattern IPC      | Good fit if you want request/reply, pub/sub, service discovery-style patterns, IPC/TCP transports. NNG explicitly supports pub/sub, RPC-style request/reply, service discovery-style protocols, and IPC/TCP/websocket transports. ([NNG][2]) |
| **ZeroMQ / cppzmq**                  | More powerful messaging fabric | Mature, high-performance async messaging; supports `inproc`, `ipc`, and `tcp`, but reliable state sync patterns require you to design more yourself. ([ZeroMQ][3]) ([ZeroMQ Guide][4])                                                       |
| **Boost.Interprocess**               | Shared memory / local queues   | Useful if you want shared memory snapshots or local message queues, but it is lower-level than an app-level state protocol. ([Boost][5])                                                                                                     |
| **Protobuf / FlatBuffers / msgpack** | Message serialization          | Useful once your command/event schema stabilizes.                                                                                                                                                                                            |

My default choice: **JUCE `InterprocessConnection` first**, unless you already know you want NNG’s messaging patterns. It keeps the dependency surface smaller and fits a JUCE app naturally. If you expect the IPC system to become a serious subsystem with discovery, request/reply, pub/sub, heartbeats, and reconnect behavior, I would pick **NNG**.

I would avoid “true decentralized replicated state” unless you really need it. A sequencer state wants ordering, revisioning, undo/redo, late-join snapshots, and deterministic MIDI output. A coordinator with an event log is much simpler and more reliable than CRDT/gossip-style peer replication.

The core pieces I would implement:

```text
Command        = user intent, e.g. SetNotePitch, AddTrack, MoveRegion
Event          = accepted state transition with revision number
Snapshot       = full project state at revision N
InstanceHello  = instance_id, plugin version, current revision
Heartbeat      = liveness
TrackBinding   = track_id → instance_id / midi output target
```

For consistency:

```text
Every event has:
revision
source_instance_id
command_id
affected_track_ids
payload
```

For reconnect/late join:

```text
instance connects
→ sends last_seen_revision
→ coordinator sends missed events
→ or sends full snapshot if too far behind
```

For failure recovery:

```text
helper dies
→ instances detect disconnect
→ one instance wins a lock-file / named-mutex election
→ starts helper
→ helper asks clients for newest revision
→ newest snapshot seeds the new coordinator
```

So the direction I would go is:

**auto-launched helper process + local IPC + authoritative event log + immutable audio-thread snapshots.**

That gives you the user experience of decentralized instances, without actually making consistency decentralized.

[1]: https://docs.juce.com/master/classjuce_1_1InterprocessConnection.html?utm_source=chatgpt.com "juce::InterprocessConnection Class Reference"
[2]: https://nng.nanomsg.org/man/v1.2.2/nng.7.html?utm_source=chatgpt.com "nng(7) - Nanomsg"
[3]: https://zeromq.org/get-started/?utm_source=chatgpt.com "Get started"
[4]: https://zguide.zeromq.org/docs/chapter2/?utm_source=chatgpt.com "Chapter 2 - Sockets and Patterns"
[5]: https://www.boost.org/libs/interprocess/?utm_source=chatgpt.com "Chapter 15. Boost.Interprocess"


-------

I agree with the helper/coordinator direction, and this codebase is already shaped well for it. I would not start by adding IPC directly into `MidiEngine` or the web bridge. I would first make the current single-instance state model explicit enough to replicate.

The current important boundaries are:

- [include/xen/state.hpp](/home/anthony/Documents/cpp/XenSequencer/include/xen/state.hpp:60): `ProjectState` is currently one `Measure` plus one `PitchSystem`.
- [include/xen/state.hpp](/home/anthony/Documents/cpp/XenSequencer/include/xen/state.hpp:150): `PluginState` owns local workspace/library/session state plus `XenTimeline`.
- [src/xen_processor.cpp](/home/anthony/Documents/cpp/XenSequencer/src/xen_processor.cpp:332): `XenProcessor::execute_command_string` is the command mutation choke point.
- [include/xen/engine_state_mailbox.hpp](/home/anthony/Documents/cpp/XenSequencer/include/xen/engine_state_mailbox.hpp:20): `EngineStateMailbox` already gives you the right audio-thread handoff model.
- [src/midi_engine.cpp](/home/anthony/Documents/cpp/XenSequencer/src/midi_engine.cpp:270): `MidiEngine::update` renders the current project’s single measure.
- [src/webview_bridge.cpp](/home/anthony/Documents/cpp/XenSequencer/src/webview_bridge.cpp:653): the UI already talks through a JSON request/response bridge.

**Main Design Change**

I would split state into three domains:

```cpp
struct TrackId { std::string value; };
struct InstanceId { std::string value; };

struct TrackState
{
    TrackId id;
    std::string name;
    Measure measure;
    PitchSystem pitch;
};

struct ProjectState
{
    std::vector<TrackState> tracks;
    TrackId focused_track;
    // possibly composition/arrangement state later
};

struct InstanceBinding
{
    InstanceId instance_id;
    TrackId output_track;
};
```

Right now `ProjectState` is effectively “one track.” For your intended workflow, that should become “the whole composition.” Each plugin instance then renders only its bound `TrackId`.

That means `MidiEngine::update(ProjectState const &, DAWState const &)` should eventually become something like:

```cpp
void MidiEngine::update(ProjectState const &project,
                        TrackId const &track_id,
                        DAWState const &daw);
```

or better, keep the audio layer smaller:

```cpp
void MidiEngine::update(TrackState const &track, DAWState const &daw);
```

Then `XenProcessor` owns an `InstanceBinding` and publishes only the selected track’s `AudioProjectSnapshot` to the audio thread.

**Where IPC Should Attach**

I would introduce a new core/service boundary between `XenProcessor` and command execution:

```text
WebviewBridge
  -> XenProcessor::submit_command
    -> SessionClient
      -> coordinator process
        -> authoritative CommandService
          -> ProjectState / Timeline
```

But I would keep a local implementation too:

```cpp
class ProjectSession
{
public:
    virtual auto snapshot() const -> ProjectSnapshot = 0;
    virtual auto submit_command(CommandRequest const &) -> CommandResult = 0;
    virtual auto instance_binding() const -> InstanceBinding = 0;
};
```

Then you can have:

```text
LocalProjectSession        current in-process behavior, useful for tests
IpcProjectSessionClient    talks to helper
CoordinatorProjectSession  helper-side authoritative state
```

This keeps `WebviewBridge` and most UI code from caring whether the project is local or shared.

**Command Model**

I would not send arbitrary “replace project” messages between instances as the normal path. Send commands or accepted events.

A reasonable protocol for this app:

```cpp
struct CommandRequest
{
    std::string command_id;
    InstanceId source_instance_id;
    std::string command_text;
    CommandContext context;
};

struct ProjectEvent
{
    std::uint64_t session_revision;
    std::string command_id;
    InstanceId source_instance_id;
    CommandApplicationResult result;
    ProjectSnapshot snapshot;
};
```

The coordinator should be the only writer of `session_revision`.

Your existing `ProjectRevision` is currently process-local and allocated from a static atomic in [include/xen/timeline.hpp](/home/anthony/Documents/cpp/XenSequencer/include/xen/timeline.hpp:39). That is fine inside one process, but for cross-process sync I would add a separate coordinator-owned revision and eventually make project/history ids deterministic or serialized from the coordinator. Do not rely on each plugin instance allocating matching revision ids.

**Important Correction To The Existing Design**

Right now `execute_command_string` mutates `plugin_state` directly, including history, command session, workspace effects, and project publishing. For shared sessions, I would refactor it into smaller pieces before adding IPC:

```text
parse/bind command
validate expected revision
apply command transaction to a supplied PluginState
return result + changed domains
```

Then both the local processor and coordinator can reuse the same command applier.

The coordinator can own:

```cpp
PluginState shared_state;
std::map<InstanceId, InstanceBinding> bindings;
std::vector<ProjectEvent> event_log;
```

Each plugin instance owns:

```cpp
InstanceId instance_id;
InstanceBinding binding;
ProjectSnapshot replicated_snapshot;
EngineStateMailbox pending_engine_state_update;
MidiEngine midi_engine;
```

**Audio Thread**

The current `EngineStateMailbox` design is exactly the right pattern. Keep it.

When an IPC event arrives on a background/message thread:

```text
receive ProjectEvent
update local replicated snapshot
extract bound TrackState
publish AudioProjectSnapshot/AudioTrackSnapshot to EngineStateMailbox
notify webview state.changed
```

The audio thread should continue doing only this kind of work:

```text
consume latest prepared snapshot if present
if changed, render MIDI
emit block MIDI
```

No sockets, no locks, no coordinator calls from `processBlock`.

**State Saving In DAWs**

Each instance still needs to save enough state into the DAW project to rejoin correctly:

```text
session_id
instance_id
bound_track_id
last_seen_session_revision
last known project snapshot, as recovery seed
```

That is different from today’s [getStateInformation](/home/anthony/Documents/cpp/XenSequencer/src/xen_processor.cpp:302), which saves only the current `ProjectState`.

I would make the saved payload a new plugin-state schema, not just project schema:

```json
{
  "schema": 2,
  "session_id": "...",
  "instance_id": "...",
  "bound_track_id": "...",
  "last_seen_revision": 42,
  "project": { ... }
}
```

On restore, instances connect to the helper by `session_id`. If no helper exists, the first restored instance starts it and seeds it from the newest saved snapshot among connecting instances.

**Library Choice**

For this app I would start with JUCE `InterprocessConnection`, because:

- You already depend heavily on JUCE.
- The traffic is local-machine only.
- Your protocol can be JSON initially using existing `nlohmann::json`.
- The hard problem is project authority/revisioning, not socket abstraction.

I would move to NNG only if you want formal pub/sub and request/reply patterns with less hand-written connection management.

**Implementation Order**

1. Refactor command application out of `XenProcessor` into a reusable `CommandService` or `ProjectCommandApplier`.
2. Change `ProjectState` from single-measure to multi-track.
3. Change `MidiEngine` update path to render one bound track.
4. Add `InstanceId`, `TrackId`, and `InstanceBinding`.
5. Add a local `ProjectSession` interface and make `WebviewBridge` talk through that instead of directly mutating `XenProcessor`.
6. Add the helper process and JUCE IPC.
7. Add event log, late join snapshot, heartbeat, and recovery election.

The clean version for XenSequencer is: one authoritative session timeline, replicated snapshots in every plugin instance, per-instance track binding for MIDI output, and the existing mailbox pattern preserved for audio safety.

Commit-message-sized summary:

```text
Design shared multi-instance project session around coordinator-owned timeline
```
