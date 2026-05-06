# Current Multiplayer Cleanup Checklist

Review date: 2026-05-04

Scope: current uncommitted multiplayer/gameplay-event work. Goal is to keep singleplayer behavior clean while making permissive co-op sync less ad hoc.

## Checklist

- [x] Fix network stage transition authority.
  - Peer exit requests should ask the coordinator and then wait for stage sync.
  - Peer-side `ProcessGameplayEvents` should not queue/apply its own quest-stage transition after sending the request.
  - Coordinator/offline transitions should use one path that calls the stage load notification/sync hook when appropriate.
  - Current suspect sites: `src/gameplay_events.cpp`, `src/inputs.cpp`, `src/step.cpp`, `src/network/net_progression.cpp`.

- [x] Stop non-authoritative attack/projectile replicas from mutating non-player entities.
  - Remote-owned bats/projectiles should not locally damage/knock back enemies, blocks, props, or breakables.
  - Durable mutation should come from the source owner through gameplay/network events.
  - Current suspect sites: `src/entities/baseball_bat.cpp`, `src/entities/common/contact_damage.cpp`.

- [x] Preserve correct carried-entity presentation when clearing player hang/climb state.
  - Picking up a player must clear hang/climb/attachment locomotion state.
  - Picking up a dead/stunned bat/snake/etc. should not force its display to neutral/alive.
  - Current suspect site: `src/entities/common/carry.cpp`.

- [x] Split gameplay event storage from network replication conversion.
  - Keep entity/gameplay code emitting gameplay facts.
  - Move `NetEvent` construction out of `gameplay_events.cpp` into a network-owned listener/converter file.
  - Candidate file: `src/network/net_gameplay_replication.cpp`.

- [x] Route tile break replication through the same gameplay-event seam.
  - Done: `stage_break.cpp` emits `GameplayTileBroken`; `network/net_gameplay_replication.cpp` converts it to `TileBroken`.

- [x] Replace content-specific presentation replication with generic presentation commands.
  - Netcode should not know about `Teleporter`, cape, bow, or future modded item behavior.
  - Content code may emit primitive cosmetic commands such as play sound at world position, shake entity, shake area, and spawn a named scripted presentation effect.
  - Network code should only transport/apply those primitive commands after resolving stable net entity ids back to local vids.
  - Durable gameplay remains semantic: entity spawn/hold/drop/throw/damage, tile change, stage transition.
  - Done: replaced `TeleporterUsed` packet/event with generic presentation commands.

- [x] Make peer-spawned shared entity ids coordinator-confirmed.
  - Peers may still allocate provisional ids for immediate local prediction.
  - Coordinator rewrites peer-spawned entities to canonical shared ids before rebroadcast.
  - Peers relink their local predicted entity to the canonical id when the ordered spawn echo returns.
  - Coordinator keeps an alias table so follow-up events using the old provisional id can still resolve.

- [x] Add ordered durable-event ack/retry boundaries.
  - Peers keep pending local durable events until the coordinator echo arrives.
  - Peers ack the highest applied coordinator order.
  - Coordinator resends only ordered events newer than each remote's ack.
  - Coordinator prunes ordered history after every remote has acked it.
  - Transient entity state patches use transient headers and do not consume coordinator order.
  - Peers apply durable events in coordinator order instead of receive order.

- [x] Add first coordinator repair path for shared entities.
  - Coordinator periodically sends lossy state patches for linked non-player entities.
  - Repair patches are outside durable order; newest repair wins.
  - This is not the full late-join/fall-behind repair snapshot yet.

## Verification

- [x] `cmake --build build -j 8`
- [x] `git diff --check`
- [x] `build/splonks-cpp --check-classic-quest-stagegen`
- [ ] Two-window smoke test: host/client movement, carry/throw player, break tiles, use rope, use exit.

## Audit Follow-Ups

Review date: 2026-05-06

Scope: current large multiplayer commit after durable ordering, coordinator repair, presentation commands, and authority cleanup.

- [x] Split `src/network/net_lobby.cpp`.
  - Split out packet send/translation helpers, durable event handlers, player snapshots/interpolation, and entity repair/state patches.
  - Remaining `net_lobby.cpp` is still a high-level session/orchestration file, but no longer owns packet serialization or per-entity repair logic.

- [ ] Decide whether scripted presentation effects need a content registry.
  - `presentation_commands` are generic at the network layer, but `TeleportSplit` and `TeleportMerge` are hardcoded presentation scripts.
  - This is acceptable as content-side code for now, but a small registry/table would scale better if many items start adding cosmetic scripts.

- [ ] Revisit `ProcessGameplayEvents` coupling.
  - It currently calls network replication and progression directly.
  - This is not a current blocker: direct calls are simpler and easier to debug than a generic listener bus.
  - A listener/dispatcher should only be added if more independent systems need to consume the same gameplay facts, or if `ProcessGameplayEvents` starts growing into another ownerless coordinator.
  - Do not convert offline gameplay into event-driven architecture just for symmetry.

- [ ] Rename or reshape `EntityArchetype::step_without_local_authority`.
  - The behavior is useful, but the name is network-shaped inside entity archetypes.
  - Consider a content-owned name such as `remote_presentation_step_mode` or an explicit presentation-step enum if more modes appear.

- [ ] Review playtest/editor state files before commit.
  - `data/settings.cfg` changed debug defaults.
  - `assets/graphics/.span/workspace.span` changed editor workspace state.
  - Keep these only if they are intentional repo state, not local testing noise.

- [ ] Implement the coordinator request/apply API and authority guards from `docs/plans/remote_multiplayer_plan.md`.
  - Peers must not create canonical non-player entities, break canonical tiles, roll canonical loot, or apply canonical shared damage directly.
  - Add assert/log guard rails before more content-specific sync patches.
  - Route peer attempts through generic request categories and let the coordinator emit generic result events.
  - Done first slice: peer tile breaks now request generic `BreakTile`; coordinator applies the break and emits `TileBroken`/spawn results.
  - Done second slice: non-authoritative damage attempts from locally-owned sources now request generic `DamageEntity`; coordinator applies normal content damage.
  - Done third slice: non-authoritative hits now request generic `HitEntity`; coordinator applies damage plus knockback and emits final damage/state results.
  - Done fourth slice: peers no longer originate canonical `EntitySpawned`, `EntityDamaged`, or broad `EntityStatePatched` result events.
  - Done fifth slice: basic throw tools now use generic `UseTool` action requests from peers; the coordinator runs the tool content and emits canonical spawn results.
  - Done sixth slice: canonical spawns/state patches now carry current animation presentation, spawn results carry initial acceleration, and common deactivation paths emit ordered `EntityDeactivated` results.
  - Done seventh slice: peers no longer send legacy tile/entity result packet families or transient entity-state patches to the coordinator; peer-to-coordinator gameplay traffic is action requests plus currently allowed presentation commands.
  - Done eighth slice: removed the old local-event drain path, renamed the queue to pending outbound events, and removed the debug/manual path that moved local events directly into ordered events.
  - Done ninth slice: removed the special `StageExitRequest` packet path; exits now use the generic `InteractEntity` request and coordinator-side exit callback.
  - Done tenth slice: split `EntityDeactivated` out of carry packets into an entity lifecycle packet so carry packets only transport held/drop/throw results.
  - Note: presentation commands remain allowed peer-to-coordinator for cosmetic sound/effect/shake presentation only, including sound positions/settings and scripted visual effects; they must not encode durable gameplay state.
  - Remaining: generic request/result coverage for all special deactivation sites, loot/container interactions, held-entity edge cases, direct entity interactions, and inventory/effect results.

- [ ] Replace remaining peer-authored carry/drop/throw result paths with action requests.
  - `EntityHeld`, `EntityDropped`, and `EntityThrown` remain as coordinator-authored result events.
  - Peer carry/throw input now requests coordinator approval instead of directly authoring shared carry state.
  - Target shape: `PickupEntity`, `DropEntity`, and `ThrowEntity` requests, then coordinator emits canonical carry/drop/throw/state results.
  - Migration list:
    - [x] Add generic `PickupEntity`, `DropEntity`, and `ThrowEntity` action request kinds.
    - [x] Make peer pickup/drop/throw input send requests instead of mutating canonical carry state locally.
    - [x] Make the coordinator apply those requests through the same carry helpers offline gameplay uses.
    - [x] Restrict `EntityHeld`, `EntityDropped`, and `EntityThrown` replication to coordinator-authored result events.
    - [x] Ignore any remaining legacy peer-authored carry result packets at the coordinator/peer boundary.
    - [ ] Smoke test player carry/throw, item carry/throw, forced drops, and throw velocity in both directions.

- [ ] Add generic requests for held-entity use and direct entity interaction.
  - Bat/pistol/bow/web cannon/mattock/machete are held entities, not tool slots.
  - Chests/shops/exits/use prompts need `InteractEntity` or equivalent.
  - Target shape: peer sends intent plus compact aim/velocity payload; coordinator runs content callback and emits generic results/presentation.
  - Held/back use migration:
    - [x] Add generic `UseHeldEntity` and `UseBackEntity` action request kinds.
    - [x] Make peer held/back item input send use-state requests instead of calling `UseEntity` locally.
    - [x] Make the coordinator validate holder/item attachment and apply `UseEntity`/`StopUsingEntity`.
    - [ ] Smoke test bat, bow, web cannon, pistol, mattock, machete, cape, teleporter, and telepack.
  - Direct interaction migration:
    - [x] Add generic `InteractEntity` request kind.
    - [x] Add `EntityArchetype::on_interact` so content owns interaction behavior and netcode only transports source/target ids.
    - [x] Convert shop buy prompts to request/apply `InteractEntity`; coordinator runs the normal buy callback.
    - [x] Convert exits to request/apply `InteractEntity`; the exit callback emits the normal stage transition event.
    - [x] Convert normal chests to request/apply `InteractEntity`; chest loot now emits normal entity-spawn events.
    - [ ] Convert key chest unlock to request/apply or an equivalent coordinator-owned contact action.
    - [ ] Review sacrifice, altar, craps, shop theft, and other area/contact interactions for coordinator-only result events.
    - [ ] Add replication for buyable-state changes and player inventory/money changes; current generic interaction can run the buy on the coordinator, but not every resulting field is represented in durable result events yet.

## Sync Audit Notes

- Tile break sensors should come along automatically when the coordinator is the side calling `BreakStageTilesAtCoords`: the break path emits `TileBroken`, runs tile-trigger callbacks, and emits tile-break spawn results from one place.
- Entity spawns are safe only if the content path emits `EmitEntitySpawnedGameplayEvent` after finalizing the spawned entity's type, position, velocity, acceleration, key state, and animation state. Boxes, pots, stage-break drops, tool throws, arrow traps, boulders, and now chest loot do this. Remaining direct `NewEntity()` sites need individual review before assuming multiplayer parity.
- Entity deactivation/state changes are safe only if they emit a durable result such as damage/state patch/drop/throw/deactivation or are covered by coordinator repair. Common vanish/marked-for-destruction and collected/bought pickup paths now emit `EntityDeactivated`; special one-off `SetInactive` paths still need audit because a periodic repair patch is not the same thing as an ordered gameplay result.
- Buy/pickup effects are not fully represented yet. Money, tool counts, effect lists, and buyable flags need explicit result coverage or a broader player/inventory state patch before shops can be considered synced.

### Remaining Direct Mutation Audit

- `NewEntity()` sites that already emit spawn results: `stage_break.cpp`, `entities/arrow_trap.cpp`, `entities/box.cpp`, `entities/chest.cpp`, `entities/common/throw.cpp`, `entities/giant_tiki_head.cpp`, `entities/player.cpp`, `entities/pot.cpp`.
- `NewEntity()` sites still needing review/result coverage: `entities/bow.cpp`, `entities/barrier_emitter.cpp`, `entities/cobra.cpp`, `entities/gear_items.cpp`, `entities/monkey.cpp`, `entities/sac_altar.cpp`, `entities/shopkeeper.cpp`, `entities/skeleton.cpp`, `entities/spider.cpp`, `entities/web_cannon.cpp`.
- `SetInactive()` sites still needing review/result coverage: special entity lifetimes, chest key consumption, shop buy state beyond purchased pickup deactivation, rope deployment, webball/cobweb replacement, teleporter/player splat, altar sacrifice, and item depletion.
- State fields needing explicit player/inventory replication before shops/items are complete: `money`, tool slot counts/kinds, effect list/counts, buyable flags, held/back slot changes not covered by carry result events.
