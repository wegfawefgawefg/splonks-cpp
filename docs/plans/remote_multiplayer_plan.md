# Remote Multiplayer Plan

Goal: online co-op that keeps the local player feeling offline-responsive, even
across high-latency links. Remote players, enemies, pickups, particles, and even
some world changes may visibly correct or arrive late. Local movement and local
interaction should not wait on the network.

Authoritative implementation checklist:
`docs/multiplayer_terraria_parity_checklist.md`.

This file records architecture rationale and protocol shape. If this file's
historical notes conflict with the checklist, the checklist wins. Do not call the
networking model complete until the gates in that checklist are checked and
covered by tests.

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

Use a Barony-like hybrid: local-predicted player movement plus
coordinator-authoritative shared world state.

- Player count is N-player by design. Four players is the first practical test
  target, not a code assumption.
- A process may own multiple local players. Example: two people on one machine
  and three on another should be represented as five `PlayerId`s in one shared
  session.
- Local multiplayer does not route through transport. Local player slots operate
  directly on the world through the same `PlayerId -> input -> entity` routing
  used by remote players.
- No peer waits for remote input before simulating local control.
- Clients author/predict their local player motion immediately and send movement
  state. The coordinator may validate/correct against solid world state, stage
  transitions, and death/respawn state.
- The coordinator owns shared world outcomes: stable entity ids, stage
  transitions, enemy/world-prop defaults, durable message ordering, and repair
  snapshots.
- Local actions may present immediately on the acting machine, but durable
  shared results must pass through coordinator ordering before they are treated
  as canonical on other machines.
- Local actions must not create canonical shared-world outcomes on peers. A peer
  may show local-only prediction or cosmetics, but the coordinator creates the
  real entity/tile/inventory result.
- Peers apply durable messages idempotently. If an message arrives late but still
  applies cleanly, apply it. If it conflicts, resolve through coordinator order
  and later repair snapshots.
- Cheating and modified clients are treated as a social/lobby compatibility
  problem, not as a reason to make every action server-authoritative.

This is not lockstep, not GGPO-style whole-game rollback, and not pure
client/server input authority. The chosen target is Terraria-style broad
coordinator authority: clients keep local player controls responsive, but shared
world mutations are requests sent to the coordinator, and the coordinator
executes normal content code before broadcasting broad result/snapshot messages.
This keeps content authoring from turning into per-item networking work.

## Hard Authority Rules

These are the multiplayer architecture rules to enforce going forward:

- Local player control must never wait on the network.
- The controlling client predicts and presents its own player immediately.
- Remote player bodies are replicas, not local gameplay actors.
- The coordinator assigns stable net ids for shared entities.
- Non-player runtime spawns are coordinator-owned by default, even when a client
  requested the spawn. A client-thrown bomb, arrow, rope ball, pot, or loose
  item may be predicted locally for presentation, but its canonical shared-world
  physics/damage/tile effects come from the coordinator.
- Peers do not run full gameplay simulation for coordinator-owned non-player
  entities. They may advance presentation/animation and may run explicit
  local-interaction seams such as exit prompts, but AI, physics, damage, tile
  breakage, projectile contact, and impulses are coordinator-authored.
- The coordinator owns canonical shared world state: stage transitions, tiles,
  loose items, enemies, props, entity health/condition, held/attached state, and
  durable inventory/economy changes.
- Clients may optimistically present local action intent, but they must not
  author durable shared results. The durable result is not canonical until the
  coordinator runs the content path and broadcasts an ordered result or repair.
- Peers must not roll canonical loot, spawn canonical non-player entities, break
  canonical tiles, or apply canonical damage to shared non-player entities.
- Durable mutations use stable message categories, not item-specific packet
  families. Adding a new held item should normally require no networking code if
  it mutates state through the existing spawn/state/tile/player result lanes.
- Cosmetic-only presentation uses generic presentation commands and remains
  non-authoritative.
- Packet loss must not permanently fork durable state. Reliable delivery or
  repair snapshots are required for all durable mutations.
- New gameplay content should not need custom networking unless it introduces a
  genuinely new durable category.

## Architecture Boundary

Do not convert the whole game to local message-driven gameplay. Normal offline
gameplay should remain direct: entity code mutates entity/tile state immediately
so controls stay simple and responsive.

Current seam: canonical `world_ops` helpers own durable mutation boundaries:

- Coordinator/offline gameplay code calls `world_ops` while applying normal
  content rules.
- Peer gameplay code sends action requests for shared mutations instead of
  mutating shared state directly.
- `world_ops` queues broad network result/action messages directly when
  networking is active.
- The progression layer handles stage-exit requests/sync.
- Entity code must not construct network packets or know about sockets.
- Networking should not know entity-specific rules like sacrifice, shop
  behavior, weapon hitboxes, or tool internals.
- In multiplayer peer mode, gameplay code should not perform shared-world
  durable mutations directly. It should request the attempted action and let
  the coordinator run the content callback that mutates shared state.

This gives a responsive co-op model during migration without forcing every
behavior through an abstract message bus. If a gameplay fact is not durable or not
useful for remote readability, it should stay local.

Durable mutations should stay behind the canonical `world_ops` API instead of
relying on broad internal message capture. These functions should not use an
`Authoritative` suffix in code; authority is a property of where the function is
called and what the session role is, not part of every symbol name.
`world_ops` is a module, not one fat file: keep entity lifecycle/state, tile
mutation, action requests, presentation commands, and future fluid/player/run
patches in separate implementation files under `src/world_ops/`.
Example shape:

```cpp
world_ops::BreakTile(state, tile_coord, source);
world_ops::SetTile(state, tile_coord, tile_patch, source);
world_ops::SpawnEntity(state, spawn_spec, source);
world_ops::DeactivateEntity(state, target, reason, source);
world_ops::DamageEntity(state, target, damage_spec, source);
world_ops::PatchPlayerState(state, player_id, player_patch, source);
world_ops::PatchRunState(state, run_patch, source);
world_ops::PatchFluidCell(state, cell_coord, fluid_patch, source);
```

## Action Request Payloads

Action requests use broad message categories, but their payload fields must be
named for the action shape. Do not add anonymous integer or float payload slots
to gameplay action requests.

Current action-request payload shape:

- `UseTool` carries `tool_slot` plus throw velocity.
- `UseHeldEntity` and `UseBackEntity` carry `use_edge` (`Press` or `Release`)
  plus aim direction.
- Tile/entity/damage requests use their named tile, target, damage, amount,
  velocity, and flag fields.

Presentation commands are separate from durable gameplay actions, but they still
use named payload fields. Do not add generic visual slots there either. Current
presentation payloads carry explicit fields such as `entity_shake_amount`,
`foreground_shake_amount`, `background_shake_amount`,
`area_entity_shake_amount`, and `shake_radius_tiles`.

These helpers own both sides of a durable mutation:

- mutate the coordinator/offline state through normal content rules;
- enqueue the broad network/result patch when networking is active;
- no-op or apply local-only presentation when the result is cosmetic;
- reject or convert peer-side calls into coordinator action requests.

This target is closer to Terraria's direct message/category style and avoids
turning the game's internal architecture into a listener graph. There is no
internal `GameplayMessage` union/queue. `src/gameplay_messages.hpp` only defines
plain action/result payload structs that `world_ops` and network serialization
share.

Strict migration rules:

- New durable gameplay code should call `world_ops`.
- New peer intent code should call `world_ops::RequestGameplayAction` or a
  narrower `world_ops` request helper.
- Raw entity/tile/player/run/fluid mutation plus a separate replication emit is
  transitional and should be removed category by category.
- Stage generation, debug-stage construction, replay loading, and net-message
  apply may still use raw storage writes because they are not normal live
  gameplay mutation paths.
- When a durable category has a `world_ops` helper, direct call sites in content
  should be migrated to it before new content uses that category.

### No Generic Listener Bus Requirement

Do not add a generic listener/message-bus system. Direct calls are acceptable when
they cross a clear ownership boundary:

- `world_ops` may directly call network replication/progression while it remains
  small and explicit.
- A listener/dispatcher is only justified if many independent systems need the
  same gameplay facts and direct `world_ops` calls become insufficient.
- The important boundary is not "everything listens to messages." The important
  boundary is "peers request shared mutations; coordinator applies shared
  mutations."
- Prefer explicit `world_ops` helper calls over adding more internal message
  fanout.

## Interaction Authority Rule

Durable interactions start as source-predicted input/presentation only; canonical
shared-world effects are coordinator-owned.

- If a local player or locally-owned held/projectile entity hits, carries,
  drops, throws, or otherwise mutates another entity, that local source may
  present intent immediately.
- The source sends an action request to the coordinator. It must not create the
  canonical shared result locally unless this process is the coordinator or
  offline.
- The coordinator orders, confirms, rebroadcasts, or repairs the result.
- This includes remote player targets. Picking up or hitting a remote player may
  be predicted by the holder/attacker, but the coordinator must be able to repair
  held state, damage state, and final velocity.
- Remote-owned projectiles/melee replicas should not independently author damage
  to our local player. Their owner sends the interaction result to the
  coordinator; peers apply the coordinator-ordered message.
- Generic remote damage without a named source should not take over local
  player bodies, because stale enemy overlap can otherwise produce bogus hits.

This rule avoids the worst double-authoring case: both the attacker's machine and
the target's machine deciding they are the source of truth for the same hit.

## Prediction Policy

Prediction is allowed only where a wrong guess can be repaired without forking
gameplay:

- Local player movement predicts immediately on the owning client.
- Normal-state movement counters that affect feel, including fall timer
  accumulation, remain locally predicted until a canonical health/condition/body
  lifecycle correction arrives. Periodic coordinator snapshots must not
  downsample these counters and change gameplay thresholds on the owning client.
- Fall damage remains coordinator-authored durable damage. Peers can keep local
  fall counters for prediction/equipment behavior, but they do not apply fall
  damage locally in multiplayer.
- Local movement presentation predicts immediately: walk/run, hang/climb/fall
  pose, emote pose, facing, and player animation timing.
- Remote players are replicas. They interpolate or snap from coordinator state;
  they do not run canonical local gameplay.
- Safe local item presentation may predict: bat windup, cape open/closed pose,
  jetpack flame, weapon use pose, use sound, recoil pose, and camera shake.
- Attachment presentation prediction is archetype opt-in via
  `predict_attachment_use_presentation`. The peer still requests authoritative
  held/back use edges; the opt-in only keeps local `UseEntity` state down for
  attachments whose presentation/equipment feel is safe to repair.
- Canonical hitboxes, damage, stun, death, pickups, money, inventory, shop/dice
  results, stage transitions, respawns, and exits are not predicted.
- Canonical tile mutations are not predicted. The coordinator applies break,
  place, and tile-change results.
- Canonical spawned entities are not predicted. Bombs, ropes, arrows, webs,
  thrown pots, loot, and enemies are coordinator-spawned with stable net ids. A
  client may show a cosmetic-only ghost if it never participates in gameplay and
  is deleted when the coordinator result arrives or times out.
- Held, attached, and carried links snap to coordinator state.
- Death, respawn, and stage transitions snap to coordinator state.
- Fluids are coordinator/world-repair state. Local render smoothing is
  presentation only.

This is the Terraria-style split we want: clients keep controls responsive, but
world-changing consequences come from coordinator-owned content execution and
broad repair/sync lanes.

## Terraria / Barony Alignment

The target architecture is closer to Terraria and Barony than to deterministic
lockstep:

- Terraria/tModLoader uses stable message categories and server relay rather
  than one packet per item. Examples include projectile sync, NPC sync, tile
  manipulation, tile square repair, and player/item action messages. Content is
  identified by ids and payload, while the server remains the middleman for
  shared state.
- Barony uses direct semantic packets and helper calls such as movement packets,
  item-use/drop packets, entity snapshots, and server update helpers. It is more
  ad hoc than we want, but the authority shape is similar: clients control their
  own player, and shared entities/world state are server/coordinator updated.

For Splonks, avoid `RequestBreakBox`, `RequestUseTeleporter`, or other
content-specific network families. Use generic categories that carry entity type,
archetype id, tool slot, tile coordinate, interaction kind, and compact content
payloads. Coordinator-side content callbacks interpret those ids and produce
generic results.

### Message IDs Versus Internal Messages

Terraria/tModLoader is message-id based at the network protocol boundary. That
means packets are explicit categories such as player controls, NPC sync,
projectile sync, item sync, tile manipulation, tile square repair, world data,
and mod packets. It does not mean Terraria internally routes all gameplay
through a pure message bus.

For Splonks, keep these concerns separate:

- Network messages are transport/protocol categories.
- `world_ops` is our current seam for applying or requesting durable facts from
  normal content code.
- Singleplayer/offline content does not need to be rewritten into a pure
  listener/message-bus architecture.
- Peers should send generic action requests to the coordinator instead of
  authoring durable state locally.
- The coordinator should run normal content callbacks and then publish broad
  result messages.

We intentionally removed the internal gameplay-message queue. The practical
ownership rule is now simpler: content stays direct/offline-friendly by calling
`world_ops`, and `world_ops` is the only live gameplay seam that may enqueue
network results without putting packet code inside entities.

Terraria-style protocol shape:

- `PlayerControls` / player state: compact player input/movement/action state.
- `SyncNPC`, `SyncProjectile`, `SyncItem`: broad full-state sync for moving
  world objects.
- `TileManipulation`, `TileSquare`, section/world data: tile and world repair.
- `ModPacket`: mod-owned extension payloads after mod identity negotiation.
- Mod extra AI/world/player hooks: content can append custom state without
  adding a packet family per item.

Splonks should mirror that shape, not the exact names: broad lanes plus compact
content payload hooks when broad fields are insufficient.

### Remaining Terraria/tModLoader Divergence Removal Plan

The active divergence checklist lives in
`docs/multiplayer_terraria_parity_checklist.md`. Keep this section as a pointer
only so we do not maintain two competing definitions of done.

The high-level blocker categories are:

- transport reliability under loss/reorder/duplicates;
- complete current-world bootstrap/resync for late join and rejoin;
- exhaustive player body repair and scenario tests;
- broad protocol lanes instead of item-specific packet families;
- explicit mutation classification for every durable gameplay path;
- debug/admin mutation routing or host-only blocking;
- headless/fake-transport proof for the full demo checklist.

### Fluid Networking Reference

Terraria has water/lava/honey/shimmer as liquid data stored on tiles. tModLoader
tile data stores liquid amount and liquid type/flags as tile data, so liquid
sync can ride with tile-square/section/world synchronization rather than
requiring entity-like per-liquid packets.

Splonks differs because fluids are a separate overlay grid, not terrain tiles.
The equivalent multiplayer rule is:

- The coordinator owns fluid simulation when fluids affect gameplay.
- Peers do not step canonical fluids locally.
- Peers receive changed-cell fluid patches and occasional repair/refresh data.
- Heavy water stages need counters/profiling before we raise patch frequency.

This is not byte-for-byte Terraria, but it matches the authority split: the
server/coordinator owns mutable liquid/world data, clients render and interact
with replicated state.

### Terraria-Style Rules For Splonks

Use this as the implementation north star:

- Network packets are broad categories: player state, player action requests,
  entity spawn, entity state patch/snapshot, tile mutation, inventory/player
  patch, stage progression, and presentation command.
- Content-specific behavior lives in entity/tool/effect/tile archetype code, not
  in `network/`. The coordinator calls the same content callbacks that
  singleplayer uses.
- `network/` may know ids, enums, payload bytes, and replicated field layouts. It
  must not know that a web cannon makes cobwebs, a teleporter phases entities, or
  a machete has a particular animation reset.
- If an archetype needs custom durable data beyond the broad fields, add an
  optional archetype serializer/deserializer or compact `ContentCommand`
  extension lane. Do not add a one-off packet family named after the item.
- Peers are allowed to predict local player presentation and UI feel. Peers are
  not allowed to roll loot, create canonical projectile entities, break
  canonical tiles, apply canonical damage, or decide canonical carry/attachment
  results.
- Coordinator repair snapshots are expected. The protocol should make wrong
  local prediction cheap to correct rather than trying to make every peer
  simulate every prop perfectly.

### Required Broad Lanes

Before adding more gameplay content, these lanes should exist and be used
consistently:

- `PlayerMoveState`: local player body presentation, animation/control flags,
  health/money/tool/effect summary when needed.
- `PlayerActionRequest`: use tool, use held/back entity, interact, pickup, drop,
  throw, emote, hit tile/entity.
- `EntitySpawned`: coordinator-assigned net id, archetype id, initial transform,
  velocity/acceleration, owner lane, links, counters, animation.
- `EntityStatePatch`: transform, velocity/acceleration, condition, AI state,
  timers/counters, health, collision/projectile flags, attachment links,
  buyable/shop display state, animation id/frame/speed/loop/finished.
- `EntityLifecycle`: deactivate/destroy/splat/collect when the entity should no
  longer exist or should switch to a non-active durable state.
- `TileMutation`: break/change/place tile, rope placement, trigger-driven tile
  changes, tile-break drops.
- `PlayerStatePatch`: health, stun/death, money, tools, effects, wanted state,
  inventory/passive/equipment state.
- `StageProgression`: exit request, accepted transition, stage load, respawn,
  join/leave.
- `PresentationCommand`: cosmetic sound, shake, particles, phase/warp visuals.
  It must never be the only carrier of durable gameplay state.
- `ActionRequestAck`: coordinator acknowledgment for generic peer requests.
  Peers retry requests until this ack clears the request id; coordinator dedupes
  repeated ids and runs content at most once.
- Continuous held/back item use is input-state driven, not item-packet driven.
  Player snapshots synthesize remote button pressed/released edges; the
  coordinator applies `UseEntity`/`StopUsingEntity` to the attached held/back
  entity every frame from that replicated input. Generic `UseHeldEntity` and
  `UseBackEntity` requests remain edge/ack requests for quick taps and loss
  recovery.
- Peer player snapshots are input packets on the coordinator, not body-authority
  packets. The coordinator simulates remote player control, physics, fall
  damage, stun timers, attachments, and collisions from replicated inputs; it
  must not accept peer snapshot position/velocity as canonical body state.
- Coordinator player snapshots sent to peers are canonical body repair packets
  for every player slot, including the receiving peer's own local player. Peers
  may predict their local player, but they must reconcile to the coordinator
  body state instead of letting prediction drift forever.

## Request / Apply API Shape

Use explicit request/apply naming to prevent authority drift.

Requests are generated by peers or local coordinator input. Requests describe
what a player/source attempted:

```cpp
enum class NetActionKind : std::uint16_t {
    UseTool,
    UseHeldEntity,
    PickupEntity,
    DropEntity,
    ThrowEntity,
    InteractEntity,
    HitEntity,
    HitTile,
    BreakTile,
    PlaceTile,
    EnterExit,
};

struct NetActionRequest {
    NetMessageHeader header;
    NetActionKind kind;
    NetEntityId source_entity_id;
    NetEntityId target_entity_id;
    IVec2 tile_pos;
    IVec2 direction;
    Vec2 world_pos;
    Vec2 velocity;
    std::uint16_t tool_slot;
    std::uint16_t content_command;
    std::array<std::uint8_t, 32> content_payload;
};
```

Coordinator apply functions run normal content behavior and emit results:

```cpp
void ApplyActionRequestAsCoordinator(State& state, const NetActionRequest& request);
void ApplyEntitySpawnedResult(State& state, const EntitySpawnedResult& result);
void ApplyEntityStatePatch(State& state, const EntityStatePatch& patch);
void ApplyTileMutationResult(State& state, const TileMutationResult& result);
void ApplyInventoryMutationResult(State& state, const InventoryMutationResult& result);
```

Important rules:

- Offline mode may call `Apply*` directly.
- Coordinator mode may call `Apply*` directly for local coordinator actions.
- Peer mode sends `Request*` and may only do local-only prediction/cosmetics.
- `Apply*` is the only path that performs canonical non-player entity spawns,
  tile changes, loot rolls, inventory changes, and shared damage.
- Result messages are generic and idempotent. They identify content by entity
  type/archetype/tool/effect ids, not by hardcoded item packet families.

## Mutation Prescan

Current code paths that can fork multiplayer state if they run canonically on a
peer:

- Tile mutation: `BreakStageTiles*`, tile break drops, rope tile changes, and
  tile trigger side effects.
- Entity spawn: tool spawns in `entities/common/throw.cpp`, trap/projectile
  spawns, pot/box/chest drops, altar rewards, skeleton skulls, cobra/web shots,
  and tile break drops.
- Entity damage/death: `entities::common::TryDamageEntity` plus death-consumed
  effects such as Meathead/Ankh-style behavior.
- Entity deactivation/destruction: `SetInactive*`, `marked_for_destruction`,
  pickup collection, projectile expiry, rope-ball expiry, altar sacrifice, and
  telefrag/crush cleanup.
- Entity impulse/state patch: `ApplyKnockback`, pushblock movement, projectile
  attachment, carried/held state, and thrown projectile ownership.
- Inventory/economy: tool counts, passive/effect acquisition, money, favor,
  shop buying/theft, and quest flags.

Do not create one packet per content item from this list. Add generic request
categories only when a new durable mutation family is needed, then let the
coordinator run the existing content code and emit generic result messages.

Hard rule: peers must not author canonical money, inventory, tool, effect,
entity, tile, or run-state deltas. Peers send input/action requests only. The
coordinator validates them, runs normal gameplay/content code, and broadcasts
the canonical result. This intentionally follows Terraria's broad message-lane
model: world/tile/tile-entity/NPC-style state is server-owned, while clients
request actions or sync only explicitly client-owned control state.

Current generic request coverage:

- `BreakTile`: peer requests a tile break; coordinator runs `BreakStageTiles*`
  and emits ordered tile/spawn results.
- `DamageEntity`: peer requests damage from a locally-owned source to a
  non-local target; coordinator runs `TryDamageEntity` and emits normal damage
  results.
- `HitEntity`: peer requests damage plus hit impulse/projectile contact metadata
  from a locally-owned source to a non-local target; coordinator runs
  `TryHitEntity`, applies the content-owned knockback, then emits the final
  damage or state result.
- `CollectEntity`: peer requests pickup/collect resolution; coordinator
  validates overlap and collectability, runs the target's normal contact
  callback, then emits player-state and entity-deactivation results.

Likely next request categories:

- `UseEntity` / `UseTool`: source entity/tool asks coordinator to run the same
  content-owned use callback that offline mode uses.
- `DestroyEntity` or `ContainerOpened`: source requests break/open/sacrifice
  resolution for boxes, pots, chests, and other loot containers.

Canonical result categories:

- `EntitySpawnedResult`: coordinator net id, entity type/archetype id, position,
  velocity, initial counters/state, holder/attachment ids.
- `EntityDeactivatedResult`: net id and reason.
- `EntityStatePatch`: net id, position, velocity, condition, health,
  held/attached ids, relevant flags, animation/display state when needed.
- `EntityInteractionResult`: source id, target id, damage/stun/knockback,
  pickup/drop/throw/attach/detach kind.
- `TileMutationResult`: tile coordinate or compact region, new tile/wall/fluid
  data, optional trigger id.
- `InventoryMutationResult`: player id, tool/effect/held/back slot changes.
- `StageProgressionResult`: exit request accepted, transition start, stage load,
  respawn.
- `PresentationCommand`: cosmetic-only sound/shake/particles/scripted effect.

Do not add a new durable packet type for each item. If a future item truly needs
new durable behavior, first ask whether it fits one of these result categories
with a content command id and payload.

## Authority Guard Rails

Add checks before broad multiplayer work continues:

- In peer mode, canonical tile break/change functions must assert/log and refuse
  unless they are applying a coordinator result.
- In peer mode, canonical non-player entity spawn helpers must assert/log and
  refuse unless they are applying a coordinator result or creating a local-only
  predicted presentation entity.
- In peer mode, loot-drop callbacks must not roll canonical drops.
- In peer mode, damage/death/deactivation of coordinator-owned non-player
  entities must be request-only.
- Debug logs should include the authority owner, net id, message id, coordinator
  order, and whether the action was request, apply, prediction, or repair.

Known failure cases that these guards must catch:

- Client breaks a box and rolls a knife locally, but host has no knife.
- Client bomb breaks tiles and rolls embedded treasure locally.
- Client pot breaks and spawns a snake locally.
- Client arrow trap fires or kills/despawns locally without coordinator result.
- Client pushblock/loose item physics creates durable tile/entity differences.

## Current First Pass Status

Implemented:

- Remote player snapshots with interpolation, including animation state.
- Remote player replicas are display-driven only; they do not run local gameplay
  physics/control.
- Coordinator-ordered tile messages for tile breaks, tile changes, and rope tile
  placement.
- Tool-spawned entity messages for things like grenades/ropes/arrows that need to
  exist on other peers.
- Entity damage/death messages that replicate final health/condition and route
  remote deaths through the normal death callback path.
- Damage messages also carry impact state for pos/vel/acc/stun, so player hits can
  knock back and stun across peers instead of only changing health.
- Generic entity state patch messages for moved replicated props such as pushblocks.
- Player bodies use player-derived network entity ids instead of deterministic
  stage ids.
- Entity held/thrown/drop message packets exist for carry ownership, including
  player-carry chains.
- Durable messages now have basic ack/retry boundaries:
  - Peer-authored durable messages stay in a local retry queue until the
    coordinator echoes them.
  - Coordinator-ordered durable messages are resent per remote until that remote
    acks the highest contiguous applied coordinator order.
  - Transient player/entity state patches remain lossy and outside durable
    history.
- The coordinator sends periodic repair state patches for linked non-player
  entities. This repairs shared prop/enemy drift without touching local player
  prediction.

Not yet implemented:

- Full durable repair snapshots for peers that fall too far behind or join after
  old ordered history has been pruned.
- Tool slot/count replication.
- Back-slot replication.
- Durable item pickup/buy messages.

Near-term priority:

- Expand coordinator repair snapshots for shared entities. This is what turns
  transient desync from permanent corruption into a short visual glitch. The
  first pass exists for linked non-player entity state; it still needs a fuller
  stage/entity ownership repair path.
- Convert direct peer-authored durable mutations into action/request/result
  categories owned by the coordinator.

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

## Barony Notes

Local reference clone inspected: `/home/vega/Coding/GameDev/Barony`.

Barony is a useful reference because it feels good over long-distance co-op but
does not appear to use deterministic lockstep.

Observed source shape:

- Transport uses UDP plus a reliable wrapper. `sendPacketSafe` prefixes packets
  with `SAFE`, sequence numbers, and retry/ack bookkeeping; Steam/EOS paths can
  use their own reliable P2P send modes. See `src/net.cpp`.
- Entity replication uses broad snapshot packets. `sendEntityUDP` sends `ENTU`
  with entity uid, sprite, position, rotation, flags, tick, and velocity.
  Clients receive/create/update entities and assign client-side behavior from
  the replicated sprite/model. See `src/net.cpp` and `src/net.hpp`.
- Player movement is not input-only lockstep. Client `PMOV` packets contain
  player position, velocity, yaw, and pitch. The server updates internal player
  movement state, runs collision validation with `clipMove`, and sends a
  corrected `PMOV` if the path hits an obstacle. See `src/net.cpp`.
- Item interactions are semantic requests. Examples include `DROP`, `USEI`, and
  `DCKA`; clients send item details to the server, and the server performs or
  rebroadcasts the durable result. See `src/items.cpp` and `src/net.cpp`.
- Monster/world state is server-owned enough that monster code frequently calls
  `serverUpdateEntitySkill`, `serverUpdateEntityFlag`, and related helpers.
  Monster behavior is generally not client-authored. See `src/actmonster.cpp`.

Conclusion: Barony is closest to "server/coordinator authoritative shared world
plus client-authored player movement with validation/correction." It is not pure
permissive peer mutation, and it is not pure server-simulated input prediction.
This is the best fit for Splonks if we want Japan/Texas/Hawaii play to feel
responsive without turning every future item into a networking whack-a-mole.

## Authority Decision

Do not continue the current fully permissive/ad hoc direction as the final
architecture. It is viable for quick tests, but it is already causing repeated
fixes for animation state, held state, damage, item use, and entity identity.

Do not switch to deterministic lockstep or server-only input authority as the
default either. That would likely reproduce Spelunky-style latency sensitivity,
which is the opposite of the goal.

Target model:

- Local player motion is immediate and locally predicted.
- Remote player bodies are interpolated display replicas plus occasional
  authoritative repair.
- Shared world mutations are coordinator ordered.
- Coordinator owns stable net ids and default ownership for enemies, props,
  loose items, stage transitions, and repair snapshots.
- Clients send action requests or optimistic action results through stable
  gameplay categories, not item-specific network hacks.
- The coordinator confirms, orders, rebroadcasts, or repairs those outcomes.
- Cosmetic-only presentation commands may remain generic and non-authoritative.

This should preserve the good feel of permissive co-op while reducing the
ongoing cost of syncing each new item by hand.

## Stable Message Categories

Prefer category messages over one-off content packet families:

- `PlayerMoveState`: position, velocity, facing, animation/control state, carried
  attachment presentation state.
- `PlayerActionRequest`: use tool, use held entity, use back entity, interact,
  pickup, drop, throw, emote, hit tile/entity.
- `EntitySpawned`: coordinator-assigned net id, archetype id, position, initial
  state, owner lane.
- `EntityStateSnapshot`: position, velocity, acceleration, condition, display
  state, animation id/frame, held/attached state, health, timers/counters,
  relevant effect summary.
- `EntityStatePatch`: small reliable changes such as condition, sprite/display,
  flags, health, inventory count, effect add/remove.
- `EntityInteraction`: damage, stun, knockback, pickup, drop, throw, attach,
  detach, telefrag, crush.
- `TileChanged`: break/change/place tile, rope placement, fluid-affecting tile
  changes.
- `StageProgression`: exit request, stage load, respawn, player join/leave.
- `PresentationCommand`: sound, shake, particle, phase effect, one-shot visual
  command.

For future modded content, prefer a generic extension lane:

- `ContentCommand { archetype_id, command_id, payload }`

The core network layer transports and orders it. The registered content
archetype interprets it. This keeps the network table stable without hardcoding
`Teleporter`, `Cape`, `Bow`, or future modded item names into `net_lobby.cpp`.

## Ownership Lanes

### Player Body

- Owner: controlling client.
- Local simulation: immediate.
- Network: send periodic player-state messages/snapshots.
- Remote peers display the latest received player state with interpolation.
- Coordinator can validate/correct movement against solid world state and hard
  session facts such as death, respawn, and stage transition.
- Normal movement correction should be rare and gentle; do not make local
  controls wait for coordinator approval.

### Held / Carried Items

- Predictor: holder's client while held.
- Canonical owner: coordinator for conflict resolution and repair.
- Pickup is a durable message: `PickupEntity(player, item)`.
- The picker applies pickup immediately and sends the action/result to the
  coordinator.
- Conflicting pickups resolve by coordinator message order. Losing peers repair or
  ignore their local pickup if needed.
- Throw/drop/use should be confirmed through stable action/result categories,
  not item-specific packet families.

### Projectiles / Tools

- Predictor: spawning client.
- Canonical owner: coordinator once the spawn is accepted and assigned a stable
  net id.
- Local actor sees projectile/tool results immediately.
- Send action/result categories:
  - `SpawnProjectile`
  - `ProjectileHitEntity`
  - `ProjectileHitTile`
  - `BreakTile`
  - `KillEntity`
  - `SpawnEntity`
- Remote peers may render the projectile path approximately. Durable results
  come from coordinator-ordered messages and repair snapshots.

### Enemies / World Props

- Default owner: coordinator.
- A client may predict an active interaction with the entity for local
  responsiveness.
- Any client may request or report a durable interaction result: hit, stun, kill,
  pickup, sacrifice, telefrag, push, or break.
- Conflicting messages resolve by ordered message application and idempotent checks.
- Remote peers accept coordinator-ordered outcomes and may be repaired by later
  snapshots. They do not replay exact physics to prove every message.

### Stage Tiles / Fluids / Lighting

- Tile changes are durable ordered results.
- A peer actor may show immediate local-only break/place presentation, but it
  must not permanently modify canonical tile state until the coordinator result
  arrives.
- Coordinator orders tile changes, rebroadcasts if necessary, and repairs
  conflicts.
- Fluids, lighting, particles, and audio are locally simulated from current
  durable state. They do not need exact cross-peer parity.

## Message Design

Every durable gameplay result should be an idempotent message:

- `message_id`
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

Messages must be safe to receive twice and safe to receive out of order within a
small window. This matters more than exact deterministic simulation.

## Required Durable Messages

Start small, but design the envelope so adding more message types is mechanical.

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
- `EntityHeld`
- `EntityDropped`
- `EntityThrown`
- `EntityDamaged`

World:

- `TileChanged`
- `TileBroken`
- Deployed ropes are represented as normal `TileChanged` results.

Inventory/economy/quest:

- `PlayerStatePatched` for player health, wanted state, money, tools, and effect list
- `RunStatePatched` for small run-level state such as favor and reward tiers
- `PresentationCommand`

Avoid adding action-specific net messages for content such as bombs, ropes,
projectiles, crates, chests, shops, or sacrifices. The coordinator should run
the normal content callback and emit generic entity, tile, player-state, and
run-state results.

Local-only messages should not be networked: particles, short-lived audio, camera
shake, debug annotations, and cosmetic lighting flicker. If a cosmetic effect is
important for remote readability, send the durable cause and let peers spawn the
effect locally.

Content-specific cosmetic messages should not get their own packet families. Use
generic presentation commands for readable one-shots such as sounds, entity
shake, area shake, and named scripted presentation effects.

## Network Identity

Local `VID` values are not enough across machines. Use stable player/session ids:

- `NetEntityId`: stable for replicated entities during a stage.
- `PlayerId`: stable gameplay player identity. This is not network-owned; local
  multiplayer and remote multiplayer both use it.
- `NetMessageId`: monotonic per message source.
- `StageInstanceId`: identifies a generated stage instance and seed.

Each peer maps `NetEntityId -> local VID`. The coordinator assigns canonical ids
for shared entities. Clients may use provisional prediction ids for immediate
local presentation, but those ids must be replaced or confirmed by the
coordinator before the entity is canonical. Local-only effects, particles, and
annotations do not need network ids.

## Required Data Structures

Minimal first pass:

```cpp
using NetEntityId = std::uint64_t;
using NetMessageId = std::uint64_t;
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

struct NetMessageHeader {
    NetMessageId message_id;
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
about sockets or peers. Entity-owned logic should eventually call canonical
`world_ops` helpers for durable mutation. Entity-owned logic should not emit a
local gameplay message and wait for a listener; it should either mutate purely
local presentation state or call the appropriate `world_ops` helper/request.

## Required Code Boundaries

Networking should live in its own module, for example:

- `src/network/net_ids.hpp`
- `src/network/net_message.hpp`
- `src/network/net_session.hpp`
- `src/network/net_transport.hpp`
- `src/network/net_fuzzer.hpp`
- `src/network/net_replication.hpp`
- `src/network/net_debug_ui.hpp`

Gameplay systems should expose durable-message hooks without importing transport:

- entity spawn/deactivate
- damage/death/stun
- pickup/drop/throw
- tool inventory changes
- tile break/change
- stage transition
- shop/favor/run-state mutation

Avoid putting entity-specific networking cases in engine code. If `SacAltar`
changes favor or reward tier, it emits a generic `RunStatePatched` result. The
network layer serializes the state patch; it does not know sacrifice rules.

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

### Durable Message Reliability

Current implementation:

- `NetMessage` durable payloads are coordinator-ordered and idempotent once
  received.
- Peer local durable messages retry until the coordinator echoes them back.
- Coordinator-ordered durable messages retry per remote until that remote acks
  the highest contiguous `coordinator_order` it has applied.
- The coordinator prunes ordered durable history only after every connected
  remote has acked it.
- The coordinator periodically sends lossy repair state patches for linked,
  non-player entities. These are intentionally not durable ordered messages; the
  newest repair wins.
- Transient player snapshots and entity state patches intentionally remain
  unreliable and should never be retried; newer snapshots replace older ones.

Remaining durable reliability requirements:

- Resend unacked durable messages on an interval with a cap/backoff so packet loss
  cannot create unbounded traffic.
- If a peer falls too far behind the retained durable history, send a repair
  snapshot or force a stage reload instead of replaying an unbounded log.
- Optionally add sparse gap acks later if we need to skip over permanently
  missing messages without forcing repair.

Durable message classes that must use ack/retry:

- Tile breaks/changes, including deployed rope tiles.
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
3. Soft-correct local optimistic world overlays when ordered durable messages differ.
4. Hard-correct local player only for impossible state, death, transition, or
   unrecoverable stage mismatch.

The local player should never wait for:

- walking/running/jumping/climbing/hanging
- using held/back items
- throwing
- shooting
- placing bombs/ropes
- teleporter preview and activation

Ordered durable messages may later correct consequences.

## Remote Movement Before Durable World Messages

Basic remote movement comes before replicated tile/entity mutations. Durable
messages prove shared-world correctness, but they are hard to evaluate if remote
players still look like packet-snapped puppets.

Remote player movement path:

- Add interpolation state keyed by `PlayerId`.
- On snapshot receive, store target position, velocity, facing, grounded state,
  and input flags instead of writing entity position directly from the packet.
- Player snapshots must not carry authoritative gameplay semantics such as
  condition, stun/death state, hang/climb state, or animation ids. Those come
  from coordinator-owned state patches/presentation, not peer motion packets.
- Each frame, move remote player entities toward their newest target after local
  simulation.
- Snap only when the correction exceeds a debug-tunable distance threshold.
- Debug controls should expose snapshot send interval, interpolation strength,
  interpolation delay, and snap distance.
- Test with two or three separate processes, including multiple local players
  owned by one process.

After movement is readable, add the first request/result world mutation.
`BreakTile` is the preferred first target because it proves "one player changed
the shared world and every process saw it" without item ownership complexity:

1. Peer attempts to break a canonical tile.
2. Peer does not apply the canonical tile mutation or roll drops locally.
3. Peer sends generic `ActionRequest{BreakTile}` to the coordinator.
4. Coordinator applies normal stage-break gameplay if still relevant.
5. Coordinator emits generic `TileBroken` and any spawned loot/entity results.
6. Peers apply coordinator results idempotently. Already-air/already-broken
   tile is a no-op.
7. Debug message log shows requested, received, applied, duplicate, and no-op
   action/result messages.

Then do `EntityDamaged` plus deactivation/state results, then
pickup/drop/throw ownership.

## Conflict Resolution

Because clients may predict or report results before coordinator confirmation,
conflicts must be boring and deterministic.

Default priority:

1. lower `coordinator_order`
2. lower `message_id` from the same source
3. lower `source_player_id` as final tie-break

Examples:

- Two players pick up the same item: first ordered `EntityHeld` wins. The loser
  drops/clears their local optimistic hold.
- Two players kill the same enemy: first ordered `EntityDamaged`/`EntityDeactivated`
  result wins. Later damage/deactivation messages against inactive/dead entities
  become no-ops.
- Two bombs break the same tile: first `TileBroken` changes it. Later duplicate
  breaks are no-ops but can still spawn local cosmetics if desired.
- One player buys an item while another steals it: message order decides whether
  the buy's generic player/entity state patches or theft/disturbance patches
  apply first.

This needs idempotent apply functions. Most bugs in this model will come from
messages that assume the old state still exists.

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

1. Define networking ids and message envelope.
   - Add `PlayerId`, `NetEntityId`, `NetMessageId`, `StageInstanceId`.
   - Add local mapping between `NetEntityId` and `VID`.
   - Add coordinator-order field, even before real networking exists.
2. Add a narrow transitional durable-fact seam.
   - Gameplay messages are acceptable as a migration aid, but they are not the
     target internal architecture.
   - Do not build a broad in-process listener bus.
3. Convert key durable systems to canonical `world_ops` helpers.
   - Start with spawn/deactivate, pickup/drop/throw, tile break/change,
     tool inventory changes, damage/death, fluid patches, run-state patches, and
     stage transition.
   - Helpers mutate coordinator/offline state and enqueue broad results.
   - Peer-side calls become action requests rather than local canonical
     mutation.
4. Support multiple player ids locally.
   - Spawn multiple player entities in one `State`.
   - Route input by `PlayerId`.
   - This gives local multiplayer and exercises message ownership.
5. Add loopback coordinator/peer mode in one process.
   - Same code path as network mode, but transport is an in-memory queue.
   - Add network fuzzer here, before UDP.
6. Replicate remote player snapshots/messages over loopback.
   - Local player remains instant.
   - Remote players are just message/snapshot driven.
7. Add reliable message stream transport.
   - UDP-based transport eventually, but loopback first.
   - Reliable ordered stream for durable messages.
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

- [x] Added foundational network ids/message/session types.
  - Files: `src/network/net_ids.hpp`, `src/network/net_message.hpp`,
    `src/network/net_session.hpp`, `src/network/net_session.cpp`.
  - `State` now owns `network::NetSessionState net_session`, initialized in
    offline mode.
- [x] Added the gameplay player registry spine.
  - Files: `src/player_id.hpp`, `src/player_registry.hpp`,
    `src/player_registry.cpp`.
  - `State` owns `PlayerRegistry players`; gameplay must not use a global
    player VID. Primary-local needs are resolved through player-query helpers.
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
- [x] Added net message queues and first apply path.
  - Files: `src/network/net_message_apply.hpp`,
    `src/network/net_message_apply.cpp`, `src/debug/playback_ui_network.cpp`.
  - The `Debug: Network` window can manually apply queued ordered messages.
  - The old manual local-to-ordered drain path was removed. Peers keep pending
    outbound action/presentation messages; the coordinator owns ordered result
    messages.
- [x] Add in-process durable message apply point in the gameplay step.
  - The playing tick applies coordinator-ordered messages once per frame.
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
    loading, and remote player motion snapshots. Reliable durable-message
    transport, object/tool ownership, local multi-input routing, and repair
    snapshots are still separate follow-up work.
- [x] Added remote player interpolation targets for UDP snapshots.
  - Snapshot receives now update a target per remote `PlayerId`; the remote
    entity moves toward that target after normal local simulation.
  - `Debug: Network` exposes snapshot interval, interpolation strength, snap
    distance, and the current remote target list.
- [x] Removed peer-authored semantic state from UDP player snapshots.
  - Player snapshots now carry only movement/input hints. Condition, stun/death,
    hang/climb state, and animation state are intentionally excluded so peer
    packets cannot overwrite coordinator-authored gameplay results.
- [x] Added first UDP request/result world mutation: `BreakTile` -> `TileBroken`.
  - Peer-side stage tile breaking emits generic `ActionRequest{BreakTile}`
    instead of mutating canonical tiles or rolling loot locally.
  - Coordinator-side stage tile breaking emits generic `TileBroken` result
    messages after normal gameplay break logic runs.
  - Peers apply ordered `TileBroken` results without re-emitting network messages
    or duplicating drop spawns.
  - Peer `BreakTile` requests retry until a matching `TileBroken` result is
    received for that tile.
- [x] Added first generic shared damage request: `DamageEntity`.
  - Non-authoritative damage attempts from locally-owned sources emit
    `ActionRequest{DamageEntity}` instead of directly applying canonical damage
    to remote/coordinator-owned targets.
  - The coordinator applies the request through normal `TryDamageEntity`, so
    on-damage/on-death callbacks and death-consumed effects still run in the
    content-owned damage path.
  - Current limitation: damage requests do not yet carry a full authoritative
    knockback/impulse spec. Existing ordered damage messages can carry final
    pos/vel once the coordinator applies them, but source-specific hit impulse
    should be folded into the next `HitEntity`/damage request shape rather than
    patched per weapon.
- [x] Added generic shared hit request: `HitEntity`.
  - `HitEntity` carries damage type/amount, knockback velocity, clear flags,
    thrown immunity, and projectile-contact metadata.
  - Coordinator-side apply uses `TryHitEntity`, so damage and impulse are
    ordered together and the replicated damage/state result includes the final
    post-hit velocity.
  - Main routed paths now include bat hits, stomp victim hits, body/projectile
    contact hits, machete/mattock strikes, pistol hits, arrow entity hits, and
    explosion push hits.
- [x] Added repo-local two-process multiplayer launcher and read-only live CLI.
  - `scripts/run_multiplayer_pair_i3.sh` builds, opens workspace 2 on
    `DisplayPort-0`, launches the top instance as host and the bottom instance
    as joiner, and passes debug-control ports.
  - `scripts/splonksctl` queries live state through the localhost-only
    debug-control server.
- [ ] Convert first gameplay systems to emit/apply durable messages.
- [x] Add reliable-ish coordinator-ordered `TileBroken` as the first durable UDP
  world mutation.
- [x] Add multiple local player ids/entities.
- [x] Add ordered entity carry messages.
  - `EntityHeld`, `EntityDropped`, and `EntityThrown` have packet encoding,
    coordinator relay, and apply paths.
  - Player-carry chains use the normal entity carry references; attachment sync
    runs multiple passes so `player0 -> player1 -> player2` resolves
    deterministically.
- [x] Add ack/retry reliable delivery boundaries for durable messages.
  - Peer local durable messages retry until coordinator echo/ack.
  - Coordinator ordered durable messages retry per peer until ack.
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
- One tile can be broken by mattock/bomb as a durable message.
- One enemy can be killed by either player as a durable message.
- Stage repair snapshot can fix an intentionally injected mismatch.

If this slice feels bad under `150 ms / 25 ms jitter / 1% loss`, the model needs
adjustment before adding shops, sacrifices, fluids, or full quest progression.

## Debugging Requirements

- Network graph: ping, jitter, packet loss, input age, snapshot age.
- Per-entity network owner/coordinator overlay.
- Message log filtered by player/entity/message id.
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

## Headless Scenario Smoke Tests

Add a deterministic multiplayer scenario harness before more feature work. The
goal is to catch desync regressions in CI/local CLI, not to replace manual feel
tests.

Preferred shape:

- Run one coordinator and N peers in-process using a fake transport and no
  renderer/audio.
- If in-process is too invasive at first, use real headless processes driven by
  the existing debug-control server, then migrate to fake transport later.
- Each scenario sets a stage, seeds RNG, spawns local/remote players, applies
  scripted inputs or admin actions, steps fixed ticks, and compares canonical
  state.
- Compare structured state hashes/diffs for tiles, fluids, entity net ids,
  entity transforms/state/links/health/animation, player inventory/effects,
  run/quest state, and stage metadata.
- Failure output should name the first divergent lane and object, not just
  print "hash mismatch."

Initial scenarios:

- Host/client move without shared mutation.
- Client breaks tile and receives break drops.
- Host and client throw bombs/ropes near the same area.
- Box/chest/pot loot rolls match on every peer.
- Pickup/drop/throw item and player, including forced drop on damage/death.
- Push block plus arrow/projectile attachment.
- Bat/machete/mattock/pistol/bow/web cannon use from peer and host.
- Shop buy, shop theft/disturbance, craps/chance buy.
- Stage exit from host and peer, with and without a carried player.
- Death/respawn while holding or being held.
- Fluid paint/debug/admin command once the admin lane exists.
- Fuzzer presets for LAN, cross-country, and Japan-to-Texas latency/loss.

This is a normal way to test netcode. The important part is making the test
harness compare authoritative durable state, not presentation-only particles or
sounds.

Strict first implementation steps:

1. Add a canonical state fingerprint/diff utility that can run on one `State`.
   It should ignore particles, audio emitters, UI windows, and other local
   presentation state.
2. Add one CLI smoke command that builds a headless `State` and proves the
   fingerprint is stable before networking is involved.
3. Add a fake transport or process-driven harness that starts one coordinator
   and one peer, steps fixed ticks, and compares fingerprints after ordered
   network queues drain.
4. Add scripted scenario helpers only through player inputs, debug/admin
   requests, or `world_ops`; do not mutate peer state directly from the test.
5. Expand to N peers after the one-coordinator/one-peer path catches tile,
   spawn, damage, carry, and stage-transition regressions.

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
- message ack age
- snapshot age
- local prediction age
- predicted-vs-coordinator body deltas, including snap threshold state, timers,
  condition, grounded state, health, and animation frame
- correction count by severity
- hard resync count

Useful presets:

- LAN: `5 ms`, `1 ms jitter`, `0% loss`
- Same region: `35 ms`, `8 ms jitter`, `0.2% loss`
- US cross-country: `80 ms`, `15 ms jitter`, `0.5% loss`
- Japan to Texas: `150 ms`, `25 ms jitter`, `1% loss`
- Bad Wi-Fi: `90 ms`, `60 ms jitter`, `3% loss`, burst loss enabled

This tool should sit below gameplay replication so every channel goes through it.
If only rendering or high-level messages are fuzzed, the test will lie.

## Open Questions

- Coordinator model: listen-server player host first; dedicated coordinator can
  be a later variant.
- Conflict policy for player-vs-player damage if we ever allow it.
- How aggressive should repair snapshots be before they become visually annoying?
- How much state should repair snapshots include before we split `State` and
  `PlayState` properly?

## Recommendation

Build toward canonical `world_ops` mutation plus broad Terraria-style network
lanes. The coordinator owns durable shared outcomes; clients keep local player
control responsive and may present local prediction, but canonical tile, entity,
fluid, inventory, run, and stage state comes from coordinator results or repair
snapshots. Gameplay messages are transitional glue, not the final internal
architecture.
The old internal gameplay-message queue has since been removed; do not reintroduce
it as transitional glue.
