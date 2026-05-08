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

- [ ] Assign stable `PlayerId`s per connected local player.
- [ ] Support multiple local players per process.
- [ ] Support peer disconnect without destroying the run.
- [ ] Support reconnect to the same `PlayerId` when identity/session token
  matches.
- [ ] Support "resume previous state" on reconnect: health, money, tools,
  effects, held/back items where valid, wanted state, and position policy.
- [ ] Support "fresh spawn" on reconnect: spawn at entrance or safe revival point
  with default character loadout.
- [ ] Decide how long disconnected player state is retained.
- [ ] Decide what happens to a disconnected player's body: remains inert,
  becomes ghost/dead, is removed, or is reserved offstage.
- [ ] Add UI/debug controls for reconnect policy during development.

## Gate 1: Transport And Reliability

Goal: durable coordinator messages behave like reliable ordered facts even though
the transport is UDP.

- [ ] Every peer request has a stable request id.
- [ ] Coordinator dedupes repeated requests and applies content at most once.
- [ ] Coordinator replies with `ActionRequestAck` for accepted/rejected requests.
- [ ] Every durable coordinator result has coordinator order.
- [ ] Peers apply durable coordinator results in order.
- [ ] Peers ack highest contiguous applied coordinator order.
- [ ] Coordinator retains durable history until every connected peer has acked it.
- [ ] Coordinator resends unacked durable history.
- [ ] Coordinator has a bounded fallback when a peer is too far behind: send full
  world snapshot or force stage resync.
- [ ] Packet-loss/reorder/duplicate fake-transport tests cover requests, results,
  acks, reconnect, and stage transitions.

Current status: partially implemented. We have request ids, acks, coordinator
order, and smoke tests for happy paths. We still need packet-loss/reorder tests
and full fallback resync.

## Gate 2: Concrete World Bootstrap And Resync

Goal: joining/rejoining peers receive current world state, not just a seed and a
hope that history replay is enough.

- [ ] Define `WorldSnapshot` packet/stream format.
- [ ] Include stage identity: quest id, quest stage id, seed, stage instance id,
  frame/stage frame, wrap flags, gravity/settings needed for gameplay.
- [ ] Include foreground tile layer.
- [ ] Include backwall tile layer.
- [ ] Include tile rotations/metadata needed by gameplay.
- [ ] Include stage annotations/triggers only if they are needed after generation;
  otherwise regenerate coordinator-only metadata deterministically.
- [ ] Include fluid grid: type, amount, velocity/gravity fields needed for current
  sim/render/gameplay.
- [ ] Include all active shared entities with stable net id, type/archetype,
  transform, velocity/acceleration, links, counters, timers, health, condition,
  AI state, flags, animation state, buyable/shop state, effects, and runtime
  replicated flags.
- [ ] Include inactive-but-durable state where relevant, or make inactive entities
  absent by definition.
- [ ] Include every player slot: connected/disconnected, local/remote identity,
  entity link, health, money, tools, effects, held/back links, wanted state,
  condition, body state, and respawn/death state.
- [ ] Include run/progression state: level number, quest progression flags, boss
  state, exits, rescued/dead players, and pending transition state.
- [ ] Chunk large snapshots so they fit transport packet limits.
- [ ] Add checksum/fingerprint after snapshot apply.
- [ ] Add host command to force-resync one peer.
- [ ] Add fake-transport tests for late join and rejoin after world mutation.

Current status: not implemented as a full concrete snapshot. We have stage sync,
ordered events, entity state patches, player snapshots, fluid patches, and repair
patches, but not a single complete join/rejoin world bootstrap.

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
- [ ] Player repair includes every remaining body field that can affect gameplay,
  or those fields are explicitly classified as local presentation.
- [ ] Add smoothing/reconciliation policy for small corrections.
- [ ] Add hard snap policy for large corrections.
- [ ] Add tests for walk/run/jump, fall damage, stun recovery, climbing, hanging,
  swimming, carrying/being carried, dying, respawning, and stage transition.
- [ ] Add latency/loss tests so correction does not permanently trap/stun/kill
  the local player.

Current status: much improved. This gate is close, but not complete until every
body-affecting field is audited and tests cover the listed scenarios.

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
- [x] `StageProgression`: stage sync/transition.
- [x] `PresentationCommand`: cosmetic sound/particles/shake/visual scripts.
- [ ] `WorldSnapshot`: complete current-world bootstrap/resync.
- [ ] `ContentExtension`: generic archetype/mod payload lane for future content
  that cannot fit broad fields.

Rules:

- [ ] Network code may know ids, enums, replicated field layouts, and broad
  action kinds.
- [ ] Network code must not know that a specific item creates a specific gameplay
  outcome unless that outcome is represented by a generic lane.
- [ ] Adding a normal weapon/tool/entity should usually require only content code
  and replicated archetype fields, not new packet code.

Current status: broad lanes mostly exist. Missing pieces are full world snapshot,
future content extension, and audit that content does not sneak in item-specific
network behavior.

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

- [ ] Tile break/change/place paths.
- [ ] Tile trigger paths: shop vandalism, embedded treasure, water/fluid support,
  rope placement, future traps.
- [ ] Entity spawn paths: tools, pots, boxes, chests, arrows, traps, enemies,
  treasure, shop items, sacrifice rewards, stagegen, debug spawns.
- [ ] Entity damage/death/deactivate paths.
- [ ] Entity contact paths: collect, pickup, stomp, projectile, enemy touch,
  spikes, water/lava, traps.
- [ ] Carry/drop/throw/attach paths, including player-carry chains.
- [ ] Tool use paths: bombs, ropes, pot tool, teleporter/telepack, jetpack, cape,
  bow, pistol, web cannon, mattock, machete, future tools.
- [ ] Inventory/economy paths: money, shop buy, dice win/loss, rewards,
  sacrifices, monkey theft, rescued damsel rewards.
- [ ] Player lifecycle: death, game over, respawn, revive, ankh, stage transition,
  reconnect.
- [ ] Fluids: paint/debug, sim changes, entity effects, tile changes caused by
  fluid, future lava/water watcher behavior.
- [ ] Debug/admin: entity editor, stage load/reroll, fluid brush, sound brush,
  spawn panel, settings sliders.

Current status: partially audited through playtesting and smoke tests. Not
complete.

## Gate 6: Prediction And Reconciliation Polish

Goal: corrections are visible but not game-breaking, and local player control is
never blocked.

- [ ] Define which local actions may be predicted visually.
- [ ] Define which local actions may not be predicted because wrong prediction is
  too destructive.
- [ ] Local player movement predicts immediately.
- [ ] Local held/back item use can show presentation immediately where safe.
- [ ] Canonical spawned entities from peer actions come from coordinator.
- [ ] Peer-predicted local artifacts either reconcile to coordinator net ids or
  are cosmetic-only and deleted.
- [ ] Small body corrections are smoothed.
- [ ] Large body corrections snap.
- [ ] Attachment/held/carry corrections snap links immediately.
- [ ] Death/respawn/stage-transition corrections snap immediately.
- [ ] Add debug overlay showing predicted vs coordinator-corrected player body.

Current status: functional but immature. Recent player repair work fixed
permanent position/fall-timer divergence, but smoothing and prediction policy
still need deliberate tests.

## Gate 7: Debug/Admin Lane

Goal: debug tools do not silently desync multiplayer.

- [ ] Decide command permissions: host-only for now, optional client admin later.
- [ ] Add generic admin request packet/lane.
- [ ] Route entity spawn through coordinator.
- [ ] Route entity edit through coordinator or mark host-only.
- [ ] Route tool/effect/money edits through coordinator.
- [ ] Route stage load/reroll through coordinator.
- [ ] Route fluid brush/gravity brush through coordinator.
- [ ] Route tile brush/debug tile edits through coordinator.
- [ ] Keep graphics/audio/UI-only toggles local.
- [ ] Add smoke test for host admin spawn and stage change.

Current status: peers are guarded from many debug mutations, but the real admin
command lane is not implemented.

## Gate 8: Headless/Fake-Transport Proof

Goal: every networking feature has a fast test that can fail before playtesting.

Existing:

- [x] `--check-network-protocol-smoke`
- [x] `--check-network-action-smoke`
- [x] `--check-network-packet-smoke`
- [x] `--check-network-frame-smoke`

Required additions:

- [ ] Packet loss/reorder/duplicate suite.
- [ ] Late join after tile/entity/player/fluid mutations.
- [ ] Disconnect and reconnect same `PlayerId`.
- [ ] Disconnect and reconnect fresh player.
- [ ] Multiple local players on one peer.
- [ ] More than two processes/players.
- [ ] Basic movement under latency.
- [ ] Fall damage and stun repair under latency.
- [ ] Carry/drop/throw players under latency.
- [ ] Held player damage/death/respawn.
- [ ] Stage transition while one player is dead.
- [ ] Stage transition while carrying another player.
- [ ] Shop buy from peer.
- [ ] Chance shop win/loss from peer.
- [ ] Box/pot/chest loot roll from peer action.
- [ ] Bomb chain reactions.
- [ ] Arrow trap/projectile contacts.
- [ ] Pushblock motion and projectile attachment.
- [ ] Fluid interaction and fluid patch convergence.
- [ ] Admin host stage load and entity spawn.

Current status: good first smoke coverage exists, but it is not exhaustive and
does not yet test hostile network conditions.

## Implementation Order

Do these in order. Do not jump to polish before convergence and resync are
proved.

1. [ ] Finish player-body audit and add scenario tests for fall/stun/climb/hang/
   carry/death/respawn.
2. [ ] Add packet loss/reorder/duplicate fake transport controls and run current
   smoke tests through them.
3. [ ] Implement concrete `WorldSnapshot` bootstrap/resync.
4. [ ] Implement late join using `WorldSnapshot`.
5. [ ] Implement leave/rejoin identity and policy.
6. [ ] Expand mutation classification audit and fix every peer-local durable
   mutation found.
7. [ ] Implement debug/admin command lane or keep all debug mutation host-only
   with explicit UI blocking.
8. [ ] Add multi-peer/multi-local-player tests.
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

