# Input Lockstep / Rollback Multiplayer Plan

Status: active experiment plan on branch `net-lockstep-experiment`.

Short entry point: `docs/plans/remote_multiplayer_plan.md`.
Legacy coordinator-authoritative documents live under
`docs/legacy_authoritative_networking/` and are reference-only.

This plan replaces the current coordinator-authoritative mutation-replication
plan as the next architecture experiment. The goal is to test whether a
deterministic input-driven model removes the networking tax from gameplay
content and gives us a cleaner long-term mod story.

## Current Goal

Implement authoritative input lockstep cleanly, without rollback at first.
Rollback remains the next phase after delay-based lockstep proves deterministic.

Success target for the current branch:

- Boot host and client from the multiplayer pair launcher.
- Connect them into one lockstep session.
- Support any number of local players per process in the architecture, even if
  the first live test uses one local player per process.
- Let the client carry the host player.
- Let both players transition through stages together.
- Preserve single-player behavior.
- Remove old coordinator-authoritative mutation cruft from active gameplay.
- Keep entity/content code local-only in language and behavior: no item-specific
  netcode, no authority branches, no request/result gameplay seams.

Operational rules while pursuing this:

- Work phase-by-phase from this checklist.
- Prefer deleting old mutation-replication machinery over adapting it.
- Add fake/headless lockstep tests before trusting live UDP.
- Commit after each meaningful, validated chunk so long solo sessions remain
  recoverable.
- During long unattended runs, make periodic commits after completed
  sub-checkboxes or stable milestones instead of leaving hours of work in one
  fragile working tree.
- Record any known gap in this document before committing.
- Keep single-player behavior as the baseline. Lockstep work must not require
  gameplay/content systems to know whether they are local or networked.

Current handoff constraints:

- Continue from this branch. Do not reset to an older commit just to escape the
  old networking cleanup; the player-slot/input-table refactors are part of the
  lockstep foundation.
- Delay-based input lockstep is the deliverable for this goal. Rollback is
  explicitly out of scope until lockstep determinism and live stage traversal
  are proven.
- Treat old coordinator-authoritative leftovers as cleanup debt. Remove them
  from active gameplay or quarantine them as legacy tests/tools; do not adapt
  new gameplay code to those paths.
- Completion requires live validation, not just CLI smokes: two windows connect,
  the peer can carry the host player, and both players can transition stages
  together.
- Live lockstep should show no active ordered mutation backlog from the old
  replication lanes.

## Current Worktree Handoff

Last known branch: `net-lockstep-experiment`.

Recent validated commits on this branch:

- `1082eda Remove stale authority damage seams`
- `bc8500d Remove stale tile and damage replication hooks`
- `5d26df7 Simulate all player slots during lockstep`
- `d517157 Harden lockstep smoke topology`
- `cc34b6b Remove obsolete replication shims`
- `b895730 Document current lockstep solo checklist`
- `9add7d6 Add lockstep carry transition smoke`
- `37b2e03 Remove stale replica entity cruft`
- `62c34bb Apply stage transitions through lockstep`
- `d6fb737 Bypass old message drain during lockstep`
- `a9b2d55 Cover stepped lockstep stage transition`
- `12bc110 Stop draining old action queue in gameplay`
- `c3e1188 Make live lobby step lockstep only`
- `00453cb Cover lockstep player drop and throw`
- `e929791 Rename control diagnostics to input owner`

Recent smoke-hardening work intentionally made the fake input-lockstep
smoke so it resembles live topology instead of accidentally simulating both
players as primary/local on both fake peers:

- `src/cli_state_smoke.cpp`
  - Adds live-like fake peer ownership: peer 0 owns player 1 and sees player 2
    as remote; peer 1 owns player 2 and sees player 1 as remote.
  - Enables `state.net_session.input_lockstep_enabled` in the fake peer states.
  - Expands first-difference diagnostics for entities, `drng`,
    `stage.tile_change_generation`, and player input fields.
- `src/state_fingerprint.cpp`
  - Removes process-local player registry fields (`connection_kind` and
    `primary_local`) from gameplay determinism fingerprinting.
- `src/entity/manager.cpp`
  - Hardens `EntityManager::SetInactive` against out-of-range ids and duplicate
    deactivation.

Resolved finding:

The stricter topology exposed a real process-local gameplay branch:

```text
state equality smoke input lockstep initial ok: stage=classic_mines_1 frame=0 stage_frame=0 entities=54 tiles=40x32 hash=15016652501064990246
input lockstep smoke clean hash mismatch at frame 342
  peer0 hash=9342928562898737137
  peer1 hash=358909475661019486
  first simple diff: entity 29 identity differs: active 1/1 type 13/13 vid 29:2/29:1
```

The root cause was `ShouldEnterGameOver`: fake lockstep states were still
`Offline`, so each peer used its own primary-local player as the sole loss
condition. When peer 0's primary player died while peer 1's primary player was
still alive, one peer entered `GameOver` and the other stayed `Playing`.
`ShouldEnterGameOver` now treats `input_lockstep_enabled` as multiplayer and
only enters game over once no connected player is alive.

Validation for this chunk:

- `cmake --build build --target splonks-cpp -j 8`
- `./build/splonks-cpp --check-state-equality-smoke`
- `./build/splonks-cpp --check-deterministic-replay-smoke`
- `./build/splonks-cpp --check-input-lockstep-smoke`

Next immediate steps:

- Live-test the two-window lockstep path beyond boot/connect.
- Verify the peer can carry the host player and both players can transition
  stages together without old ordered mutation backlog.
- If live carry/stage-transition validation needs automation, add a clean
  debug input/script lane. Do not add one-off state mutation commands just to
  force a scenario.
- Run and keep green:
  - `cmake --build build --target splonks-cpp -j 8`
  - `./build/splonks-cpp --check-state-equality-smoke`
  - `./build/splonks-cpp --check-deterministic-replay-smoke`
  - `./build/splonks-cpp --check-input-lockstep-smoke`
- Commit that validated coverage chunk before moving to live two-window testing.

Current live validation status:

- `scripts/run_multiplayer_pair_i3.sh` builds and launches two windows.
- Host and peer connect with `input_lockstep_enabled: true`.
- Host role is `coordinator`; peer role is `peer`.
- Both start on `classic_mines_1` with matching stage seed.
- The debug control server can inject ordinary local input with
  `scripts/splonksctl --port <port> input <frames> [buttons...]`. This feeds
  the same `PlayerInputFrame` path that lockstep transmits; it does not mutate
  entities, tiles, or stage state directly.
- Live injected peer movement was validated: `input 90 left run` on the peer
  moved player 2 from the spawn cluster to `x=6,y=82`, and both host and peer
  reported the same final player position.
- `ordered_messages`, `pending_outbound_messages`, and `applied_messages` are
  zero in the live net debug query, confirming the old durable mutation backlog
  is inactive in the basic boot/connect case.
- Debug control JSON now reports `input_owner` instead of stale
  `net_owner` authority language.
- Not yet proven live: peer carrying host player and both players transitioning
  stages together. Direct live pickup input against a normal host player did
  not carry because normal carry rules require player targets to be dead or
  stunned. Fake/headless coverage for carry, drop, throw, and stepped stage
  transition is green.

Current cleanup status:

- Active `StepPlaying` and `StepGameOver` no longer drain the old
  `GameplayActionRequested` mutation queue.
- Live `StepNetworkLobby` is lockstep-only for open sessions. Old snapshot,
  entity patch, fluid patch, ordered message, and peer request paths remain as
  legacy code debt but are no longer part of the live lobby step.
- Legacy code still present and should be deleted/quarantined in later cleanup:
  `src/world_ops/action.cpp`, `State::pending_gameplay_actions`,
  `src/gameplay_messages.hpp`, old `src/network/net_message_apply*`, old
  `src/network/net_gameplay_replication.*`, old packet mapper/apply files, and
  old CLI network smokes that validate mutation-message lanes.

Current stepped-transition coverage note:

- Files touched: `src/cli_state_smoke.cpp`, `src/network/net_lobby.hpp`,
  `src/network/net_lobby_internal.hpp`,
  `src/network/net_lobby_input_lockstep.cpp`, `src/step.cpp`, and
  `src/network/net_lobby_player_lifecycle.cpp`.
- Intent: let fake/headless lockstep tests exercise actual stage-transition
  stepping without requiring a live UDP socket.
- Findings fixed:
  - Process-local player lamp state diverged after transition because each peer
    controlled a different player. While input lockstep is enabled, every
    connected player now emits the same player lamp deterministically.
  - Network player lifecycle still used old coordinator-authoritative rules:
    coordinator revived/moved all players, while peers only revived/moved local
    players. While input lockstep is enabled, respawn/revive applies to all
    connected slots on every process.
- Latest validation status:
  - Build passed.
  - State equality smoke passed.
  - Deterministic replay smoke passed.
  - Input lockstep smoke passed, including `input lockstep carry transition
    final` on `classic_mines_2`.

## Unattended Run Contract

When continuing this work under `/goal`, keep running until the active checklist
is materially advanced, not merely until the first compile succeeds.

Rules for long solo runs:

- Make the smallest clean architectural change that advances input lockstep.
- Prefer deleting or quarantining old coordinator-authoritative code over
  adapting it.
- Keep entity/content code local-only. If a gameplay file starts asking whether
  it is host, peer, predicted, repaired, or remote, stop and redesign that seam.
- Run the core build and smoke suite after each stable chunk.
- Commit after each green sub-checkbox or stable cleanup milestone.
- If a chunk cannot be made green before stopping, write the exact failing
  command and observed failure here.
- Do not mark the lockstep goal complete until live validation proves: host and
  client connect, peer can carry host player, both transition stages together,
  and no old ordered mutation backlog is active.

## Current Solo-Run Checklist

This is the concrete work queue for the current `/goal` run. Do these before
claiming lockstep is usable.

1. Strengthen fake/headless lockstep coverage.
   - [x] Peer 0 owns player 1; peer 1 owns player 2.
   - [x] Peer-owned player can pick up/carry the host-owned player.
   - [x] Both players remain active and deterministic while one player is carried.
   - [x] Both players transition to the next stage together in fake/headless
     lockstep coverage.
   - [x] Stage transition must not create orphan players, duplicate held
     players, or process-local player divergence.
   - [x] Add explicit fake/headless drop/throw coverage for player-carry if
     live testing exposes gaps.
2. Keep the multi-local-player architecture intact.
   - A process may own multiple local `PlayerId`s.
   - Lockstep input packets batch independent player input streams.
   - Do not reintroduce a single-primary-player assumption.
3. Continue removing old coordinator-authoritative baggage from active gameplay.
   - No item-specific replication hooks.
   - No content branches for coordinator/peer/predicted/remote mutation.
   - No no-op wrappers kept solely for old network compatibility.
   - Quarantine old tests/tools if they are not useful for lockstep.
   - Active `StepPlaying` / `StepGameOver` no longer drain the old
     coordinator-authoritative `GameplayActionRequested` mutation queue. That
     queue remains only as legacy networking/test debt until the retired message
     protocol is removed.
   - `StepNetworkLobby` is now lockstep-only for open live sessions. The old
     snapshot/entity-patch/fluid-patch/ordered-message send path remains as
     legacy code for deletion, but it is no longer part of the live lobby step.
   - Debug control entity JSON now reports `input_owner` instead of old
     `net_owner: local-authority/remote-authority` terminology.
4. Reuse the old network impairment profiles for lockstep input transport.
   - Same-house, same-city, same-state, Texas-to-California,
     California-to-Florida, and Texas-to-Japan profiles should apply to input
     packets/fake transport.
   - Delay, jitter, loss, duplicate, and reorder tests should exercise the
     lockstep scheduler, not retired mutation-replication lanes.
5. Preserve single-player behavior.
   - Any cleanup must keep offline gameplay local-only and unchanged.
   - Entity/content code should read like ordinary local gameplay again.
6. Commit periodically.
   - Commit after each validated checkbox or stable sub-checkbox.
   - Do not leave hours of uncommitted lockstep surgery in the working tree.
   - If a known gap remains, write it here before committing.

## Why Reconsider

The current multiplayer model has become expensive in exactly the wrong place:
normal gameplay code. Every tool, held item, thrown entity, shop interaction,
trap, particle, light, pickup, tile break, and stage transition can create a new
sync edge. Even when the protocol lanes are broad, gameplay code still has to
know whether it is offline, coordinator, peer, predicted, repaired, locally
owned, remotely owned, or presentation-only.

That is a bad fit for this project because:

- Content iteration slows down when every new item risks bespoke netcode.
- Mods become fragile because arbitrary new behavior may need protocol support.
- Regressions are common because authority patches interact in subtle ways.
- The codebase accumulates multiplayer branches in systems that should not care
  about networking.
- High-latency action feel remains difficult because server-authoritative shared
  outcomes fight local responsiveness.

Input lockstep with rollback moves the boundary: networking owns input delivery,
frame agreement, save states, rollback, and resimulation. Gameplay code runs as
ordinary deterministic simulation and does not know whether the input came from
local hardware or the network.

## Expected Deletions

This is the main upside. Based on the current code shape, a successful lockstep
branch should allow deleting or dramatically shrinking:

- Most coordinator message apply files: entity/player/run/tile/fluid/presentation
  patch application should no longer be normal gameplay sync.
- Most gameplay replication emitters: entity spawned/damaged/thrown/held,
  tile changed/broken, stage light, fluid patch, run-state patch, and presentation
  command lanes become unnecessary as authoritative gameplay sync.
- Most `world_ops` request/apply networking seams. `world_ops` may remain useful
  as clean gameplay helpers, but it should not exist because peers need to ask a
  coordinator for every durable mutation.
- Most entity net-id mapping for ordinary runtime entities. Deterministic runtime
  allocation should produce the same local ids on every peer.
- Most local/remote authority branches in content.
- Most snapshot repair machinery for in-stage entity/tile state. Snapshots may
  still exist for late join or recovery, but not as the normal correctness path.
- Much of the current network smoke harness that asserts message-lane equality,
  replacing it with deterministic replay/hash tests.

The current rough size is about `12k` lines in `src/network`, plus about `800`
lines in `src/world_ops`, plus many content call sites mentioning net authority,
requests, replication, or prediction. We should not expect all of that to vanish:
lobby, UDP transport, packet fuzzing, input packets, hashes, state snapshots,
debug UI, and test harnesses still exist. But a large fraction of per-content
sync code should become dead.

## Compatibility With Mods

This model is much more mod-compatible than mutation replication.

If all peers load the same mod set and the modded simulation is deterministic,
new entities/items/tools can work in multiplayer without adding protocol fields.
The network does not need to know that a modded bow spawned a modded arrow which
exploded into modded water. It only needs all peers to receive the same inputs and
step the same deterministic code.

Required mod rules:

- All peers must agree on content ids, archetype tables, scripts, assets that
  affect gameplay, and initial stage seed.
- Gameplay-affecting randomness must come from deterministic game RNG streams,
  not wall-clock time or process-local RNG.
- Gameplay logic must not depend on pointer addresses, unordered container
  iteration, filesystem order, thread timing, audio timing, render timing, or
  local-only debug state.
- Floating-point behavior must be deterministic enough for our target platforms,
  or critical simulation should move toward fixed-point/integer choices over
  time.

Floating-point determinism note:

- Same-machine/same-build replay is expected to be stable enough for the first
  lockstep tests.
- Cross-machine lockstep can diverge from different math library behavior,
  fused-multiply-add choices, compiler flags, CPU floating-point modes, or
  platform-specific `sin`/`cos`/`sqrt`/`atan2` results.
- Do not enable `fast-math` for deterministic builds.
- If cross-platform drift becomes visible, migrate critical physics/collision
  values toward fixed-point or explicit quantization rather than patching netcode
  around drift.

Mods that violate determinism can still be allowed in single-player, but they
would not be multiplayer-safe.

## Model Options

### 1. Pure Delay-Based Input Lockstep

All peers exchange input for future frame `N + input_delay`. The sim only
advances when every required input for the frame is available.

Pros:

- Simplest correctness model.
- Minimal gameplay netcode.
- Great deterministic test target.
- No rollback/save-state complexity initially.

Cons:

- Local input latency equals configured delay.
- Bad on Texas/Japan style links unless the delay is high.
- Feels closer to Spelunky HD/2 online latency, which we have been trying to
  avoid.

### 2. Input Lockstep With Local Prediction And Rollback

Each peer predicts missing remote inputs and runs local input immediately. When
real inputs arrive, if prediction was wrong, rewind to the last agreed frame and
resimulate to the present.

Pros:

- Gameplay code can remain deterministic and network-agnostic.
- Local controls can feel immediate.
- Most content/mods need no netcode.
- Desync bugs become deterministic-state bugs, not per-item sync bugs.

Cons:

- Requires efficient state snapshot/restore.
- Requires deterministic simulation discipline.
- Rollback can be expensive if the whole `State` is huge.
- Presentation/audio/particles need rollback-aware handling or separation.
- Late joins still need a full state snapshot or spectator/wait policy.

### 3. Current Coordinator Mutation Replication

Coordinator owns broad shared state; peers request actions and receive patches.

Pros:

- Does not require whole-world determinism.
- Easier to support late join by snapshot.
- Better for server-authoritative anti-cheat.

Cons:

- Content code keeps accumulating authority and request seams.
- New content can need new sync behavior.
- High latency creates prediction/repair fights.
- Harder mod story because network protocol must understand durable categories.

Decision: pursue option 2, but implement option 1 first as the deterministic
baseline. Rollback without proven deterministic lockstep is noise.

## Architecture Target

Networking should own these systems:

- Session/lobby membership.
- Player slot assignment.
  - A network peer/process is not the same thing as a player. One process may
    own zero, one, or many local `PlayerId`s. Example supported shapes: two
    players on one machine and three players on another in the same session.
- Per-frame local input capture.
- Input packets for every local player owned by a peer/process.
  - Packets should carry a compact `PlayerId -> PlayerInputFrame` batch for the
    frame, not assume one player per connection.
- Remote input buffering.
- Frame scheduling / lockstep barrier.
- Deterministic seed and stage identity.
- State hash exchange.
- Desync detection.
- Optional state snapshot exchange for late join/recovery.
- Rollback save-state ring.
- Restore and resimulate when predicted input differs.
- Debug latency/loss/jitter profiles.

Gameplay should own these systems without network branches:

- Movement.
- Pickup/drop/throw/carry.
- Tools and held item use.
- Entity spawning/deactivation.
- Damage/death/stun.
- Tile breaking/changing.
- Fluid sim.
- Shops.
- Altars.
- Stage progression.
- Lights/particles/presentation, with a rollback-aware presentation policy.

Gameplay code should consume an input table keyed by `PlayerId`, not local
hardware directly and not network messages directly.

## Determinism Requirements

The first milestone is not rollback. The first milestone is proving that two
processes starting from the same state and receiving the same ordered inputs
produce the same gameplay hash every frame.

Required work:

- [x] Define a deterministic gameplay hash that excludes local-only render/audio
  data but includes every gameplay-affecting entity, tile, fluid, player, RNG,
  stage, and progression field.
- [ ] Make all deterministic RNG use explicit deterministic streams stored in `State`.
  Started with `State::drng` and snake AI. Remaining call sites must be
  classified and either converted to state-owned deterministic RNG or explicitly kept
  presentation/stagegen-only.
- [ ] Audit places that read wall-clock time, frame time, random device, pointer
  address, unordered iteration, or local debug flags during gameplay.
- [ ] Ensure entity iteration order is stable.
- [ ] Ensure runtime ids allocate deterministically from the same code paths.
- [ ] Classify particles/audio/lights as gameplay-affecting or presentation-only.
- [ ] Make fluid stepping deterministic under the same inputs.
- [ ] Make stage transition deterministic from agreed inputs.
  - Lockstep-active stage transitions now apply the same pending transition
    locally on every peer after the transition delay, instead of relying on the
    old coordinator stage-sync path.
- [ ] Add a headless same-process deterministic replay test.
- [x] Add a fake-transport deterministic replay test.
  Implemented by `--check-input-lockstep-smoke`: two independent `State`s,
  two player input streams, fake impaired packet delivery, and per-frame
  gameplay hash comparison. Real two-process UDP remains Phase 6.

## Rollback Requirements

Rollback should be added only after deterministic lockstep passes.

Required work:

- [ ] Split or snapshot `State` into gameplay state and non-rollback presentation
  state.
- [ ] Implement a save-state ring for the last `N` frames.
- [ ] Store local and remote inputs per frame.
- [ ] Predict missing remote input. Initial policy: repeat last input.
- [ ] On late real input mismatch, restore the last agreed frame and resimulate.
- [ ] Suppress duplicate one-shot presentation/audio during resimulation.
- [ ] Add debug overlay: current frame, confirmed frame, rollback count, rollback
  frames, prediction errors, input delay, remote buffer depth.
- [ ] Add fuzzer tests for latency, jitter, loss, duplicate, and reordering.

## Late Join / Reconnect

Input lockstep does not make late join free. A late peer cannot reconstruct the
current world from only the current input frame unless it replays from stage
start, which may be too slow.

Allowed policies:

- No late join during active stage for the first experiment.
- Join at next stage transition.
- Full gameplay snapshot from an existing peer or host, then enter lockstep from
  snapshot frame.
- Reconnect by restoring a retained player slot at the next safe sync point.

First implementation target: no active-stage late join. Peers join lobby before
stage start or wait for next stage. Add full snapshot later if the lockstep model
is retained.

## Presentation Policy

Presentation must not poison deterministic gameplay.

- Gameplay particles/lights that affect collision, damage, AI, or visibility
  must be deterministic gameplay entities or deterministic state.
- Pure audio, screen shake, sparkles, bubbles, transient light flashes, and HUD
  effects should be presentation-only and either not rolled back or replayed with
  duplicate suppression.
- During rollback resimulation, avoid emitting duplicate sounds/particles for
  old frames unless explicitly requested by debug.

## Execution Checklist

This is the working checklist. Do these in order. Do not skip ahead to rollback
or keep patching the old coordinator-replication model.

### Phase 0: Active Plan Alignment

- [x] Create branch `net-lockstep-experiment`.
- [x] Archive old coordinator-authoritative docs under
  `docs/legacy_authoritative_networking/`.
- [x] Remove active-doc references to old coordinator-authoritative cleanup and
  parity checklists.

Exit gate: active docs point only to lockstep/rollback. Old coordinator docs may
reference each other inside `docs/legacy_authoritative_networking/`, but they are
not active implementation targets.

### Phase 1: Make Player Input A First-Class Frame Table

Goal: gameplay reads deterministic per-player input from state, not from
`state.playing_inputs` as a single global player input.

- [ ] Define `PlayerId` / local player slot structs independent of old
  coordinator/peer ownership.
  - Current state: stable `PlayerId` and `PlayerSlot` already exist, but slots
    still carry old `Local`/`Remote` connection classification. That can stay
    until the old coordinator transport is removed.
  - Required invariant: `PlayerId` ownership is many-to-one with network
    processes. A single peer can own multiple local players, and every phase of
    lockstep must treat those as separate player input streams batched by the
    owning process.
- [x] Define `PlayerInputFrame` as the compact deterministic input record.
- [x] Add `State` storage for current frame inputs keyed by `PlayerId`.
  - Implemented on `State::players.slots`: each slot carries
    `input_frame`, `previous_input_frame`, and derived `PlayingInputs`.
- [x] Keep edge/down/released derivation deterministic from previous input
  frame.
- [x] Route offline primary player through the table.
- [x] Route local multiplayer/debug bots through the table.
- [x] Update player control lookup to get inputs by controlled entity/player id.
- [x] Replace `state.player_vid` assumptions with player-slot or primary-player
  helpers where needed.
  - No `state.player_vid` member remains; `controlled_entity_vid` remains as a
    camera/audio/debug selected entity, not as player identity.
- [x] Keep camera/audio listener semantics working for one local player.
- [x] Keep existing gameplay behavior unchanged in offline play.
  - Validation so far: `cmake --build build --target splonks-cpp -j 8`,
    `build/splonks-cpp --check-state-fingerprint-smoke`, and
    `build/splonks-cpp --check-state-equality-smoke` pass. Manual feel test is
    still useful after more lockstep cleanup.

Exit gate: single-player/offline build works, local player controls feel the
same, and no networking code is needed to step multiple local player inputs.

### Phase 2: Deterministic State Fingerprint And Same-Process Replay

Goal: prove same initial state plus same inputs gives same gameplay state before
we touch real networking.

- [x] Define `ComputeGameplayDeterminismFingerprint(State&)`.
  Deterministic replay uses this stricter hash, including the deterministic RNG
  cursor. Canonical/network equality fingerprints intentionally ignore the RNG
  cursor because old result-application smoke tests compare resulting world
  state, not replayed draw history.
- [x] Include gameplay-affecting state: stage identity, tiles, rotations,
  fluids, entities, player slots, tools, effects, RNG streams, progression, and
  stage frame.
- [x] Exclude local-only presentation/debug state: camera, audio emitters,
  particles if purely visual, debug UI, net transport queues.
- [x] Add scripted input sequences for one player.
- [x] Add scripted input sequences for multiple local players.
- [x] Add a CLI smoke that runs a stage for 1,000+ frames, records inputs/hashes,
  resets to the same seed, replays inputs, and verifies every hash.
- [x] Add an initial CLI smoke that replays a 180-frame movement/jump script
  with first-difference diagnostics.
- [x] Extend the CLI smoke with a 240-frame two-local-player replay script.
- [x] Add diagnostics that print first divergent frame and a small state summary.

Exit gate: deterministic same-process replay passes for at least movement,
jump/climb/hang, tool use, pickup/throw, tile break, explosion, fluid, shop, and
stage transition scenarios. Current broad replay covers movement, pickup/drop,
attack, bomb, rope, spawned entities, spikes, ladder, containers, and enemies.
Still add explicit fluid, shop, and stage-transition scripted scenarios.

### Phase 3: Determinism Audit And Cleanup

Goal: remove or isolate obvious nondeterminism before network lockstep hides it.

- [ ] Audit gameplay use of wall-clock time, render frame timing, SDL state, mouse
  state, OS state, pointer addresses, unordered iteration, and random device.
- [ ] Route gameplay randomness through deterministic RNG stored in `State`.
  `State::drng` is the deterministic RNG stream used by simulation. It must be
  state-owned, not `base_seed + frame`, because a
  frame may perform multiple random draws and skipped/double draws must show up
  as hash divergence. It is now fingerprinted by deterministic replay and seeded
  from the stage seed.
- [ ] Ensure entity allocation and iteration order are deterministic under the
  same inputs.
- [ ] Ensure fluid updates are deterministic under the same inputs.
- [ ] Classify particles/lights/audio as gameplay or presentation.
- [ ] Isolate presentation-only side effects so replay can suppress or ignore
  them.
- [ ] Keep debug/editor tools outside deterministic gameplay unless explicitly
  part of the scripted test.

Exit gate: deterministic replay stays green after enabling broad gameplay
coverage.

#### DRNG Conversion Checklist

Convert gameplay-affecting files to `State::drng`. Leave stagegen on the
stage seed path until stagegen itself is moved to explicit streams. Leave
presentation-only files on local RNG unless the result affects gameplay state.

- [x] `src/entities/snake.cpp`
- [x] `src/entities/caveman.cpp`
- [x] `src/entities/monkey.cpp`
- [x] `src/entities/spider.cpp`
- [x] `src/entities/pot.cpp`
- [x] `src/entities/box.cpp`
- [x] `src/entities/dice.cpp`
- [x] `src/entities/mattock.cpp` gameplay break chance; dig particles remain
  presentation RNG.
- [x] `src/entities/sac_altar.cpp` gameplay reward choice/velocity; sacrifice
  particles remain presentation RNG.
- [x] `src/entities/baseball_bat.cpp` kill sound variation remains
  presentation RNG.
- [x] `src/entities/bat.cpp` wake sound variation remains presentation RNG.
- [x] `src/entities/block.cpp` particles remain presentation RNG.
- [x] `src/entities/boulder.cpp` particles remain presentation RNG.
- [x] `src/entities/chest.cpp` gameplay loot/trap rolls; sparkles remain
  presentation RNG.
- [x] `src/entities/cobra.cpp` gameplay AI/spit cooldown; venom particles remain
  presentation RNG.
- [x] `src/entities/craps_table.cpp`
- [x] `src/entities/door.cpp` particles remain presentation RNG.
- [x] `src/entities/mantrap.cpp`
- [x] `src/entities/moving_platform.cpp` particles remain presentation RNG.
- [x] `src/entities/skeleton.cpp` loose skull velocity; break/death particles
  remain presentation RNG.
- [x] `src/on_damage_effects.cpp` particles remain presentation RNG.
- [x] `src/step.cpp` debug local-player bot RNG is local debug input
  generation; exclude bots from lockstep determinism unless their scripted input
  is explicitly recorded.
- [x] `src/entities/common/explosion.cpp`, `src/effects/treasure_pickup.cpp`,
  `src/entities/jetpack.cpp`, `src/entities/pistol.cpp`,
  `src/entities/web_cannon.cpp`, and `src/presentation_commands.cpp` use RNG for
  particles/transient presentation only.
- [x] `src/render/tiles_and_entities.cpp` and `src/sprite.cpp` use RNG for
  render shake/random-frame presentation only.
- [x] Debug-stage RNG remains debug-only and is outside lockstep sessions unless
  the resulting stage is snapshotted or generated from explicit seed streams.
- [ ] Stagegen/helper RNG in `src/stage.cpp`, `src/room.cpp`, `src/tile.cpp`,
  and `src/stage_gen/**` remains on the explicit stage-seed path for now; move
  it to named stagegen DRNG streams when stagegen determinism becomes the active
  task.

### Phase 4: Strip Coordinator Mutation Replication From Gameplay

Goal: stop the codebase from being hybrid. Gameplay should no longer ask whether
it is coordinator/peer before doing ordinary gameplay.

- [x] Remove peer action request/apply branches from content paths.
- [x] Remove `HasLocalAuthority...` checks from active entity/gameplay logic
  unless they are replaced by lockstep input ownership.
- [x] Remove `allow_remote_player_target` style damage exceptions created for
  coordinator-authority networking.
- [x] Remove entity/tile/run/fluid/stage-light/presentation replication emits from normal
  gameplay.
- [x] Skip old ordered mutation-message apply/drain while input lockstep is
  active.
- [x] Delete or quarantine old `world_ops` networking seams. Keep only gameplay
  helper pieces that remain useful without networking.
- [x] Rename remaining live local helper names that still say `Request` but no longer
  cross the network.
- [ ] Delete old network smoke tests that only validate mutation-message lanes.
- [ ] Keep transport/fuzzer code only if it is useful for input packets.
- [ ] Remove entity/archetype/debug baggage added only for the old
  coordinator-authoritative model: replica stepping, local prediction flags,
  replicated runtime flag helpers, and mutation-message-only state fields.
  - Removed unused entity archetype `replica_logic` / `step_as_replica` fields
    and their content initializers.
  - Renamed the stale carry helper
    `ReleaseEntityFromHolderAndEmitNetwork` to
    `ReleaseEntityFromHolderIfAttached`.
- [ ] Keep state fingerprint/replay code if it supports determinism.

Implementation note: `world_ops::{SpawnEntity,DeactivateEntity,SetForegroundTile}`,
stage lighting, stage fluids, and carry/damage/tool/tile-break content paths are
currently local deterministic helpers. Old no-op patch/presentation shims were
removed rather than kept as compatibility wrappers.

Exit gate: ordinary gameplay code has no coordinator/peer mutation branches, and
the project builds with the old replication model disabled or removed.

### Phase 5: Delay-Based Input Lockstep Over Fake Transport

Goal: run two simulated peers from the same initial state by exchanging only
inputs.

- [x] Define input packet: session id, stage instance id, player id, frame, input
  bits/axes, and local input sequence.
- [x] Add per-player input buffers indexed by frame.
- [x] Add lockstep frame scheduler.
- [x] Add fixed input delay.
- [x] Step frame `N` only when all required player inputs for `N` are available.
- [x] Exchange hashes every `N` frames.
  Current fake smoke compares every simulated frame, which is stricter than a
  periodic exchange.
- [x] Stop on mismatch and dump first divergent frame.
- [x] Add fake-transport tests for delay, jitter, reorder, duplicate, and loss.

Implementation:

- `src/network/input_lockstep.hpp`
- `src/network/input_lockstep.cpp`
- `--check-input-lockstep-smoke`

Validation:

- `cmake --build build --target splonks-cpp -j 8`
- `./build/splonks-cpp --check-state-equality-smoke`
- `./build/splonks-cpp --check-deterministic-replay-smoke`
- `./build/splonks-cpp --check-input-lockstep-smoke`

Exit gate: two or more fake peers can run scripted gameplay for thousands of
frames with matching hashes while exchanging only input and control packets.

### Phase 6: Real Two-Process Input Lockstep

Goal: boot two game windows and play with delay-based lockstep.

- [x] Reuse or replace the current UDP transport for input packets.
- [ ] Reuse the existing impairment/profile controls where possible:
  same-house/same-city/same-state/Texas-to-California/California-to-Florida/
  Texas-to-Japan latency, jitter, loss, duplicate, and reorder settings should
  feed the lockstep input transport/fuzzer instead of the retired authoritative
  mutation transport.
- [ ] Remove or hide live controls that only make sense for the old
  authoritative snapshot/event lanes once lockstep owns live networking.
- [x] Add lobby start barrier: stage seed, player list, stage id, start frame,
  input delay.
- [ ] Disable active-stage late join initially; peers join before start or wait
  for next stage.
- [ ] Add debug UI: local frame, remote input buffer depth, blocked frame count,
  hash, last agreed frame, packet loss/jitter profile.
  Current live debug exposes role, lockstep frame, local-input frame, and input
  delay. Buffer depth/hash/agreed-frame remain TODO.
- [x] Add a clean debug input/script lane for live validation.
  Implemented through `splonksctl input`, which writes a temporary
  `DebugInputOverrideState` consumed by normal input capture. This is not a
  world mutation/admin command.
- [ ] Keep current multiplayer pair launcher if useful.
- [ ] Playtest same-machine two-window lockstep.
  Basic launch/query verified host and peer advancing on the same stage/frame
  with zero authoritative ordered-message backlog. Manual carry and stage-exit
  validation remain TODO.
- [ ] Playtest artificial latency profiles.
- [ ] Remove old authoritative entity/network baggage once lockstep is live:
  content should stop carrying special coordinator/peer terms, stale
  request/result paths, and per-item replication hooks that are no longer part
  of the model.

Exit gate: two local windows can play a real stage without desync, using input
lockstep only.

### Phase 7: Rollback

Goal: remove input-delay feel while keeping deterministic correctness.

- [ ] Add save-state ring for the last `N` gameplay frames.
- [ ] Store input history for every player.
- [ ] Predict missing remote inputs. Initial policy: repeat last input.
- [ ] On late real input mismatch, restore last agreed frame and resimulate to
  current frame.
- [ ] Suppress duplicate presentation/audio during resimulation.
- [ ] Add rollback debug: current frame, confirmed frame, rollback count,
  rollback span, prediction miss rate.
- [ ] Add fuzzer tests for rollback under latency/jitter/loss.

Exit gate: high-latency profile remains locally responsive and hashes converge.

### Phase 8: Late Join / Reconnect

Goal: add player/session usability once the base model works.

- [ ] Decide first production policy: wait until next stage, replay from stage
  start, or snapshot current gameplay state.
- [ ] If using snapshots, define deterministic gameplay snapshot format.
- [ ] Reconnect to retained player slot at safe sync point.
- [ ] Add tests for disconnect/reconnect.

Exit gate: reconnect policy is explicit and does not compromise deterministic
frame stepping.

## Non-Negotiable Rules

- Do not add new item-specific netcode while this experiment is active.
- Do not keep both architectures half-wired in gameplay content.
- If a gameplay system needs a network branch under lockstep, treat that as a
  design smell unless it is input capture, deterministic seeding, save/restore,
  or presentation duplicate suppression.
- Do not optimize rollback before deterministic replay is proven.
- Do not delete the working authoritative branch until the lockstep branch can
  run a real two-player stage without desync.

## Solo Work / Commit Policy

- Subdivide large checkboxes into smaller checklist items when a phase is too
  broad to validate in one sitting.
- Commit after each completed checkbox or meaningful chunk of a checkbox.
- Keep commits focused around one recoverable milestone: cleanup, protocol
  shape, scheduler, test harness, rollback buffer, etc.
- Run the relevant build/smoke checks before each commit when practical.
- If a chunk leaves a known gap, record it in this plan before committing so the
  next session can resume without reconstructing context.
