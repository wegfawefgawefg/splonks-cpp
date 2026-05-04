# Current Multiplayer Cleanup Checklist

Review date: 2026-05-04

Scope: current uncommitted multiplayer/gameplay-event work. Goal is to keep singleplayer behavior clean while making permissive co-op sync less ad hoc.

## Checklist

- [ ] Fix network stage transition authority.
  - Peer exit requests should ask the coordinator and then wait for stage sync.
  - Peer-side `ProcessGameplayEvents` should not queue/apply its own quest-stage transition after sending the request.
  - Coordinator/offline transitions should use one path that calls the stage load notification/sync hook when appropriate.
  - Current suspect sites: `src/gameplay_events.cpp`, `src/inputs.cpp`, `src/step.cpp`, `src/network/net_progression.cpp`.

- [ ] Stop non-authoritative attack/projectile replicas from mutating non-player entities.
  - Remote-owned bats/projectiles should not locally damage/knock back enemies, blocks, props, or breakables.
  - Durable mutation should come from the source owner through gameplay/network events.
  - Current suspect sites: `src/entities/baseball_bat.cpp`, `src/entities/common/contact_damage.cpp`.

- [ ] Preserve correct carried-entity presentation when clearing player hang/climb state.
  - Picking up a player must clear hang/climb/attachment locomotion state.
  - Picking up a dead/stunned bat/snake/etc. should not force its display to neutral/alive.
  - Current suspect site: `src/entities/common/carry.cpp`.

- [ ] Split gameplay event storage from network replication conversion.
  - Keep entity/gameplay code emitting gameplay facts.
  - Move `NetEvent` construction out of `gameplay_events.cpp` into a network-owned listener/converter file.
  - Candidate file: `src/network/net_gameplay_replication.cpp`.

- [ ] Route tile break replication through the same gameplay-event seam.
  - `stage_break.cpp` still directly constructs `TileBroken` network events.
  - Prefer a `GameplayTileBroken` event so stage/tile code does not depend on network packet types.

## Verification

- [ ] `cmake --build build -j 8`
- [ ] `git diff --check`
- [ ] `build/splonks-cpp --check-classic-quest-stagegen`
- [ ] Two-window smoke test: host/client movement, carry/throw player, break tiles, use rope, use exit.
