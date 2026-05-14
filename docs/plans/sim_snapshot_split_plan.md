# Sim Snapshot Split Plan

Status: implemented.

Goal: split snapshot semantics so rollback and network catchup restore only
deterministic simulation state, while debug playback keeps its broad local
editor/view restore behavior. This removes the current cludge where a peer may
apply a host snapshot containing the host's `controlled_ent_vid`, then repair
local control afterward.

## Problem

`GameplaySnapshot` started as debug playback/rewind infrastructure and became
the shared restore format for rollback, desync recovery, and late-join catchup.
It is too broad for network use.

It currently includes deterministic gameplay state mixed with local process
state:

- `controlled_ent_vid`
- `spectator_target_player_id`
- `PlayerSlot::connection_kind`
- `PlayerSlot::primary_local`
- debug overlay/brush/input state
- local bots/debug overrides
- camera/audio listener state
- particles/audio emitters/presentation state

The network hash does not require these fields to match. That part is correct.
The bug is that network snapshot restore imports them anyway, then patches some
local roles after restore. That is semantically wrong and fragile, especially
with multiple local players per process.

## Target Model

### `SimSnapshot`

Deterministic gameplay state only. Used by:

- rollback save/restore
- hash mismatch rollback repair
- host-current snapshot catchup
- late join barrier catchup
- fake/headless lockstep tests

Allowed fields:

- mode/game-over/run progression that affects simulation
- frame/stage frame/scene frame if used by simulation
- deterministic RNG streams
- quest/run state
- stage, tiles, fluids, triggers, acoustics data that affects simulation
- ents and ent runtime state
- ent tool/inventory state
- player registry durable identity, connected state, player id to ent mapping,
  and deterministic per-player input history needed for simulation
- contact/cooldown bookkeeping
- pending stage transition and multiplayer respawn mode

Disallowed fields:

- `controlled_ent_vid`
- `spectator_target_player_id`
- local/remote ownership classification
- primary local player choice
- debug UI state
- debug brush state
- debug input overrides and random local bots
- camera target, camera position, camera anchor
- audio listener preference
- particles/audio emitters/transient presentation
- world prompts/debug annotations unless they affect gameplay determinism

### `LocalOverlaySnapshot`

Process-local state captured before applying a `SimSnapshot`, then restored
afterward. Used only around rollback/network restore paths.

Fields:

- local player ids owned by this process
- primary local player id
- controlled ent/camera target preference, resolved by player id after restore
- spectator target
- debug input overrides and local bots when running local validation
- local camera/audio listener preference where needed

Important rule: do not store raw `VID`s as the source of truth when a stable
player id can be used. After a sim restore, remap player ids to current `VID`s.

### `DebugPlaybackSnapshot`

Broad local tooling snapshot. Used by:

- debug rewind/scrub
- debug recording save/load
- local offline playback

It may keep local UI, camera, debug brush, particles, audio emitters, and other
presentation/editor state because the goal is "restore what I was seeing."

Debug playback should be treated as offline/local tooling. It should not be the
network catchup format.

## Migration Plan

1. Rename the current broad snapshot concept.
   - Move the current `GameplaySnapshot` API used by debug playback toward
     `DebugPlaybackSnapshot`.
   - Keep old names temporarily only if needed to limit churn, but active
     network/rollback code must stop depending on the broad type.

2. Add `SimSnapshot`.
   - Create a new header/source pair, likely `src/sim_snapshot.hpp/.cpp`.
   - Start by copying only the fields required for deterministic state.
   - Add explicit `MakeSimSnapshot`, `RestoreSimSnapshot`,
     `SerializeSimSnapshotToBytes`, and `DeserializeSimSnapshotFromBytes`.
   - Use existing low-level serializers for ents, stage, players, tools, and
     contact bookkeeping where appropriate.

3. Add `LocalOverlaySnapshot`.
   - Capture local player ids from `PlayerRegistry`.
   - Capture the primary local player id, not only one raw `VID`.
   - Capture spectator target and local debug validation state if tests need it.
   - Restore local/remote slot roles for all local player ids after
     `RestoreSimSnapshot`.
   - Recompute `controlled_ent_vid` from the restored primary local player id.

4. Move rollback to `SimSnapshot`.
   - Change `LockstepRollbackSnapshot` to hold `SimSnapshot`.
   - Keep rollback presentation preservation separate, as it is now.
   - Remove broad snapshot fields from rollback memory accounting.
   - Validate rollback repair and latency smokes.

5. Move network snapshot catchup to `SimSnapshot`.
   - Change join barrier and desync snapshot chunking to serialize
     `SimSnapshot`.
   - Apply `LocalOverlaySnapshot` around peer snapshot restore.
   - Remove the post-restore patch that repairs host-imported
     `controlled_ent_vid`; it should become unnecessary because `SimSnapshot`
     never imports that field.
   - Keep chunk transport and retry logic unchanged.

6. Keep debug playback on the broad snapshot.
   - Debug scrub/recording can still serialize broad local state.
   - Disable or clearly gate debug playback controls during online lockstep if
     needed.
   - Do not use debug playback serialization for network resync.

7. Add hard validation gates.
   - A smoke where host snapshot has host-local `controlled_ent_vid`, peer owns
     player 2, peer applies `SimSnapshot`, and peer still controls player 2.
   - A multi-local smoke where one process owns players 2 and 3; after snapshot
     restore both remain local, only the configured primary is primary, and both
     input streams still queue.
   - A serialization smoke proving `SimSnapshot` roundtrips to the same network
     fingerprint.
   - A negative/static check or grep-based audit that network catchup code does
     not call broad debug playback snapshot serialization.

## Completion Checklist

- [x] Introduce `SimSnapshot` type and serialize/restore functions.
- [x] Introduce `LocalOverlaySnapshot` type and restore helpers.
- [x] Move rollback save/restore ring to `SimSnapshot`.
- [x] Move desync snapshot catchup to `SimSnapshot`.
- [x] Move late-join barrier catchup to `SimSnapshot`.
- [x] Rename or isolate the broad debug snapshot as debug playback only.
- [x] Delete the temporary `RestoreLocalPlayerSlotRoles` controlled-ent repair
      branch once network catchup no longer imports `controlled_ent_vid`.
- [x] Add single-local host/peer snapshot restore regression smoke.
- [x] Add multi-local peer snapshot restore regression smoke.
- [x] Run `cmake --build build --target splonks-cpp -j 8`.
- [x] Run `./build/splonks-cpp --check-det-replay-smoke`.
- [x] Run `./build/splonks-cpp --check-input-lockstep-smoke`.
- [ ] Rerun guided human playtest after the split.

## Non-Goals

- Do not redesign lockstep transport.
- Do not add item/entity-specific networking.
- Do not remove debug playback; keep it local/offline scoped.
- Do not optimize snapshot size before the semantic split is correct.
