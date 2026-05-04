# Remote Multiplayer Plan

Goal: online co-op that keeps the local player feeling offline-responsive, even
across high-latency links. Remote players, enemies, pickups, particles, and even
some world changes may visibly correct or arrive late. Local movement and local
interaction should not wait on the network.

## References

- GGPO rollback model: https://www.ggpo.net/
- Valve Source multiplayer networking / prediction / lag compensation:
  https://developer.valvesoftware.com/wiki/Source_Multiplayer_Networking
- Valve lag compensation notes:
  https://developer.valvesoftware.com/wiki/Lag_Compensation
- Factorio lockstep / latency hiding notes:
  https://www.factorio.com/blog/post/fff-147
- Factorio latency state issue notes:
  https://factorio.com/blog/post/fff-302
- Synthetik official networking FAQ:
  https://www.synthetikgame.com/faq
- Synthetik co-op feature summary:
  https://www.co-optimus.com/game/4937/pc/synthetik.html

## Non-Goals

- Do not use pure deterministic lockstep for action gameplay. It protects global
  determinism but makes local control feel tied to network latency.
- Do not require full-game rollback as the first architecture. It is expensive in
  this engine because `State` currently includes stage tiles, entities, fluids,
  lighting, particles, audio emitters, debug/runtime state, RNG, and generation
  artifacts.
- Do not optimize for competitive fairness first. This is cooperative and
  permissive; correctness matters, but local responsiveness matters more.

## Decided Model

Use trusted co-op, event-sourced, coordinator-ordered networking.

- Player count is N-player by design. Four players is the first practical test
  target, not a code assumption.
- A process may own multiple local players. Example: two people on one machine
  and three on another should be represented as five `PlayerId`s in one shared
  session.
- Local multiplayer does not route through transport. Local player slots operate
  directly on the world through the same `PlayerId -> input -> entity` routing
  used by remote players.
- No peer waits for remote input before simulating local control.
- Clients own their local player, held/back items, immediate local actions, and
  the short-lived visual consequences of those actions.
- Clients broadcast durable results, not low-level intentions.
- One peer is a session coordinator. It assigns ids, orders durable events,
  breaks ties, sends repair snapshots, and handles stage transitions. It is not a
  physics authority.
- Peers apply durable events idempotently. If an event arrives late but still
  applies cleanly, apply it. If it conflicts, resolve through deterministic
  priority.
- Cheating and modified clients are treated as a social/lobby compatibility
  problem, not as a reason to make every action server-authoritative.

This is not lockstep, not GGPO-style whole-game rollback, and not classic
client/server physics authority. It is deliberately permissive because Splonks is
trusted co-op and source-available.

## Architecture Boundary

Do not convert the whole game to local event-driven gameplay. Normal offline
gameplay should remain direct: entity code mutates entity/tile state immediately
so controls stay simple and responsive.

Use gameplay events only as a replication/progression seam:

- Gameplay code emits durable facts after the local mutation happened.
- The replication layer converts those facts to `NetEvent`s and packets.
- The progression layer handles stage-exit requests/sync.
- Entity code must not construct network packets or know about sockets.
- Networking should not know entity-specific rules like sacrifice, shop
  behavior, weapon hitboxes, or tool internals.

This gives a permissive co-op model without forcing every behavior through an
abstract event bus. If a gameplay fact is not durable or not useful for remote
readability, it should stay local.

## Source Authority Rule

Durable interactions are source-owned, not target-owned.

- If a local player or locally-owned held/projectile entity hits, carries,
  drops, throws, or otherwise mutates another entity, that local source reports
  the durable fact.
- This includes remote player targets. Picking up or hitting a remote player
  must replicate from the holder/attacker, not be filtered just because the
  target body is remote on this machine.
- Remote-owned projectiles/melee replicas should not independently author damage
  to our local player. Their owner sends the damage event; our machine applies
  that event if it names an explicit source entity.
- Generic remote damage without a named source should not take over local
  player bodies, because stale enemy overlap can otherwise produce bogus hits.

This rule avoids the worst double-authoring case: both the attacker's machine
and the target's machine deciding they are the source of truth for the same hit.

## Current First Pass Status

Implemented:

- Remote player snapshots with interpolation, including animation state.
- Remote player replicas are display-driven only; they do not run local gameplay
  physics/control.
- Coordinator-ordered tile events for tile breaks, tile changes, and rope tile
  placement.
- Tool-spawned entity events for things like grenades/ropes/arrows that need to
  exist on other peers.
- Entity damage/death events that replicate final health/condition and route
  remote deaths through the normal death callback path.
- Damage events also carry impact state for pos/vel/acc/stun, so player hits can
  knock back and stun across peers instead of only changing health.
- Generic entity state patch events for moved replicated props such as pushblocks.
- Player bodies use player-derived network entity ids instead of deterministic
  stage ids.
- Entity held/thrown/drop event packets exist for carry ownership, including
  player-carry chains.

Not yet implemented:

- Reliable delivery for durable events. Durable events are ordered and
  idempotent after receipt, but there is not yet a proper ack/retry layer, so a
  UDP packet dropped before the coordinator sees it can still permanently fork
  world state.
- Tool slot/count replication.
- Back-slot replication.
- Durable item pickup/buy events.
- Repair snapshots for correcting late or conflicting event histories.

Near-term priority:

- Add reliable durable-event delivery before widening multiplayer much further.
  Local tests can pass without it, but internet play will randomly desync under
  packet loss until durable events are acked and retried.

## Synthetik Notes

Public details about Synthetik Ultimate / Synthetik: Legion Rising's exact
replication model are limited. What is visible:

- Official FAQ describes player-hosted lobbies, UPnP/port forwarding, game UDP
  ports, firewall issues, and IPv6 hosting limitations.
- Official/site-facing co-op description is 2-player online co-op with item,
  perk, buff, and loot sharing.
- Steam forum reports point to routing/server-region/connectivity problems and
  some multiplayer-only performance/crash issues.

Inference: Synthetik's good-feeling co-op is probably not deterministic lockstep.
It appears to be host/lobby based and permissive enough that local play does not
feel like waiting for remote inputs. We should copy that feel, not assume its
implementation is ideal or fully known.

## Ownership Lanes

### Player Body

- Owner: controlling client.
- Local simulation: immediate.
- Network: send periodic player-state events/snapshots.
- Remote peers display the latest received player state with interpolation.
- Coordinator does not validate normal movement. Hard repair only if a peer's
  state graph becomes corrupt or the stage transition/death state conflicts.

### Held / Carried Items

- Owner: holder's client while held.
- Pickup is a durable event: `PickupEntity(player, item)`.
- The picker applies pickup immediately and broadcasts the event.
- Conflicting pickups resolve by coordinator event order. Losing peers undo or
  ignore their local pickup if needed.
- Throw/drop/use is broadcast as durable result events, not replayed by remote
  physics from input.

### Projectiles / Tools

- Owner: spawning client.
- Local actor sees projectile/tool results immediately.
- Broadcast results:
  - `SpawnProjectile`
  - `ProjectileHitEntity`
  - `ProjectileHitTile`
  - `BreakTile`
  - `KillEntity`
  - `SpawnEntity`
- Remote peers may render the projectile path approximately. Durable results
  come from explicit events.

### Enemies / World Props

- Default owner: coordinator, unless a client has an active interaction with the
  entity.
- Any client may broadcast a durable interaction result: hit, stun, kill, pickup,
  sacrifice, telefrag, push, or break.
- Conflicting events resolve by ordered event application and idempotent checks.
- Remote peers accept plausible-looking outcomes. They do not replay exact
  physics to prove the event.

### Stage Tiles / Fluids / Lighting

- Tile changes are durable ordered events.
- The actor applies them immediately and broadcasts.
- Coordinator orders them and rebroadcasts if necessary.
- Fluids, lighting, particles, and audio are locally simulated from current
  durable state. They do not need exact cross-peer parity.

## Event Design

Every durable gameplay result should be an idempotent event:

- `event_id`
- `source_player_id`
- `source_entity_vid` or network entity id
- `stage_frame` or network tick
- payload

Examples:

- spawn entity
- deactivate entity
- damage/kill entity
- pickup item
- transfer ownership
- break tile
- place rope tile
- explode bomb
- alter quest state
- disturb shop
- change favor
- stage transition

Events must be safe to receive twice and safe to receive out of order within a
small window. This matters more than exact deterministic simulation.

## Required Durable Events

Start small, but design the envelope so adding more event types is mechanical.

Core session:

- `PeerJoined`
- `PeerLeft`
- `PlayerSpawned`
- `PlayerDespawned`
- `StageLoaded`
- `StageTransitionStarted`
- `StageTransitionCommitted`
- `RepairSnapshot`

Entity graph:

- `EntitySpawned`
- `EntityDeactivated`
- `EntityStatePatched`
- `EntityOwnerChanged`
- `EntityHeld`
- `EntityDropped`
- `EntityThrown`
- `EntityDamaged`
- `EntityKilled`
- `EntityStunned`

World:

- `TileChanged`
- `TileBroken`
- `TileTriggerFired`
- `RopeTilePlaced`
- `FluidPatched`

Inventory/economy/quest:

- `ToolSlotChanged`
- `EffectAdded`
- `EffectRemoved`
- `MoneyChanged`
- `FavorChanged`
- `ShopDisturbed`
- `ShopItemBought`
- `QuestFlagChanged`

Action-specific first targets:

- `BombPlaced`
- `BombExploded`
- `RopeThrown`
- `ArrowFired`
- `ProjectileHit`
- `MattockDug`
- `TeleporterUsed`
- `CrateOpened`
- `ChestOpened`
- `SacrificeApplied`

Local-only events should not be networked: particles, short-lived audio, camera
shake, debug annotations, and cosmetic lighting flicker. If a cosmetic effect is
important for remote readability, send the durable cause and let peers spawn the
effect locally.

## Network Identity

Local `VID` values are not enough across machines. Use stable player/session ids:

- `NetEntityId`: stable for replicated entities during a stage.
- `PlayerId`: stable gameplay player identity. This is not network-owned; local
  multiplayer and remote multiplayer both use it.
- `NetEventId`: monotonic per event source.
- `StageInstanceId`: identifies a generated stage instance and seed.

Each peer maps `NetEntityId -> local VID`. The coordinator assigns ids for
coordinator-created entities; clients assign from client-owned id ranges for
local action results that need to exist immediately. Local-only effects,
particles, and annotations do not need network ids.

## Required Data Structures

Minimal first pass:

```cpp
using NetEntityId = std::uint64_t;
using NetEventId = std::uint64_t;
using StageInstanceId = std::uint64_t;

enum class NetRole {
    Offline,
    Coordinator,
    Peer,
};

struct NetPeerState {
    PlayerId player_id;
    std::string display_name;
    float estimated_ping_ms;
    float jitter_ms;
};

struct NetEntityLink {
    NetEntityId net_id;
    VID local_vid;
};

struct NetEventHeader {
    NetEventId event_id;
    PlayerId source_player_id;
    StageInstanceId stage_instance_id;
    std::uint64_t source_local_frame;
    std::uint64_t coordinator_order;
};

struct PlayerSlot {
    PlayerId player_id;
    std::optional<VID> entity_vid;
    PlayerConnectionKind connection_kind; // local or remote
    bool connected;
    bool primary_local;
    PlayingInputs inputs;
    PlayingInputs immediate_inputs;
};
```

Store networking state outside pure entity behavior. Entities should not know
about sockets or peers. Entity-owned logic can emit local gameplay events through
helpers; the networking layer decides whether those events are local-only or
durable replicated events.

## Required Code Boundaries

Networking should live in its own module, for example:

- `src/network/net_ids.hpp`
- `src/network/net_event.hpp`
- `src/network/net_session.hpp`
- `src/network/net_transport.hpp`
- `src/network/net_fuzzer.hpp`
- `src/network/net_replication.hpp`
- `src/network/net_debug_ui.hpp`

Gameplay systems should expose durable-event hooks without importing transport:

- entity spawn/deactivate
- damage/death/stun
- pickup/drop/throw
- tool inventory changes
- tile break/change
- stage transition
- shop/favor/quest state mutation

Avoid putting entity-specific networking cases in engine code. If `SacAltar`
creates favor, it emits or requests a generic `FavorChanged`/`SacrificeApplied`
event. The network layer serializes the event; it does not know sacrifice rules.

## Transport

Use UDP or a UDP-based library. TCP alone is the wrong default for action
replication because head-of-line blocking will turn packet loss into visible
stutter.

Packet classes:

- Unreliable frequent: inputs, player snapshots, entity snapshots, pings.
- Reliable ordered or reliable sequenced: stage load, entity spawn/despawn,
  tile changes, inventory changes, quest state, shop/favor, ownership transfer.
- Reliable unordered: asset/content handshake, chat/debug messages.

If we choose a library later, evaluate Steam Networking Sockets, ENet, GameNetworkingSockets,
or a minimal custom UDP layer. Do not bind gameplay architecture to the transport.

### Durable Event Reliability

Current implementation warning:

- `NetEvent` durable payloads are coordinator-ordered and idempotent once
  received.
- They are not yet guaranteed to be received. UDP packet loss can still drop a
  peer-to-coordinator event before the coordinator knows it exists.
- Transient player snapshots and entity state patches intentionally remain
  unreliable and should never be retried; newer snapshots replace older ones.

Reliable durable-event layer requirements:

- Peers keep locally emitted durable events in a retry queue until the
  coordinator echoes or explicitly acks them.
- Coordinator assigns `coordinator_order` and keeps ordered durable events in a
  per-peer resend queue until that peer acks receipt/application.
- Add ack packets containing `stage_instance_id`, highest contiguous
  `coordinator_order` applied, and optionally sparse event ids for gaps.
- Resend unacked durable events on an interval with a cap/backoff so packet loss
  cannot create unbounded traffic.
- Prune ordered history only after every connected peer has acked, or after a
  peer disconnects/falls back to repair snapshot.
- Keep transient state patches out of durable history. They are packet-loss
  tolerant and should not participate in ack/retry.
- If a peer falls too far behind the retained durable history, send a repair
  snapshot or force a stage reload instead of replaying an unbounded log.

Durable event classes that must use ack/retry:

- Tile breaks/changes and rope tile placement.
- Tool-spawned entities such as grenades, arrows, ropes, bombs.
- Entity damage/death and persistent entity deactivation.
- Pickup/drop/throw/held/back ownership changes.
- Tool slot/count changes.
- Shop/favor/quest state changes.
- Stage transition, respawn, and repair snapshot commits.

## Reconciliation Rules

Prefer correction hierarchy:

1. Do nothing if divergence is below a visual threshold.
2. Smooth remote entities toward received owner snapshots.
3. Soft-correct local optimistic world overlays when ordered durable events differ.
4. Hard-correct local player only for impossible state, death, transition, or
   unrecoverable stage mismatch.

The local player should never wait for:

- walking/running/jumping/climbing/hanging
- using held/back items
- throwing
- shooting
- placing bombs/ropes
- teleporter preview and activation

Ordered durable events may later correct consequences.

## Remote Movement Before Durable World Events

Basic remote movement comes before replicated tile/entity mutations. Durable
events prove shared-world correctness, but they are hard to evaluate if remote
players still look like packet-snapped puppets.

Remote player movement path:

- Add interpolation state keyed by `PlayerId`.
- On snapshot receive, store target position, velocity, facing, condition, and
  grounded state instead of writing entity position directly from the packet.
- Each frame, move remote player entities toward their newest target after local
  simulation.
- Snap only when the correction exceeds a debug-tunable distance threshold.
- Debug controls should expose snapshot send interval, interpolation strength,
  interpolation delay, and snap distance.
- Test with two or three separate processes, including multiple local players
  owned by one process.

After movement is readable, add the first reliable durable event. `TileBroken`
is the preferred first target because it proves "one player changed the shared
world and every process saw it" without item ownership complexity:

1. Packet carries event id, source player, event kind, and payload.
2. Peer applies its own tile break locally immediately and sends `TileBroken`
   to the coordinator.
3. Coordinator assigns order, applies if still relevant, and rebroadcasts.
4. Peers apply idempotently. Already-air/already-broken tile is a no-op.
5. Debug event log shows sent, received, ordered, applied, duplicate, and no-op
   tile events.

Then do `EntityDamaged`/`EntityKilled`, then pickup/drop/throw ownership.

## Conflict Resolution

Because clients broadcast results, conflicts must be boring and deterministic.

Default priority:

1. lower `coordinator_order`
2. lower `event_id` from the same source
3. lower `source_player_id` as final tie-break

Examples:

- Two players pick up the same item: first ordered `EntityHeld` wins. The loser
  drops/clears their local optimistic hold.
- Two players kill the same enemy: first `EntityKilled` wins. Later damage/kill
  events against inactive/dead entity become no-ops.
- Two bombs break the same tile: first `TileBroken` changes it. Later duplicate
  breaks are no-ops but can still spawn local cosmetics if desired.
- One player buys an item while another steals it: event order decides whether
  `ShopItemBought` or theft/disturbance applies first.

This needs idempotent apply functions. Most bugs in this model will come from
events that assume the old state still exists.

## Desync Policy

Desync is not binary failure. Classify it:

- Cosmetic desync: particles, audio, lighting, minor water shape. Ignore.
- Soft gameplay desync: remote enemy position, loose item position. Smooth or
  snap remote only.
- Local prediction mismatch: predicted pickup/hit/tile break rejected. Resolve
  with a small visual correction and avoid repeated local snap.
- Hard desync: stage tiles, quest flags, inventory, active entity graph, or shop
  ownership diverge. Coordinator sends a repair snapshot or forces stage resync.

## Local Multiplayer

Local multiplayer should be a simpler version of the same model:

- Multiple controlled player entities in one `State`.
- No network transport.
- No ownership transfer across machines.
- Same player id and input-command concepts.

This is useful as a stepping stone, but it should not define the remote model.

## Implementation Phases

1. Define networking ids and event envelope.
   - Add `PlayerId`, `NetEntityId`, `NetEventId`, `StageInstanceId`.
   - Add local mapping between `NetEntityId` and `VID`.
   - Add coordinator-order field, even before real networking exists.
2. Add an in-process event bus for durable gameplay events.
   - Events are emitted locally and applied locally in offline mode.
   - This lets us migrate gameplay to event-sourced durable changes before
     sockets exist.
3. Convert key durable systems to emit/apply events.
   - Start with spawn/deactivate, pickup/drop/throw, tile break/change,
     tool inventory changes, damage/death, and stage transition.
4. Support multiple player ids locally.
   - Spawn multiple player entities in one `State`.
   - Route input by `PlayerId`.
   - This gives local multiplayer and exercises event ownership.
5. Add loopback coordinator/peer mode in one process.
   - Same code path as network mode, but transport is an in-memory queue.
   - Add network fuzzer here, before UDP.
6. Replicate remote player snapshots/events over loopback.
   - Local player remains instant.
   - Remote players are just event/snapshot driven.
7. Add reliable event stream transport.
   - UDP-based transport eventually, but loopback first.
   - Reliable ordered stream for durable events.
   - Unreliable stream for frequent player/entity snapshots.
8. Add repair snapshots.
   - Serialize durable stage/entity/tool/quest state.
   - Ignore particles/audio/debug-only state.
   - This aligns with `docs/state_playstate_replay_split.md`.
9. Add first real two-process LAN test.
   - Join, stage load, two players visible, pickup/drop/throw, tile break,
     enemy kill, exit transition.
10. Add internet/session layer.
   - Lobby, direct connect, NAT/relay choice, reconnect.
   - Host migration only if needed.

## Implementation Status

- [x] Added foundational network ids/event/session types.
  - Files: `src/network/net_ids.hpp`, `src/network/net_event.hpp`,
    `src/network/net_session.hpp`, `src/network/net_session.cpp`.
  - `State` now owns `network::NetSessionState net_session`, initialized in
    offline mode.
- [x] Added the gameplay player registry spine.
  - Files: `src/player_id.hpp`, `src/player_registry.hpp`,
    `src/player_registry.cpp`.
  - `State` owns `PlayerRegistry players`; current `player_vid` remains
    compatibility for primary local player while gameplay code is swept.
  - Control intent now resolves through `PlayerId -> entity VID -> inputs`
    before falling back to legacy `controlled_entity_vid`.
  - The playing tick runs control logic for every player slot with an entity;
    `controlled_entity_vid` is fallback for legacy debug possession only.
  - Entity stepping now processes all registered player-slot entities first,
    then skips them in the normal entity pass.
- [x] Added debug local multiplayer bots.
  - `Debug: Network` can add/remove local debug player slots that spawn normal
    player entities and feed random movement/jump inputs through the registry.
  - Tool usage is disabled by default and can be toggled per bot.
- [x] Added network fuzzer config/stat shell and latency presets.
  - Files: `src/network/net_fuzzer.hpp`, `src/network/net_fuzzer.cpp`.
- [x] Added a debug-drivable local event queue, ordered queue, and first apply
  path.
  - Files: `src/network/net_event_apply.hpp`,
    `src/network/net_event_apply.cpp`, `src/debug/playback_ui_network.cpp`.
  - The `Debug: Network` window can emit a local `MoneyChanged` event, drain it
    into coordinator order, and apply it to visible money.
- [x] Add in-process durable event bus apply/drain point in the gameplay step.
  - Offline/coordinator sessions drain local events into ordered events, then
    apply ordered events once per playing tick.
- [x] Added first real UDP host/join transport.
  - Files: `src/network/net_transport.hpp`,
    `src/network/net_transport.cpp`, `src/network/net_protocol.hpp`,
    `src/network/net_protocol.cpp`, `src/network/net_lobby.hpp`,
    `src/network/net_lobby.cpp`.
  - `Debug: Network` can host a UDP socket, join `host:port`, assign explicit
    coordinator-owned `PlayerId`s for each local player in the joining process,
    and spawn/register remote player slots.
  - Hosts can accept multiple joining processes. Join packets report local
    player count so one process can reserve more than one `PlayerId`.
  - Host join accepts carry `quest_id`, `quest_stage_id`, and `stage_seed`.
    If hosting starts from an unseeded quest stage, the host chooses a sync seed
    and reloads that stage before accepting joins.
  - Peers load the synced quest stage before spawning assigned local players and
    remote host/peer players.
  - Unreliable player snapshots now flow peer -> host -> other peers, and host
    local player snapshots flow host -> peers.
  - This slice proves N-process UDP session setup, seed-synced quest-stage
    loading, and remote player motion snapshots. Reliable durable-event
    transport, object/tool ownership, local multi-input routing, and repair
    snapshots are still separate follow-up work.
- [x] Added remote player interpolation targets for UDP snapshots.
  - Snapshot receives now update a target per remote `PlayerId`; the remote
    entity moves toward that target after normal local simulation.
  - `Debug: Network` exposes snapshot interval, interpolation strength, snap
    distance, and the current remote target list.
- [x] Added remote player animation state to UDP snapshots.
  - Player snapshots now carry animation id, frame, time, speed, and animate
    flag so remote player bodies do not freeze in stale local animation states.
- [x] Added first UDP durable world mutation: `TileBroken`.
  - Central stage tile breaking emits generic `TileBroken` events.
  - Peers send pending tile breaks to the coordinator until the ordered echo is
    seen. The coordinator assigns order, applies idempotently, and rebroadcasts
    ordered tile breaks to connected peers.
- [x] Added repo-local two-process multiplayer launcher and read-only live CLI.
  - `scripts/run_multiplayer_pair_i3.sh` builds, opens workspace 2 on
    `DisplayPort-0`, launches the top instance as host and the bottom instance
    as joiner, and passes debug-control ports.
  - `scripts/splonksctl` queries live state through the localhost-only
    debug-control server.
- [ ] Convert first gameplay systems to emit/apply durable events.
- [x] Add reliable-ish coordinator-ordered `TileBroken` as the first durable UDP
  world mutation.
- [x] Add multiple local player ids/entities.
- [x] Add ordered entity carry events.
  - `EntityHeld`, `EntityDropped`, and `EntityThrown` have packet encoding,
    coordinator relay, and apply paths.
  - Player-carry chains use the normal entity carry references; attachment sync
    runs multiple passes so `player0 -> player1 -> player2` resolves
    deterministically.
- [ ] Add ack/retry reliable delivery for durable events.
  - Peer local durable events retry until coordinator echo/ack.
  - Coordinator ordered durable events retry per peer until ack.
  - Transient player/entity snapshots stay lossy and are never retried.
- [ ] Add loopback coordinator/peer transport and route it through the fuzzer.

## First Vertical Slice

Do not start with every system. The first slice should prove the model:

- Two players in the same debug room.
- Loopback transport with fuzzer.
- Local player control has zero added latency.
- Remote player snapshots interpolate.
- One shared rock can be picked up, thrown, and resolved if both players race
  for it.
- One tile can be broken by mattock/bomb as a durable event.
- One enemy can be killed by either player as a durable event.
- Stage repair snapshot can fix an intentionally injected mismatch.

If this slice feels bad under `150 ms / 25 ms jitter / 1% loss`, the model needs
adjustment before adding shops, sacrifices, fluids, or full quest progression.

## Debugging Requirements

- Network graph: ping, jitter, packet loss, input age, snapshot age.
- Per-entity network owner/coordinator overlay.
- Event log filtered by player/entity/event id.
- Prediction overlay view for local-only tile/entity changes.
- Desync checksum panels for durable state only.
- Artificial lag/loss/jitter controls in debug UI.
- Localhost debug-control CLI for querying live processes while reproducing
  desyncs. This is for development inspection, not gameplay protocol.
  - Host test instance defaults to port `41000`.
  - Joiner test instance defaults to port `41001`.
  - `scripts/splonksctl --port 41000 status`
  - `scripts/splonksctl --port 41001 players`
  - `scripts/splonksctl --port 41000 entities near 128 64`
  - `scripts/splonksctl --port 41000 entity 0`
  - `scripts/splonksctl --port 41000 tiles 0 0 8 8`
  - `scripts/splonksctl --port 41000 net`
  - `scripts/splonksctl --port 41000 perf`
  - Keep it localhost-only. Mutating commands can come later, after read-only
    inspection proves useful.

## Network Fuzzer / Degradation Tool

Add this early. It should work before real internet multiplayer is stable.

Controls:

- one-way latency per peer/direction
- jitter per peer/direction
- packet loss percentage
- packet duplication percentage
- packet reordering window
- bandwidth cap
- burst loss mode
- reliable-message delay/drop simulation before retransmit
- clock drift simulation

Modes:

- loopback coordinator/peer in one process
- two local processes on the same machine
- LAN
- real internet

Debug output:

- per-channel send/receive packet counts
- dropped/late/duplicated/reordered packet counts
- event ack age
- snapshot age
- local prediction age
- correction count by severity
- hard resync count

Useful presets:

- LAN: `5 ms`, `1 ms jitter`, `0% loss`
- Same region: `35 ms`, `8 ms jitter`, `0.2% loss`
- US cross-country: `80 ms`, `15 ms jitter`, `0.5% loss`
- Japan to Texas: `150 ms`, `25 ms jitter`, `1% loss`
- Bad Wi-Fi: `90 ms`, `60 ms jitter`, `3% loss`, burst loss enabled

This tool should sit below gameplay replication so every channel goes through it.
If only rendering or high-level events are fuzzed, the test will lie.

## Open Questions

- Coordinator model: listen-server player host first; dedicated coordinator can
  be a later variant.
- Conflict policy for player-vs-player damage if we ever allow it.
- How aggressive should repair snapshots be before they become visually annoying?
- How much state should repair snapshots include before we split `State` and
  `PlayState` properly?

## Recommendation

Build toward trusted co-op, event-sourced durable gameplay, and a lightweight
coordinator that orders events and repairs drift. This gives the desired
Japan-to-Texas feel: local motion and local tools are instant, while other
players and world consequences may arrive late or correct smoothly.
