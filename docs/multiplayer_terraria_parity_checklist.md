# Multiplayer Terraria-Style Parity Checklist

Purpose: define the concrete finish line for Splonks remote multiplayer. This is
the authoritative checklist for making networking behave like a Terraria /
tModLoader-style broad message system: coordinator-owned shared world, local
player responsiveness, broad protocol lanes, concrete world resync, and repeatable
tests.

This document supersedes vague "materially closer" language. A networking change
is not done because one playtest got better. It is done when the relevant gate in
this checklist is implemented, tested, and no longer has known exceptions.

Related docs:

- `docs/plans/remote_multiplayer_plan.md`: architecture rationale and protocol
  lane details.
- `docs/current_multiplayer_cleanup_checklist.md`: short-term cleanup notes for
  the current large commit.
- `docs/network_stage_and_settings_classification.md`: durable/shared/local
  state classification.
- `docs/water_fluid_simulation.md`: fluid model and Terraria liquid references.

## Why We Kept Getting "Closer" But Not Done

The target model has several independent requirements. Fixing one can make the
game feel dramatically better while another still allows permanent divergence.

Examples from recent work:

- Tile mutation sync improved, but player body repair was incomplete, so tiles
  matched while player positions forked.
- Player position repair improved, but fall/stun/body timers were not in the
  player snapshot lane, so local fall damage could still diverge.
- Generic action requests improved tools and shops, but late join/rejoin still
  needs a concrete current-world snapshot, not only stage seed plus event history.

Going forward, we should not say "Terraria-style enough" until all gates below
are checked.

## Definition Of Done

Remote multiplayer is complete for the current classic quest demo when all of
these are true:

- A host can start any supported quest stage and one or more peers can join.
- A peer can join after the stage has already changed, tiles have been broken,
  entities have moved/died/spawned, fluids have changed, and players have
  inventory/effects/back/held items.
- A peer can disconnect and reconnect.
- On reconnect, the player can either resume their previous player slot/state or
  start fresh, depending on the chosen join policy.
- Local player control remains responsive without waiting for a round trip.
- Shared world facts converge from coordinator state: tiles, fluids, entities,
  projectiles, players, inventory/economy, stage progression, and run state.
- Packet loss, packet reorder, duplicate packets, and reconnect do not permanently
  fork durable state.
- New normal content does not need a custom packet family unless it introduces a
  genuinely new durable state category.
- Headless/fake-transport tests prove the important scenarios before playtesting.

## Join Policy

We need explicit policies instead of accidental behavior.

- [x] Assign stable `PlayerId`s per connected local player.
- [x] Support multiple local players per process.
- [x] Support peer disconnect without destroying the run.
- [x] Support reconnect to the same `PlayerId` when the peer presents its
  previously assigned id and coordinator retained state still exists.
- [x] Store disconnected player state separately from the live entity body:
  player id, character type, health, money, tools, effects/passives, held/back
  inventory policy, current stage id, last safe position, disconnect frame/time,
  and any run/player flags needed to resume correctly.
- [x] Remove disconnected live player bodies quickly after disconnect so they
  cannot keep taking damage, triggering contacts, being carried, or blocking the
  run while absent.
- [x] Support reconnect as fresh character at level entrance.
- [x] Support reconnect as fresh character at host position.
- [x] Support reconnect as retained character at level entrance.
- [x] Support reconnect as retained character at last saved position.
- [x] Support reconnect as retained character at host position.
- [x] Decide disconnected-state retention lifetime and cleanup rules.
- [x] Add host/debug controls for reconnect mode during development.
- [x] Add UI/debug controls for reconnect policy during development.

Current status: coordinator disconnect now captures retained player data in a
separate retained-player table, severs carry links, deactivates the live body,
removes the live player slot, and reconstructs a fresh or retained body by
`NetReconnectSpawnMode` when the peer presents the previous `PlayerId`. Retained
reconnect captures and recreates non-player held/back items; fresh reconnect
spawns a default body and discards the retained payload. Reconnects that do not
present a preferred id allocate a new player id. Player-held-player chains are
severed on disconnect rather than saved as inventory. Retained states expire
after the host-configured lifetime, defaulting to 30 minutes, with `0` meaning
keep them until manually cleared. The network debug window exposes reconnect mode,
retention lifetime, retained-player inspection, and manual retained-state clear.
Packet smoke covers same-id reconnect, fresh reconnect, live body removal, and
retained held/back item restore.

## Gate 1: Transport And Reliability

Goal: durable coordinator messages behave like reliable ordered facts even though
the transport is UDP.

- [x] Every peer request has a stable request id.
- [x] Coordinator dedupes repeated requests and applies content at most once.
- [x] Coordinator replies with `ActionRequestAck` for accepted/rejected requests.
- [x] Every durable coordinator result has coordinator order.
- [x] Peers apply durable coordinator results in order.
- [x] Peers ack highest contiguous applied coordinator order.
- [x] Coordinator retains durable history until every connected peer has acked it.
- [x] Coordinator resends unacked durable history.
- [x] Coordinator has a bounded fallback when a peer is too far behind: send an
  explicit same-stage resync marker plus a full world snapshot, or force stage
  resync when the stage itself changed.
- [x] Packet-loss/reorder/duplicate fake-transport tests cover requests, results,
  acks, reconnect, and stage transitions.

Current status: implemented for the current lanes. Packet/frame smoke covers
request ids, request dedupe, accepted and rejected action acks, coordinator
order, ordered result idempotence for reversed/duplicated delivery, dropped
coordinator delivery resend, lost peer request retry, lost explicit action ack,
same-id reconnect, fresh reconnect, dropped initial stage sync, and multi-peer
durable history retention where one peer acking is not enough to prune an event.
A force-resync fallback exists and is tested by corrupting a peer after normal
history has been acked and requiring a fresh world snapshot stream to repair it.
Peers that miss an order older than the coordinator's retained history are not
allowed to stall forever: the coordinator sends a same-stage `StageSync` reset
with the snapshot start order, then streams the current world snapshot from that
order.

## Gate 2: Concrete World Bootstrap And Resync

Goal: joining/rejoining peers receive current world state, not just a seed and a
hope that history replay is enough.

- [x] Define `WorldSnapshot` packet/stream format.
- [x] Include stage identity: quest id, quest stage id, seed, stage instance id,
  frame/stage frame, wrap flags, gravity/settings needed for gameplay.
- [x] Include stage tile-change generation metadata needed by gameplay caches.
- [x] Include foreground tile layer.
- [x] Include backwall tile layer.
- [x] Include tile rotations/metadata needed by gameplay.
- [x] Include stage annotations/triggers only if they are needed after generation;
  otherwise regenerate coordinator-only metadata deterministically.
- [x] Include fluid grid: type, amount, velocity/gravity fields needed for current
  sim/render/gameplay.
- [x] Include all active shared entities with stable net id, type/archetype,
  transform, velocity/acceleration, links, counters, timers, health, condition,
  AI state, flags, animation state, buyable/shop state, effects, and runtime
  replicated flags.
- [x] Include inactive-but-durable state where relevant, or make inactive entities
  absent by definition.
- [x] Include every player slot: connected/disconnected, local/remote identity,
  entity link, health, money, tools, effects, held/back links, wanted state,
  condition, body state, and respawn/death state. Snapshot player-state events
  cover active player bodies. Disconnected retained-player data is
  coordinator-side reconnect state, not a live world body; packet smoke now
  proves all reconnect policies: fresh at entrance, fresh at host, retained at
  entrance, retained at last position, and retained at host. Frame smoke covers
  dead-host transition and held-player respawn.
- [x] Include run/progression state: level number, quest progression flags, boss
  state, exits, rescued/dead players, and pending transition state. The broad
  run-state lane now covers frame/stage frame, depth, score, death count,
  game-over/win flags, stage metadata, classic quest flags, and sacrifice altar
  state. Stage exit definitions are deterministic stage data, while per-entity
  stage-exit ids are covered by `EntityStatePatch`. Pending quest-stage
  transitions are handled by the `StageSync` lane, including loss recovery smoke.
  Boss/rescue-specific state does not exist yet; when those systems are added
  they must extend this same broad progression lane rather than creating
  item-specific network packets.
- [x] Chunk large snapshots so they fit transport packet limits.
- [x] Add checksum/fingerprint after snapshot apply. Packet smoke verifies the
  final run-state event in a world snapshot carries a network-stable fingerprint
  and peers record whether their applied snapshot matches it.
- [x] Add host command to force-resync peers.
- [x] Add fake-transport tests for rejoin after world mutation.

Current status: first concrete snapshot slice exists. The host now sends join
accept with a snapshot start coordinator order, then enqueues a full broad-lane
snapshot stream for foreground/backwall tiles, tile rotations, active entities,
active player state, fluid cells, run/lifecycle state, stage-exit entity ids,
and linked inactive entity deactivations. Snapshot fluid cells are non-blocking
state-repair patches, not durable ordered facts, so one lost fluid packet cannot
stall later tile/entity/grenade/rope application. Empty/default fluid cells are
omitted from snapshots; same-stage force-resync clears the peer fluid overlay
before applying non-empty/non-default fluid patches. Packet smoke validates this
snapshot slice after normal ordered history is discarded, and also validates
force-resync by corrupting peer tile/backwall/rotation/fluid/entity/player/run/
stage-metadata state after history has been acked, including broad entity fields
such as money, stage-exit id, render-enabled state, holding state, and damage
vulnerability.
Packet smoke also covers a late join through
the real join request/accept path after tile/entity/player/fluid mutations.
Snapshots now end with a network-stable fingerprint using replicated entity ids
instead of local VIDs, so late join, reconnect-after-mutation, bootstrap, and
force-resync prove the peer applied the same durable state. Remaining work:
complete future boss/rescued-player state if those systems are added, and full
rejoin scenario coverage through the fake transport.

Stagegen annotations are debug-only and are regenerated when the peer loads the
quest stage from the same quest id/stage id/seed. Stage tile triggers are also
regenerated from stagegen data; tile mutation snapshots then apply current broken
or changed tiles over that deterministic base. Linked inactive entities are
represented by deactivation events in the snapshot stream; unlinked inactive
entities are absent by definition. Snapshot data is already packetized through
the broad lanes rather than packed into one oversized packet.

Known playtest symptoms to classify under this gate, not patch ad hoc:

- Host-created rope tiles can be climbable on peers while the rope visuals do not
  appear, which suggests tile data may arrive without the matching tile
  metadata/render classification or presentation state.
- Host death/respawn can leave the run stuck or camera-confused, which belongs
  to player lifecycle and run/progression snapshot state.

## Gate 3: Player Controls, Prediction, And Repair

Goal: local control feels offline, but coordinator body state eventually wins.

- [x] Peer sends input/control state to coordinator.
- [x] Coordinator simulates remote player body from replicated inputs.
- [x] Coordinator does not accept peer snapshot position as canonical body state.
- [x] Coordinator sends canonical player body snapshots for all players.
- [x] Peers repair their own local predicted player from coordinator snapshots.
- [x] Player repair includes position, velocity, facing, grounded, health,
  coyote, fall timer, stun timer, projectile contact timer, condition,
  `has_physics`, and `can_collide`.
- [x] Player repair includes hang/climb body fields: movement flags,
  jump-hold gravity frames, jump delay, climb detach cooldown, hang count, and
  hang side.
- [x] Player repair includes transform/body-shape fields that affect collision
  and prediction: acceleration, size, and rotation.
- [x] Player repair includes action/body cooldowns that affect repeated gameplay
  decisions: holding timer, bomb throw delay, rope throw delay, attack delay,
  equip delay, and thrown-immunity timer.
- [x] Player repair includes thrown/projectile-contact body fields that affect
  player-as-projectile behavior: `thrown_by`, `can_apply_projectile_contact`,
  projectile contact damage type, and projectile contact damage amount.
- [x] Player repair includes player presentation state needed by remote replicas:
  animation id, animation frame/time/speed, animate flag, loop flag, and
  finished flag.
- [x] Final audit proves every remaining body field that can affect gameplay,
  or those fields are explicitly classified as local presentation.
- [x] Add smoothing/reconciliation policy for small corrections.
- [x] Add hard snap policy for large corrections.
- [x] Add tests for walk/run/jump, fall damage, stun recovery, climbing, hanging,
  swimming, carrying/being carried, dying, respawning, and stage transition.
- [x] Add latency/loss tests so correction does not permanently trap/stun/kill
  the local player.

Current status: implemented for the current player-body surface. Frame smoke now
intentionally corrupts the known hang/climb/jump-delay body fields, action/body
cooldowns, acceleration, size, rotation, and player animation state, then
requires coordinator snapshots to repair them. It also corrupts thrown/projectile-
contact state after a player throw and requires coordinator snapshots to restore
it. Small non-attachment body corrections are smoothed when they are within
`remote_snap_distance`; large corrections snap immediately to the coordinator
body. Body facts such as health, timers, condition, flags, cooldowns, and
animation state apply immediately even when position is smoothed. Attachment-
driven positions are repaired by broad entity/carry lanes instead of being
smoothed as free movement. Frame smoke now proves both the small-correction
smoothing path and the hard-snap path. Because the player snapshot packet grew,
each packet currently carries 4 player entries and the sender chunks additional
player slots into follow-up snapshot packets instead of increasing packet size.
Named scenario coverage now exists in frame smoke: walk/run/jump movement,
latency movement repair, fall damage repair, stun/body repair, climb repair,
hang repair, water/swim effect replication, carry/throw repair, death while
held, held-player respawn, dead-host stage transition, stage transition while
carrying, and dropped initial stage sync. The body loss recovery smoke drops one
coordinator player snapshot, corrupts the peer body into a dead/stunned/
no-physics/no-collision state, then requires the next coordinator snapshot to
restore health, condition, timers, movement flags, collision flags, and position.

Player body audit notes:

- Replicated in the high-frequency player body snapshot: position, velocity,
  acceleration, size, rotation, health, coyote time, fall timer, stun timer,
  projectile contact timer, `thrown_by`, projectile contact damage type/count,
  `can_apply_projectile_contact`, movement flags, jump-hold gravity frames,
  jump delay, climb detach cooldown, hang count, holding timer,
  bomb/rope/attack/equip delay countdowns, thrown-immunity timer, facing,
  condition, grounded, `has_physics`, `can_collide`, hang side, player animation
  id/frame/time/speed/animate/loop/finished state, and input flags.
- Replicated through broad player/entity state lanes instead of body snapshots:
  money, tools, effects, wanted state, held/back links, active/deactivated state,
  non-player animation, buyable/shop state, AI state, counters, points, entity
  money, stage-exit ids, render-enabled state, runtime flags, generic holding
  state, and non-player entity attachment links. Generic entity state patches now
  include `damage_vulnerability`, so dynamic immunity/vulnerability changes are
  part of the broad lane and the network fingerprint.
- Intentionally local/transient/presentation-only for body repair: `use_state`
  button edges, `jumped_this_frame`, `last_condition`, `last_ai_state`,
  `dist_traveled_this_frame`, travel/contact sound cooldowns, collision
  bookkeeping flags, lighting/self-light fields, and debug/stage-spawn labels.
  `marked_for_destruction` and `stage_spawn_index` are excluded from the network
  fingerprint because they are manager/stagegen bookkeeping, not durable playable
  state.
- Archetype/static for normal play and therefore not repaired every player
  snapshot: pickup eligibility flags, stomp/contact flags, base max speed,
  throw velocity scale, buoyancy, friction flags, draw layer defaults, light
  defaults, callbacks, weights, static vulnerabilities, and recovery policy
  booleans. If debug/admin editing makes one of these live at runtime, it must
  route through the host/admin lane or a broad state patch.

Known playtest symptoms to classify under this gate, not patch ad hoc:

- Client jump predicts immediately, then visibly stutters from coordinator
  correction. This is prediction/repair policy, not a tool-specific bug.
- Client fall damage can feel too early, suggesting fall timer/body repair still
  needs scenario tests rather than local tuning.

## Gate 4: Broad State Lanes

Goal: use broad Terraria-like message lanes, not one packet family per content
item.

Required lanes:

- [x] `PlayerActionRequest`: peer intent to use/interact/pickup/drop/throw/hit.
- [x] `ActionRequestAck`: coordinator acceptance/dedupe response.
- [x] `EntitySpawned`: coordinator-assigned entity creation.
- [x] `EntityStatePatch`: broad entity repair/state patch.
- [x] `EntityLifecycle`: deactivate/destroy/collect.
- [x] `EntityCarry`: held/dropped/thrown links and velocities.
- [x] `TileMutation`: break/change/place foreground/backwall tiles.
- [x] `PlayerStatePatch`: money, tools, effects, wanted, health.
- [x] `PlayerBodySnapshot`: high-frequency player body/control repair.
- [x] `FluidCellPatch`: coordinator-owned fluid cell visible/gameplay repair.
  It is sent as non-blocking state repair; newer patches/refreshes replace
  missed cells, and fluid packets must not create holes in the durable
  coordinator order.
- [x] `StageProgression`: stage sync/transition.
- [x] `PresentationCommand`: cosmetic sound/particles/shake/visual scripts.
- [x] `WorldSnapshot`: complete current-world bootstrap/resync. The snapshot
  stream is implemented as broad ordered lanes for tiles, fluids, entities,
  player state, inactive linked entity cleanup, and run/progression state.
  Packet smoke covers late join, reconnect after world mutation, force resync,
  checksum/fingerprint validation, and every reconnect spawn policy.
- [x] `ContentExtension`: explicitly future-only. Current content is compiled
  C++ content using broad ids/enums/replicated fields. Runtime mod/archetype
  payloads will add this lane when we support runtime-loaded content that cannot
  fit the existing broad fields.

Rules:

- [x] Network code may know ids, enums, replicated field layouts, and broad
  action kinds.
- [x] Network code must not know that a specific item creates a specific gameplay
  outcome unless that outcome is represented by a generic lane.
- [x] Adding a normal weapon/tool/entity should usually require only content code
  and replicated archetype fields, not new packet code.

Current status: Gate 4 is complete for the current compiled-content demo.
Network-layer audit found no concrete item/tool behavior such as teleporter,
bow, jetpack, shop, dice, or weapon-specific outcomes in `src/network`. The
remaining content references are broad ids/enums and replicated field layouts:
`EntityType`, `ToolKind`, `EffectId`, `Tile`, `NetActionKind`, and packet field
schemas. The one content-ish lobby leak was entrance lookup/reset during
network respawn/rejoin; that now lives behind stage progression helpers instead
of direct `Tile::Entrance` / `EntityType::Entrance` checks in the network lobby.
Smoke tests may still use concrete content as fixtures. `EntityStatePatch` is
intentionally one event per packet and currently requires a 576-byte packet cap,
still below normal UDP MTU.

## Gate 5: Mutation Classification

Goal: every gameplay mutation path has an owner and a legal network path.

Every mutation must be classified as exactly one:

- `LocalPresentation`: particles, camera shake, local-only sound, UI, debug draw.
- `PeerRequest`: peer wants a durable mutation and sends generic request.
- `CoordinatorApply`: coordinator/offline path mutates canonical state and emits
  broad result/patch.
- `CoordinatorRepair`: derived or high-frequency state periodically corrected by
  coordinator snapshot/patch.
- `HostAdminOnly`: debug/dev mutation allowed only on host/offline until admin
  lane exists.

Audit checklist:

- [x] Tile break/change/place paths.
- [x] Tile trigger paths: shop vandalism, embedded treasure, water/fluid support,
  rope placement, future traps.
- [x] Entity spawn paths: tools, pots, boxes, chests, arrows, traps, enemies,
  treasure, shop items, sacrifice rewards, stagegen, debug spawns.
- [x] Entity damage/death/deactivate paths.
- [x] Entity contact paths: collect, pickup, stomp, projectile, enemy touch,
  spikes, water/lava, traps.
- [x] Carry/drop/throw/attach paths, including player-carry chains.
- [x] Tool use paths: bombs, ropes, pot tool, teleporter/telepack, jetpack, cape,
  bow, pistol, web cannon, mattock, machete, future tools.
- [x] Inventory/economy paths: money, shop buy, dice win/loss, rewards,
  sacrifices, monkey theft, rescued damsel rewards.
- [x] Player lifecycle: death, game over, respawn, revive, ankh, stage transition,
  reconnect.
- [x] Fluids: paint/debug, sim changes, entity effects, tile changes caused by
  fluid, future lava/water watcher behavior.
- [x] Debug/admin: entity editor, stage load/reroll, fluid brush, sound brush,
  spawn panel, settings sliders.

Current status: core gameplay mutation paths are audited against the canonical
world_ops/action lanes. Peer gameplay requests go through coordinator apply;
coordinator/offline mutation emits broad result/patch messages; high-frequency
body/fluid state is repaired by coordinator snapshots/patches. Debug/admin is
still intentionally separate: peer-side world mutation UI is blocked, host
entity spawn, entity-editor state edits, tool/effect/money edits, tile brush,
fluid brush, and stage load/reroll use existing replicated lanes. Sound brush
and audio/player/fluid mechanics tuning are host-admin debug tooling; peers are
UI-blocked from mutating them until a real client-admin command lane exists.
Pure graphics/UI/camera/performance/post-fx display controls remain local
presentation and are intentionally not part of canonical multiplayer state.

Known playtest symptoms to classify under this gate, not patch ad hoc:

- Mattock peer use is now covered by the generic held-item use smoke path. If it
  regresses in live play, treat it as a bug in the broad held-use/action lane,
  not as a mattock-specific packet.

## Gate 6: Prediction And Reconciliation Polish

Goal: corrections are visible but not game-breaking, and local player control is
never blocked.

- [x] Define which local actions may be predicted visually.
- [x] Define which local actions may not be predicted because wrong prediction is
  too destructive.
- [ ] Local player movement predicts immediately.
- [x] Local held/back item use can show presentation immediately where safe.
- [ ] Canonical spawned entities from peer actions come from coordinator.
- [ ] Peer-predicted local artifacts either reconcile to coordinator net ids or
  are cosmetic-only and deleted.
- [ ] Small body corrections are smoothed.
- [ ] Large body corrections snap.
- [ ] Attachment/held/carry corrections snap links immediately.
- [ ] Death/respawn/stage-transition corrections snap immediately.
- [x] Add debug overlay showing predicted vs coordinator-corrected player body.

Prediction policy:

- Predict local player movement immediately. The owning client should feel like
  offline play; coordinator snapshots repair position, velocity, condition, and
  animation when they differ.
- Keep local-only movement counters that affect feel, such as normal-state fall
  timer accumulation, locally predicted until a canonical health/condition/body
  lifecycle correction arrives. Stale periodic snapshots must not change fall
  damage thresholds on the owning client.
- Fall damage is still a durable damage mutation. Peers may accumulate a local
  fall timer for movement/equipment feel, but only offline/coordinator gameplay
  applies fall damage; peers wait for coordinator damage/repair.
- Predict movement-derived local player presentation immediately: walk/run,
  hang/climb/fall poses, emotes, and facing. Coordinator player snapshots repair
  stale remote-facing presentation.
- Interpolate remote players from coordinator state. Do not let remote player
  replicas author canonical gameplay.
- Allow safe local item presentation: bat windup, cape open/closed pose,
  jetpack flame, weapon use pose, local sound, recoil pose, and camera shake.
  These may be discarded or repaired without changing durable state.
- Continuous attachment presentation is archetype opt-in through
  `predict_attachment_use_presentation`. Peers still send authoritative
  held/back use requests, but opted-in attachments may keep their local
  `UseEntity` state held down so visuals like an open cape do not flicker while
  waiting for coordinator repair.
- Do not predict canonical hitboxes, damage, stun, death, pickups, money,
  inventory counts, shop/dice results, stage transitions, respawns, or exits.
- Do not predict canonical tile mutations. Tile break/place/change comes from
  coordinator result or repair lanes.
- `Debug: Network` now shows each player target beside the local body: position
  and velocity deltas, snap severity, fall/coyote/stun timers, grounded,
  condition, health, and animation frame. The control-server `net` command emits
  the same body/delta diagnostics for live process inspection.
- Do not predict canonical spawned entities. Bombs, ropes, arrows, webs, thrown
  pots, loot, and enemies are coordinator-assigned net ids. Local cosmetic ghosts
  are allowed only if they are clearly non-canonical and deleted on confirmation
  or timeout.
- Snap held/attached/carry link corrections immediately. Smoothing link state
  creates more visible bugs than it hides.
- Snap death, respawn, and stage-transition corrections immediately.
- Keep fluids coordinator/world-repair driven. Render smoothing is local
  presentation; fluid amount/cell changes are canonical world state.

Current status: policy is defined, but implementation is still immature. Recent
player repair work fixed permanent position/fall-timer divergence in some cases,
but movement prediction, safe item presentation, and smoothing/snap thresholds
still need deliberate tests.

## Gate 7: Debug/Admin Lane

Goal: debug tools do not silently desync multiplayer.

- [x] Decide command permissions: host-only for now, optional client admin later.
- [ ] Add generic admin request packet/lane.
- [x] Route entity spawn through coordinator.
- [x] Route entity edit through coordinator or mark host-only.
- [x] Route tool/effect/money edits through coordinator.
- [x] Route stage load/reroll through coordinator.
- [x] Route fluid brush/gravity brush through coordinator.
- [x] Route tile brush/debug tile edits through coordinator.
- [x] Block sound brush and mechanics/audio tuning on peers.
- [x] Keep graphics/UI/display-only toggles local.
- [x] Add smoke test for host admin spawn and stage change.

Current status: client-side admin is deliberately not implemented. Multiplayer
peers are UI-blocked from debug world mutation controls; host/coordinator debug
stage load, entity spawn, entity edits, player money edits, effect edits, and
tool-slot edits have smoke coverage through the existing broad spawn/entity
patch/player patch lanes. Host tile brush changes route through the broad
foreground tile patch lane; host fluid brush changes flow through the coordinator
fluid-cell patch lane. Sound brush and audio/mechanics tuning are host-admin-only
debug controls. Pure graphics/UI/camera/performance/post-fx display controls
remain local. The real client-admin command lane is still future work.

## Gate 8: Headless/Fake-Transport Proof

Goal: every networking feature has a fast test that can fail before playtesting.

Existing:

- [x] `--check-network-protocol-smoke`
- [x] `--check-network-action-smoke`
- [x] `--check-network-packet-smoke`
- [x] `--check-network-frame-smoke`

Required additions:

- [x] Packet loss/reorder/duplicate suite.
- [x] Packet smoke has a deterministic reversed/duplicated coordinator-result
  delivery path for ordered result idempotence.
- [x] Packet smoke drops one coordinator delivery without ack and verifies
  retained durable history resends on the next delivery.
- [x] Packet smoke verifies durable history is retained until all connected
  peers ack, not only the first peer.
- [x] Packet smoke duplicates peer request delivery and verifies the coordinator
  queues exactly one action for the request id.
- [x] Packet smoke drops one peer request delivery and verifies the peer retries
  from its pending request buffer.
- [x] Packet smoke drops an explicit action ack while delivering the accepted
  coordinator result and verifies the result implicitly clears the pending peer
  request.
- [x] Packet smoke duplicates a rejected peer request and verifies the explicit
  action ack clears the pending peer request without a durable result.
- [x] Packet smoke covers broad-lane world snapshot bootstrap for tile/backwall
  changes, tile rotation, fluid state, and a spawned entity after normal ordered
  history is discarded.
  This also guards against fluid snapshot packets blocking later durable ordered
  facts.
- [x] Late join after tile/entity/player/fluid mutations.
- [x] Force world snapshot resync after peer tile/backwall/rotation/fluid/entity/
  player/stage-metadata corruption and acked-history pruning.
- [x] Snapshot fingerprint verification after late join, reconnect-after-world-
  mutation, snapshot bootstrap, and force-resync.
- [x] Disconnect and reconnect same `PlayerId`.
- [x] Disconnect and reconnect fresh player.
- [x] Multiple local players on one peer.
- [x] More than two processes/players.
- [x] Basic movement under latency.
- [x] Fall damage and stun repair under latency.
- [x] Carry/drop/throw players under latency.
- [x] Held player damage/death/respawn.
- [x] Damage while carrying another player severs carry links and converges
  holder stun/health state.
- [x] Environmental death while held severs carry links and converges death/
  respawn state.
- [x] Respawn while held severs carry links and converges body state.
- [x] Stage transition while one player is dead.
- [x] Stage transition converges when the first coordinator stage-sync delivery
  is dropped.
- [x] Stage transition while carrying another player.
- [x] Shop buy from peer.
- [x] Chance shop win/loss/prize roll from peer.
- [x] Box/pot/chest loot roll from peer action.
- [x] Bomb chain reactions.
- [x] Arrow trap/projectile contacts.
- [x] Pushblock motion and projectile attachment.
- [x] Fluid interaction and fluid patch convergence.
- [x] Admin host stage load and entity spawn.

Current status: broad fake-transport smoke coverage exists for core hostile
network conditions: duplicate/reordered coordinator results, dropped coordinator
deliveries, dropped peer requests, dropped explicit action acks, rejected
request acks, late join, reconnect, stage transition loss, latency, and
multi-peer history retention. It is still not exhaustive for every gameplay
scenario, so new durable lanes should add focused smoke cases.

## Implementation Order

Do these in order. Do not jump to polish before convergence and resync are
proved.

1. [ ] Finish player-body audit and add scenario tests for fall/stun/climb/hang/
   carry/death/respawn.
2. [x] Add packet loss/reorder/duplicate fake transport controls and run current
   smoke tests through them.
3. [x] Implement concrete `WorldSnapshot` bootstrap/resync.
4. [x] Implement late join using `WorldSnapshot`.
5. [x] Implement leave/rejoin identity and retained-player reconnect policy.
6. [x] Expand mutation classification audit and fix every peer-local durable
   mutation found.
7. [x] Implement debug/admin command lane or keep all debug mutation host-only
   with explicit UI blocking.
8. [x] Add multi-peer/multi-local-player tests.
9. [ ] Tune prediction smoothing/correction after correctness is proven.

## Commit Rule

Before committing multiplayer work:

- [ ] Build succeeds.
- [ ] `--check-network-protocol-smoke` passes.
- [ ] `--check-network-action-smoke` passes.
- [ ] `--check-network-packet-smoke` passes.
- [ ] `--check-network-frame-smoke` passes.
- [ ] Any new lane or behavior has at least one smoke test.
- [ ] `git diff --check` passes.
- [ ] Update this checklist if the work changes a gate.
