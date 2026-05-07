# Current Multiplayer Cleanup Checklist

Review date: 2026-05-04

Scope: current uncommitted multiplayer/world-ops work. Goal is to keep singleplayer behavior clean while making permissive co-op sync less ad hoc.

## Checklist

- [ ] Add a gated network debug/admin command lane.
  - Raw debug editor actions currently are not guaranteed to sync unless they route through normal gameplay requests/results.
  - Desired commands: spawn entity, patch entity state, set stage/quest/debug stage, paint fluids, brush sounds/presentation, and other test-only world mutations.
  - Shape should be coordinator-routed and permission-gated later: any client may request a debug command, the coordinator validates/applies it, then normal generic spawn/state/tile/fluid/stage result replication carries the actual mutation.
  - Do not let debug UI directly mutate shared world state in peer mode unless it is explicitly local-only presentation.
  - Guardrail in place: multiplayer peers cannot directly debug-spawn entities, swap/edit entities, edit entity tools/effects, reroll/load stages, paint fluids, or edit fluid/player mechanics tuning. This prevents test-only desync while the real admin command lane is still missing.

- [x] Fix network stage transition authority.
  - Peer exit requests should ask the coordinator and then wait for stage sync.
  - Peer-side exit handling should not queue/apply its own quest-stage transition after sending the request.
  - Coordinator/offline transitions should use one path that calls the stage load notification/sync hook when appropriate.
  - Current suspect sites were resolved or moved behind `world_ops` / stage progression.

- [x] Stop non-authoritative attack/projectile replicas from mutating non-player entities.
  - Remote-owned bats/projectiles should not locally damage/knock back enemies, blocks, props, or breakables.
  - Durable mutation should come from the source owner through gameplay/network events.
  - Current suspect sites: `src/entities/baseball_bat.cpp`, `src/entities/common/contact_damage.cpp`.

- [x] Preserve correct carried-entity presentation when clearing player hang/climb state.
  - Picking up a player must clear hang/climb/attachment locomotion state.
  - Picking up a dead/stunned bat/snake/etc. should not force its display to neutral/alive.
  - Current suspect site: `src/entities/common/carry.cpp`.

- [x] Remove gameplay event storage from network replication conversion.
  - Superseded by removing the internal gameplay-event queue entirely.
  - `NetEvent` construction now lives in network-owned broad-lane replication code.
  - Payload/action structs live in `src/gameplay_messages.hpp`.

- [x] Route tile break replication through the same `world_ops` seam.
  - Superseded by `world_ops`: `stage_break.cpp` commits tile-broken facts through `world_ops`, which queues the broad network result.

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

- [x] Remove `ProcessGameplayEvents` coupling.
  - Removed the internal `GameplayEvent` union/queue and deleted `src/gameplay_events.cpp`.
  - Direct calls through `world_ops` are simpler and easier to debug than a generic listener bus.
  - Do not convert offline gameplay into event-driven architecture just for symmetry.
  - `src/gameplay_messages.hpp` now only holds plain action/result payload structs shared by `world_ops` and network serialization.

- [x] Rename or reshape `EntityArchetype::step_without_local_authority`.
  - The behavior is useful, but the name is network-shaped inside entity archetypes.
  - Consider a content-owned name such as `remote_presentation_step_mode` or an explicit presentation-step enum if more modes appear.
  - Done: renamed the flag to `step_as_replica`; behavior is unchanged.

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
  - Done eleventh slice: removed stale per-field inventory/effect/money net events; player health/money/tools/effects now use `PlayerStatePatched`.
  - Done twelfth slice: entity state patches now carry `counter_a`/`counter_b`, and bow fire emits canonical arrow spawn plus bow state patch instead of relying on local-only ammo/spawn state.
  - Done thirteenth slice: web cannon now emits canonical webball/cobweb spawns, webball/cobweb deactivations, and weapon counter state patches through generic result events.
  - Done fourteenth slice: monkey robbery now emits canonical stolen gold spawn and player tool-state patch when it steals/casts a tool.
  - Done fifteenth slice: cobra spit now emits canonical projectile spawn and deactivation results.
  - Done sixteenth slice: barrier emitters now emit canonical beam segment spawn/deactivation results instead of local-only beam children.
  - Done seventeenth slice: skeleton skull drops, giant spider loot drops, sac altar rewards/punishments, sacrifice deactivation, altar-piece deactivation, and shopkeeper starting pistol now emit generic spawn/deactivate/player-state/held results.
  - Done eighteenth slice: direct deactivations for chest keys, rope deployment, arrow-on-kill cleanup, boulders, idols, telefrags, temporary bat swings, damsel rescue, fleshguy splat, machete corpse carving, and explosion-on-death now emit generic deactivation/player-state results.
  - Done nineteenth slice: coordinator-authored damage/state/presentation results are no longer filtered out just because their source is a remote player or remote-held item.
  - Reviewed `gear_items.cpp`: open parachute is currently a non-colliding presentation helper owned by the equipped entity, not durable gameplay. Do not promote it to a canonical net entity unless we decide remote presentation cannot be driven from replicated player/effect state.
  - Note: presentation commands remain allowed peer-to-coordinator for cosmetic sound/effect/shake presentation only, including sound positions/settings and scripted visual effects; they must not encode durable gameplay state.
  - Remaining: generic request/result coverage for all special deactivation sites, loot/container interactions, held-entity edge cases, direct entity interactions, and inventory/effect results.

- [ ] Align remaining multiplayer implementation with the Terraria-style broad-lane model.
  - Broad lanes only: player action requests, player move/state patches, entity spawn/state/lifecycle, tile mutation, player/inventory/effect patch, stage progression, and cosmetic presentation.
  - No new item-named packet families such as `UseTeleporter`, `BreakBox`, `MacheteReset`, or `WebCannonFired`.
  - Coordinator/offline content code may emit durable facts after normal mutation. Peers must send action requests and wait for coordinator-authored durable results/repair.
  - Hard rule: peers must not author canonical money, inventory, tool, effect, entity, tile, or run-state deltas. If a peer wants a durable mutation, it sends a generic request and the coordinator validates/applies it.
  - Network code may serialize ids, replicated fields, and compact archetype payloads. It must not encode content rules for specific weapons/items.
  - Done: added generic `ActionRequestAck` packets so peer action requests retry until the coordinator acknowledges the request id instead of being fire-and-forget UDP.
  - Finish by deleting or disabling any old peer-authored durable result path that bypasses coordinator apply.
  - Add a short smoke checklist for every broad lane after changes: tile break/drop, box/chest loot, thrown bomb, rope deploy, bat/machete swing, pistol/web/bow shot, pickup/drop/throw, back equip/use, shop buy, exit transition.

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
    - [x] Make peer held/back item input send edge requests instead of calling `UseEntity` locally.
    - [x] Synthesize remote input pressed/released edges from player snapshots so coordinator-side remote slots have normal button state.
    - [x] Make the coordinator validate holder/item attachment and apply continuous `UseEntity`/`StopUsingEntity` from replicated input state.
    - [x] Keep reliable edge requests plus `ActionRequestAck` for quick taps and packet loss recovery.
    - [ ] Smoke test bat, bow, web cannon, pistol, mattock, machete, cape, teleporter, and telepack.
  - Direct interaction migration:
    - [x] Add generic `InteractEntity` request kind.
    - [x] Add `EntityArchetype::on_interact` so content owns interaction behavior and netcode only transports source/target ids.
    - [x] Convert shop buy prompts to request/apply `InteractEntity`; coordinator runs the normal buy callback.
    - [x] Convert exits to request/apply `InteractEntity`; the exit callback emits the normal stage transition event.
    - [x] Convert craps/chance table betting to request/apply `InteractEntity`; coordinator runs the normal dice callback.
    - [x] Convert money/gem/inventory pickup contact to generic `CollectEntity`; coordinator validates overlap and runs the normal pickup contact callback.
    - [x] Convert normal chests to request/apply `InteractEntity`; chest loot now emits normal entity-spawn events.
    - [x] Move death/respawn carry cleanup into shared content-owned `SeverEntityCarryLinksForReset`; coordinator respawn now clears every connected player slot and broadcasts canonical state patches.
    - [x] Convert key chest unlock to request/apply `InteractEntity`; the held key is the interaction source, the coordinator runs unlock logic, and chest open state/key deactivation/Udjat spawn are generic results.
    - [x] Add archetype `replica_logic` for client-side prompt/request generation without running non-authority physics or durable step progression.
    - [x] Allow chest/key-chest replica logic only for client-side request generation; the actual open/unlock mutation remains coordinator-owned.
    - [x] Split craps replica logic so peers can show prompts/request bets but only the coordinator rolls dice, locks/unlocks prizes, pays money, and advances table state.
    - [x] Gate area-listener callbacks by gameplay authority so shop area disturbance cannot be authored by non-coordinator peers.
    - [ ] Review sacrifice, altar, craps, shop theft, and other area/contact interactions for coordinator-only result events.
    - [ ] Add replication for buyable-state changes and player inventory/money changes; current generic interaction can run the buy on the coordinator, but not every resulting field is represented in durable result events yet.
      - [x] Added generic `PlayerStatePatched` durable result carrying player health, wanted state, money, tool slots, and effect list.
      - [x] Emit player state patches from common money/gem/idol/craps/monkey money mutations, inventory/effect pickups, shop spending, and tool-slot use.
      - [x] Add buyable-state patching for purchased/shop entities so price/purchased state does not rely only on deactivation.
      - [x] Add generic `RunStatePatched` durable result for run-level favor/reward-tier state.
      - [x] Replicate shop disturbance through generic player/entity state patches instead of a shop-specific net message.
      - [x] Route back-slot equip/takeoff through generic coordinator action requests and include attachment links in entity state patches.
      - [x] Updated coordinator repair state patches to carry the full entity patch payload, including links, counters, AI/wanted state, attachment mode, and buyable display state.
      - [x] Fixed transient entity state patches skipped by local apply so they are pruned instead of resending forever.
      - [x] Entity-state apply now distinguishes an explicitly empty link from an unresolved replicated id, so out-of-order transient repairs do not clear holder/back/buyable links.

## Sync Audit Notes

- Tile break sensors should come along automatically when the coordinator is the side calling `BreakStageTilesAtCoords`: the break path runs tile-trigger callbacks and commits the tile-broken result through `world_ops`.
- Entity spawns are safe only if the content path calls `world_ops::SpawnEntity`/`SpawnConfiguredEntity` after finalizing the spawned entity's type, position, velocity, acceleration, key state, and animation state. Boxes, pots, stage-break drops, tool throws, arrow traps, boulders, chest loot, bow arrows, web balls, cobwebs, monkey-stolen gold, cobra spit, beam segments, loose skull drops, giant spider loot, sac altar rewards/punishments, and shopkeeper pistols do this. Remaining direct `NewEntity()` sites need individual review before assuming multiplayer parity.
- Entity deactivation/state changes are safe only if they emit a durable result such as damage/state patch/drop/throw/deactivation or are covered by coordinator repair. Common vanish/marked-for-destruction and collected/bought pickup paths now emit `EntityDeactivated`; special one-off `SetInactive` paths still need audit because a periodic repair patch is not the same thing as an ordered gameplay result.
- Buy/pickup/rescue effects now have broad player-side coverage through `PlayerStatePatched`: health, wanted state, money, tool counts/cooldowns/kinds, and effect lists are coordinator-authored results. Buyable display flags and shop disturbance/wanted state are carried by generic entity/player state patches.

### Remaining Direct Mutation Audit

- Live content spawn sites now route through `world_ops::SpawnEntity` or `world_ops::SpawnConfiguredEntity`; raw spawn emits are no longer used outside `world_ops`.
- Live content action request sites now route through `world_ops::RequestGameplayAction`; raw action request emits are no longer used outside `world_ops`.
- `NewEntity()` sites still needing review/result coverage: none in content gameplay paths. Remaining raw `NewEntity()` calls are stage/debug initialization, net apply, or reviewed presentation-only parachute visuals.
- `SetInactive()` sites still needing review/result coverage: remaining special entity lifetimes are either covered by generic deactivation events or reviewed as presentation-only.
- State fields still needing explicit replication before shops/items are complete: none known in the player/shop/sacrifice/back-slot path; continue discovering through multiplayer playtests.

## API Coverage Audit

Review date: 2026-05-07

Goal: prove every durable mutation has one of these classifications before we chase interpolation or cosmetic polish:

- Coordinator request lane: peers ask for intent, coordinator validates and applies.
- Coordinator result lane: coordinator/offline content mutates state and emits ordered generic facts.
- Repair/snapshot lane: high-frequency or derived state is periodically corrected by coordinator snapshots.
- Stage-load only: generated once and replaced wholesale on stage load/transition.
- Local-only presentation: cosmetic and allowed to diverge without affecting gameplay.

### Covered Broad Lanes

- [x] Player action intent has a generic request lane.
  - Covered actions: `UseTool`, `PickupEntity`, `DropEntity`, `ThrowEntity`, `UseHeldEntity`, `UseBackEntity`, `PutHeldEntityOnBack`, `TakeOffBackEntity`, `InteractEntity`, `CollectEntity`, `PushEntity`, `BreakTile`, `DamageEntity`, and `HitEntity`.
  - Keep future content inside these lanes where possible before adding any new action kind.

- [x] Entity lifecycle and physics state have generic result/repair lanes.
  - Covered results: `EntitySpawned`, `EntityDeactivated`, `EntityHeld`, `EntityDropped`, `EntityThrown`, `EntityDamaged`, and `EntityStatePatched`.
  - `EntityStatePatched` currently carries the fields needed for player carry, shop/buyable display, AI/wanted, links, counters, position, velocity, acceleration, condition, collision flags, and animation state.

- [x] Player inventory/effect/money state has a generic result lane.
  - `PlayerStatePatched` carries health, money, wanted state, tool slots, and the effect list.
  - Current count limits are fixed in the network event schema: two tool slots and twelve effects.

- [x] Basic run-level quest/altar state has a generic result lane.
  - `RunStatePatched` carries sacrifice favor/reward-tier state plus current classic quest flags used by key chest/Udjat/progression.
  - Add new typed fields here when a quest adds new durable runtime state.

- [x] Tile breaking and rope placement have coordinator result lanes.
  - `TileBroken` covers normal break flow, tile triggers, and break-spawn fallout when the coordinator owns the break.
  - `RopeTilePlaced` covers deployed rope tiles.

- [x] Presentation has a generic cosmetic lane.
  - `PresentationCommand` can transport sound, shake, and scripted visual presentation.
  - This lane must stay non-durable: no money, entity, tile, inventory, effect, or quest state should be encoded here.

### Missing Or Incomplete API Lanes

- [x] Add a generic tile patch lane.
  - `TileChanged` now exists as a `world_ops`/network result carrying tile, rotation, and foreground/backwall layer.
  - This covers world rotation, debug tile edits, non-break tile transforms, one-way/rotated tile edits, and future content that sets tiles without going through `BreakStageTilesAtCoords`.

- [x] Add a fluid state lane or explicitly classify fluids as coordinator-local until implemented.
  - `Stage` owns fluid grids: amount, display amount, velocity, gravity, gravity strength, temporary gravity, and fluid tile type.
  - `StepStageFluids` mutates these every simulation tick, and debug brushes can mutate fluid state directly.
  - Do not send per-cell reliable events every frame by default.
  - Candidate shape: coordinator-owned fluid sim plus periodic lossy fluid snapshots/patches for visible state, with debug/admin commands for painting fluid and gravity.
  - If fluids affect movement/damage/physics in multiplayer, they cannot remain unsynced local simulation.
  - Current classification: coordinator-owned fluid sim with lossy visible/gameplay cell patches to peers.
  - Required payload coverage: tile type, amount, velocity, permanent gravity, gravity strength, and temporary gravity if temp gravity becomes gameplay-relevant.
  - Done: `FluidCellPatched` carries tile type, amount, velocity, permanent gravity, gravity strength, and temporary gravity; peers apply patches and do not locally step the fluid sim.
  - Clear patches repeat briefly so a dropped "cell became empty" packet is unlikely to leave stale remote water.
  - tModLoader/Terraria reference: Terraria stores liquid amount/type as tile data, so its water sync rides with tile/world data paths. Splonks fluids are an overlay grid, so our equivalent is coordinator-owned fluid simulation plus changed-cell/repair patches.

- [x] Add a quest/run-state patch lane beyond altar state.
  - `RunStatePatched` now includes `quest_id` plus current classic flags: Black Market made, Udjat made/held, Moai made, Hedjet held, Sceptre held, and Book of the Dead held.
  - Runtime Udjat pickup now emits this patch instead of relying on the next stage load to converge quest state.

- [x] Classify and extend the main mutable `Entity` repair payload.
  - `EntityStatePatched` now includes b/c/d links, point b/c/d, counter c/d, threshold a/b, draw layer, and a packed runtime flag mask for render/pickup/impassable/hang/stomp/push/contact capability flags.
  - Still local/static by classification: callbacks, labels, inside/child vectors, stage-spawn indices, most sound/asset fields, water/light tuning unless promoted later, and transient per-frame flags.
  - Entity state packets now send one state event per packet to stay under the 512-byte packet budget.

- [ ] Add a debug/admin command lane.
  - Debug spawn, entity editor patches, stage selection, fluid brush, sound brush, and future live inspection tools should be coordinator-routed when multiplayer is active.
  - This can stay permission-gated/test-only, but the path should use the same generic spawn/state/tile/fluid/stage results as gameplay.
  - Guardrail in place: peer-side shared-world debug mutation is disabled instead of being allowed to fork state.
  - Current scope decision: host/admin-only is enough for now. Client-requested admin commands are optional later.

- [x] Audit mechanics settings and live tuning sliders.
  - Player tuning, fluid settings, water effect values, lighting settings, and other debug sliders can change simulation behavior.
  - Decide per setting: local visual/debug only, stage-load config, or coordinator-synced mechanics setting.
  - If a slider changes physics or damage during multiplayer, peers should not apply it independently without a coordinator/admin lane.
  - Guardrail in place: peer-side player tuning and fluid/water-effect tuning windows are read-only/disabled for mechanics mutation.
  - Decision: these are dev tuning tools for establishing good defaults, not live multiplayer gameplay features. Keep them host/offline-only for now.
  - Details: `docs/network_stage_and_settings_classification.md`.

- [ ] Audit presentation coverage separately from durable sync.
  - Many content paths play sounds or spawn particles locally after a durable mutation.
  - Missing cosmetic replication is acceptable short-term, but important feedback such as explosions, teleports, death, buying, and weapon fire should emit generic presentation commands from the authority path.
  - Do not block authoritative gameplay cleanup on perfect cosmetic parity.

- [x] Add an explicit stage metadata classification.
  - Stage tiles/backwalls/fluid grids/background stamps/lights/embedded treasures/triggers/borders/wrap settings are currently a mix of generated state and runtime state.
  - Decide what is stage-load only, what can mutate at runtime, and what needs patch/snapshot coverage.
  - Embedded treasure is probably safe when tile breaking is coordinator-owned, but debug edits and non-break tile transforms need an explicit lane before assuming parity.
  - Done: `docs/network_stage_and_settings_classification.md`.

- [ ] Add headless multiplayer scenario tests.
  - Target: one coordinator plus N peers with no renderer/audio, preferably in-process through a fake transport.
  - First fallback is acceptable: spawn real headless processes and drive them through the existing debug-control server.
  - Each scenario should set a stage/seed, apply scripted inputs/actions, step fixed ticks, then compare canonical state.
  - Required comparison lanes: tiles, fluids, entity net ids, entity transforms/state/links/health/animation, player inventory/effects, run/quest state, and stage metadata.
  - Initial scenarios should cover tile break/drop, bomb/rope, box/chest/pot loot, carry/drop/throw, pushblock/projectile attachment, held weapons, shop buy/theft, exit transition, death/respawn while held, and fuzzer presets.
  - Failure output should be a structured diff, not only a hash mismatch.
  - First slice done: `--check-state-equality-smoke` loads two independent seeded states, compares canonical fingerprints, applies deterministic `world_ops` tile/entity mutations to both, and compares again.
  - The canonical fingerprint intentionally ignores `VID.version`; versions are allocator stale-reference guards, not durable gameplay state, and can legitimately differ across independently loaded states or peers.
  - Second slice done: `--check-network-protocol-smoke` has a coordinator emit real network result records for tile change, rope tile placement, entity spawn, entity state patch, and entity deactivation; a peer applies them through `ApplyOrderedEvents`; fingerprints are compared after each lane.

- [ ] Document and enforce message-category ownership.
  - Terraria/tModLoader is message-id based at the network boundary, not pure internal gameplay-event driven.
  - Splonks should keep direct content callbacks for offline/coordinator gameplay and route durable boundaries through `world_ops`.
  - Network code should stay broad-lane/category based; entity/tool/tile content should not construct packets.
  - Do not add item-named packet families unless the gameplay introduces a genuinely new durable category.

- [x] Migrate durable mutations to canonical `world_ops`.
  - Target helpers: tile break/patch, entity spawn/deactivate/state patch, damage/hit, carry/drop/throw, player state patch, run/quest state patch, and fluid cell patch.
  - Each helper should mutate coordinator/offline state and enqueue the corresponding broad result lane when networking is active.
  - Peer-side calls should send generic action/admin requests or be rejected by authority guards.
  - This is the replacement path for relying on internal event capture everywhere.
  - Do not use `Authoritative` in every symbol name; keep authority in docs/guards and use normal canonical gameplay names in code.
  - Implemented as a module under `src/world_ops/` rather than a single fat file.
  - Current slices: action requests, entity spawn/deactivate/state/damage/carry, presentation commands, foreground tile patch, rope placement, and tile-broken commit.
  - Remaining future expansion: real first-class `BreakTile`, `DamageEntity`, and fluid helpers instead of some existing content-owned implementations calling commit helpers internally.

### Immediate API Work Order

- [x] Implement generic tile patch events before adding more tile-changing content.
- [ ] Implement debug/admin command routing for spawn/entity patch/stage load/tile paint/fluid paint.
- [x] Classify mutable `Entity` fields and either extend `EntityStatePatched` or mark fields as archetype/local-only.
- [x] Decide fluid multiplayer model before using water/lava as required gameplay in networked stages.
- [x] Expand run/quest state patching before adding more quest flags beyond altar and stage progression.
- [ ] Add headless scenario harness and canonical state diff before more multiplayer feature work.
  - First local equality smoke exists as `build/splonks-cpp --check-state-equality-smoke`.
  - First protocol apply smoke exists as `build/splonks-cpp --check-network-protocol-smoke`.
  - Remaining: fake transport or process-driven coordinator/peer scenario that compares fingerprints after queues drain from action requests, not only direct coordinator result records.
- [x] Replace broad durable-fact capture with canonical `world_ops` helpers.
- [x] Extend `--check-state-fingerprint-smoke` from single-state stability to two-state equality once stage/init nondeterminism is audited.
  - Added `--check-state-equality-smoke`.
