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

- [ ] Split `src/network/net_lobby.cpp`.
  - It is currently around 2500 lines, which violates the repo's preferred 300-500 line target.
  - Split by responsibility: lobby/session setup, packet send helpers, coordinator packet handlers, peer packet handlers, player snapshots, entity repair/state patches, and debug/status helpers.

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
