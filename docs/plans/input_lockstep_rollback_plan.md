# Input Lockstep / Rollback Multiplayer Plan

Status: active experiment plan on branch `net-lockstep-experiment`.

Short entry point: `docs/plans/remote_multiplayer_plan.md`.
Legacy coordinator-authoritative documents live under
`docs/legacy_authoritative_networking/` and are reference-only.

This plan replaces the current coordinator-authoritative mutation-replication
plan as the next architecture experiment. The goal is to test whether a
deterministic input-driven model removes the networking tax from gameplay
content and gives us a cleaner long-term mod story.

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
- Per-frame local input capture.
- Input packets for every local player on a peer.
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

- [ ] Define a deterministic gameplay hash that excludes local-only render/audio
  data but includes every gameplay-affecting entity, tile, fluid, player, RNG,
  stage, and progression field.
- [ ] Make all gameplay RNG use explicit deterministic streams stored in `State`.
- [ ] Audit places that read wall-clock time, frame time, random device, pointer
  address, unordered iteration, or local debug flags during gameplay.
- [ ] Ensure entity iteration order is stable.
- [ ] Ensure runtime ids allocate deterministically from the same code paths.
- [ ] Classify particles/audio/lights as gameplay-affecting or presentation-only.
- [ ] Make fluid stepping deterministic under the same inputs.
- [ ] Make stage transition deterministic from agreed inputs.
- [ ] Add a headless same-process deterministic replay test.
- [ ] Add a two-process/fake-transport deterministic replay test.

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

### Phase 0: Baseline And Safety

- [x] Create branch `net-lockstep-experiment`.
- [x] Archive old coordinator-authoritative docs under
  `docs/legacy_authoritative_networking/`.
- [ ] Make sure the current authoritative branch/commit is preserved for
  comparison and emergency reference.
- [ ] Add a short `docs/current_multiplayer_cleanup_checklist.md` replacement or
  delete references to the old file from active docs.
- [ ] Run and record baseline status before ripping code:
  `cmake --build build --target splonks-cpp -j 8`.
- [ ] Run and record current smoke status. Existing network smokes may be deleted
  later, but the initial failure/success state should be known.

Exit gate: active docs point only to lockstep/rollback, the current branch builds,
and the old architecture is preserved in git history.

### Phase 1: Make Player Input A First-Class Frame Table

Goal: gameplay reads deterministic per-player input from state, not from
`state.playing_inputs` as a single global player input.

- [ ] Define `PlayerId` / local player slot structs independent of old
  coordinator/peer ownership.
- [ ] Define `PlayerInputFrame` as the compact deterministic input record.
- [ ] Add `State` storage for current frame inputs keyed by `PlayerId`.
- [ ] Keep edge/down/released derivation deterministic from previous input frame.
- [ ] Route offline primary player through the table.
- [ ] Route local multiplayer/debug bots through the table.
- [ ] Update player control lookup to get inputs by controlled entity/player id.
- [ ] Replace `state.player_vid` assumptions with player-slot or primary-player
  helpers where needed.
- [ ] Keep camera/audio listener semantics working for one local player.
- [ ] Keep existing gameplay behavior unchanged in offline play.

Exit gate: single-player/offline build works, local player controls feel the
same, and no networking code is needed to step multiple local player inputs.

### Phase 2: Deterministic State Fingerprint And Same-Process Replay

Goal: prove same initial state plus same inputs gives same gameplay state before
we touch real networking.

- [ ] Define `ComputeGameplayDeterminismHash(State&)`.
- [ ] Include gameplay-affecting state: stage identity, tiles, rotations,
  fluids, entities, player slots, tools, effects, RNG streams, progression, and
  stage frame.
- [ ] Exclude local-only presentation/debug state: camera, audio emitters,
  particles if purely visual, debug UI, net transport queues.
- [ ] Add scripted input sequences for one player.
- [ ] Add scripted input sequences for multiple local players.
- [ ] Add a CLI smoke that runs a stage for 1,000+ frames, records inputs/hashes,
  resets to the same seed, replays inputs, and verifies every hash.
- [ ] Add diagnostics that print first divergent frame and a small state summary.

Exit gate: deterministic same-process replay passes for at least movement,
jump/climb/hang, tool use, pickup/throw, tile break, explosion, fluid, shop, and
stage transition scenarios.

### Phase 3: Determinism Audit And Cleanup

Goal: remove or isolate obvious nondeterminism before network lockstep hides it.

- [ ] Audit gameplay use of wall-clock time, render frame timing, SDL state, mouse
  state, OS state, pointer addresses, unordered iteration, and random device.
- [ ] Route gameplay randomness through deterministic RNG stored in `State`.
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

### Phase 4: Strip Coordinator Mutation Replication From Gameplay

Goal: stop the codebase from being hybrid. Gameplay should no longer ask whether
it is coordinator/peer before doing ordinary gameplay.

- [ ] Remove peer action request/apply branches from content paths.
- [ ] Remove `HasLocalAuthority...` checks from entity/gameplay logic unless they
  are replaced by lockstep input ownership.
- [ ] Remove `allow_remote_player_target` style damage exceptions created for
  coordinator-authority networking.
- [ ] Remove entity/tile/run/fluid/presentation replication emits from normal
  gameplay.
- [ ] Delete or quarantine old `world_ops` networking seams. Keep only gameplay
  helper pieces that remain useful without networking.
- [ ] Delete old network smoke tests that only validate mutation-message lanes.
- [ ] Keep transport/fuzzer code only if it is useful for input packets.
- [ ] Keep state fingerprint/replay code if it supports determinism.

Exit gate: ordinary gameplay code has no coordinator/peer mutation branches, and
the project builds with the old replication model disabled or removed.

### Phase 5: Delay-Based Input Lockstep Over Fake Transport

Goal: run two simulated peers from the same initial state by exchanging only
inputs.

- [ ] Define input packet: session id, stage instance id, player id, frame, input
  bits/axes, and local input sequence.
- [ ] Add per-player input buffers indexed by frame.
- [ ] Add lockstep frame scheduler.
- [ ] Add fixed input delay.
- [ ] Step frame `N` only when all required player inputs for `N` are available.
- [ ] Exchange hashes every `N` frames.
- [ ] Stop on mismatch and dump first divergent frame.
- [ ] Add fake-transport tests for delay, jitter, reorder, duplicate, and loss.

Exit gate: two or more fake peers can run scripted gameplay for thousands of
frames with matching hashes while exchanging only input and control packets.

### Phase 6: Real Two-Process Input Lockstep

Goal: boot two game windows and play with delay-based lockstep.

- [ ] Reuse or replace the current UDP transport for input packets.
- [ ] Add lobby start barrier: stage seed, player list, stage id, start frame,
  input delay.
- [ ] Disable active-stage late join initially; peers join before start or wait
  for next stage.
- [ ] Add debug UI: local frame, remote input buffer depth, blocked frame count,
  hash, last agreed frame, packet loss/jitter profile.
- [ ] Keep current multiplayer pair launcher if useful.
- [ ] Playtest same-machine two-window lockstep.
- [ ] Playtest artificial latency profiles.

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
