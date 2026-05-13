# Input Lockstep / Rollback Multiplayer Plan

Status: active experiment plan on branch `net-lockstep-experiment`.

Short entry point: `docs/plans/remote_multiplayer_plan.md`.
Legacy host-authoritative documents live under
`docs/legacy_authoritative_networking/` and are reference-only.

This plan replaces the current host-authoritative mutation-replication
plan as the next architecture experiment. The goal is to test whether a
det input-driven model removes the networking tax from gameplay
content and gives us a cleaner long-term mod story.

## Current Goal

Implement authoritative input lockstep cleanly, with rollback used to keep
input delay low after delay-only lockstep proved deterministic.

Success target for the current branch:

- Boot host and client from the multiplayer pair launcher.
- Connect them into one lockstep session.
- Support any number of local players per process in the architecture, even if
  the first live test uses one local player per process.
- Let the client carry the host player.
- Let both players transition through stages together.
- Preserve single-player behavior.
- Remove old host-authoritative mutation cruft from active gameplay.
- Keep ent/content code local-only in language and behavior: no item-specific
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
- Delay-based input lockstep and rollback are both active for this goal.
  Delay remains the deterministic fallback; rollback is the latency-feel path.
- Treat old host-authoritative leftovers as cleanup debt. Remove them
  from active gameplay or quarantine them as legacy tests/tools; do not adapt
  new gameplay code to those paths.
- Completion requires live validation, not just CLI smokes: two windows connect,
  the peer can carry the host player, and both players can transition stages
  together.
- Live lockstep should show no active ordered mutation backlog from the old
  replication lanes.

## Current Worktree Handoff

Last known branch: `net-lockstep-experiment`.

Active model status:

- Old host-authoritative mutation lanes are no longer active in live packet
  stepping.
- Live lockstep uses join/leave/input-frame packets.
- A lockstep frame-pacing guard prevents a process from stepping past a remote
  process's advertised sim frame. Remote advertised frame is inferred as
  `latest input frame received for that remote player - input_delay`.
- Without that guard, a peer could legally receive the host's future input
  records and run about one input-delay window ahead of the host. That looked
  like a huge live desync even when the inputs were arriving.

## Delay And Rollback Plan

Goal: make lockstep feel good without returning networking concerns to
gameplay/content code.

Current facts:

- Networking is main-thread pumped once per simulation tick. There is no
  separate net thread.
- Default delay is currently `2` sim frames. At 60fps that is roughly `33ms`
  before real network latency. Higher fixed delays can still be selected before
  hosting.
- Protocol ping/pong now measures application-level RTT and jitter. Same-machine
  RTT includes simulation scheduling, not just kernel/socket latency.
- Delay-based lockstep is simple and det, but it always adds input latency.
- Rollback is the route to local-feeling controls over long-distance links, but
  it requires retaining and replaying deterministic state.

Implementation direction:

1. Keep configurable input delay as the safe baseline.
   Delay is deterministic and easy to reason about. It should remain selectable
   before hosting and visible in debug, because some links may be stable enough
   to prefer pure delay over frequent rollback.
2. Use rollback to make the default delay small.
   The default should stay near `2` frames on good links. Missing remote inputs
   are predicted, late differing inputs rewind to the saved gameplay snapshot,
   and the sim replays to the present.
3. Prove correctness before hiding correction artifacts.
   The first exit gate is final-state equality under latency/jitter/reorder.
   Smoothing, camera polish, and duplicate presentation suppression come after
   rollback replay is deterministic.
4. Keep gameplay code unaware of networking.
   Items, ents, traps, shops, fluids, and stage progression should only consume
   `InputFrame`s and normal state. The lockstep scheduler owns delay,
   prediction, snapshots, rollback, and replay.

Current two-track plan:

We should do both configurable delay and rollback, but they solve different
problems.

Track A: delay baseline.

- Keep fixed input delay as the deterministic safety mode.
- Use it for LAN or stable links where `2-5` frames feels acceptable.
- Expose measured RTT/jitter and a suggested delay, but do not dynamically
  mutate the active delay unless the change is scheduled for a future lockstep
  frame.
- Exit gate: with rollback disabled, a selected delay produces no missing-input
  stalls and all peers hash to the same state.

Track B: rollback/prediction.

- Keep the default delay low, currently `2` frames.
- Predict missing remote input by repeating the last known input.
- When a late input disagrees, restore the gameplay snapshot for the mismatch
  frame and replay to current.
- Keep presentation/audio out of replay so old sounds and particles do not
  duplicate.
- Exit gate: under fuzzer latency/jitter/reorder, peers converge to the same
  gameplay hash, rollback counters advance, and local movement feels better
  than raising delay to cover worst-case latency.

Implementation order from here:

1. Finish rollback telemetry so we can see prediction misses, rollback count,
   replay cost, and snapshot depth while testing.
2. Keep the existing fixed-delay selector and suggested delay UI as the fallback
   path.
3. Run smoke coverage for no-fuzzer, LAN-ish, cross-country, and Texas/Japan
   profiles.
4. Human-playtest movement, hang, jump, carry/throw, tools, explosives, and
   stage transition under the fuzzer profiles.
5. Tune default delay and prediction policy only after correctness is stable.
6. Add smoothing only for visual correction artifacts; never hide a real
   deterministic divergence with interpolation.

Next tuning plan: live delay/window controls.

Goal: let the host tune latency behavior during a session without breaking
lockstep determinism.

1. Scheduled input-delay changes.
   - [x] Add a lockstep settings-change record with:
     `apply_frame`, `input_delay_frames`, and a monotonically increasing
     settings sequence.
   - [x] Host chooses `apply_frame = current_frame + max(old_delay, new_delay) +
     safety_margin`.
   - [x] Host sends the settings record through the lockstep packet path and
     resends it while pending plus a short post-apply retention window.
   - [x] Peers store the pending change and apply it only when their sim reaches
     `apply_frame`.
   - [x] UI and `splonksctl net` show active delay, pending delay, and apply
     frame.
   - [x] Manual changes schedule; they do not mutate
     `lockstep_input_delay_frames` immediately on only one process.
   - [x] Exit gate: `--check-input-lockstep-smoke` includes a settings smoke
     that schedules a delay change, verifies it does not apply early, and
     verifies host/peer apply the same value at the scheduled frame.

2. Scheduled rollback-window changes.
   - [x] Add `max_rollback_frames` to the same scheduled settings record.
   - [x] Increasing the window applies at `apply_frame`.
   - [x] Decreasing the window prunes only snapshots older than the new window
     after `apply_frame`; it must not delete snapshots needed for already-known
     pending rollback.
   - [x] UI and `splonksctl net` show active rollback window, pending rollback
     window, retained snapshot count, max observed rollback span, and replay
     cost.
   - [x] Exit gate: `--check-input-lockstep-smoke` includes a settings smoke
     that schedules a rollback-window change and verifies host/peer apply the
     same value at the scheduled frame.

3. Optional host auto-delay.
   - [x] Host computes a target from connected peers:
     `ceil(((max_peer_rtt / 2) + jitter_margin_ms) / frame_ms) + safety_frames`.
   - [x] Smooth the target so it does not flap: require the suggested value to stay
     different for a cooldown window before scheduling a change.
   - [x] Manual override disables auto scheduling but still displays the suggested
     value.
   - [x] Clamp the target to the configured min/max delay range.
   - [x] Prefer keeping delay low and letting rollback handle spikes; auto-delay
     should prevent chronic prediction misses, not hide every transient packet
     spike.
   - [x] Exit gate: `--check-input-lockstep-smoke` includes an auto-delay smoke
     that feeds peer RTT/jitter through the stable-candidate path and verifies a
     scheduled deterministic settings change.

Phase 1: configurable delay.

- [x] Add a host-side debug/network control for `lockstep_input_delay_frames`
  before hosting.
- [x] Display delay in both frames and milliseconds.
- [x] Display measured RTT/jitter per peer on host and peer.
- [x] Add an automatic suggested delay:
  `ceil(((RTT / 2) + jitter_margin_ms) / frame_ms) + safety_frames`.
- [x] Keep active-session delay changes disabled at first. The join accept
  already carries the initial host-selected delay, so pre-session tuning is
  safe.
- [x] If live delay changes are needed later, apply them as a scheduled
  lockstep setting change at a future frame. Never mutate delay immediately on
  one process.
- [x] Add a smoke or control-server check that starts a fake session with a
  non-default delay and proves both sides step identically.

Expected useful delay presets:

- Localhost / same machine: `1-2` frames.
- Same house / LAN: `2-3` frames.
- Same city/state: `3-5` frames.
- Cross-country: `5-8` frames.
- Texas to Japan or similar: `8+` frames, likely still better with rollback.

Phase 2: rollback.

- [x] Store a compact deterministic state ring buffer for recent simulation
  frames. This must not be the heavyweight debug playback history by default,
  though the existing snapshot/replay code can guide implementation.
- [x] Store all local and remote `InputFrame` records by frame.
- [x] Step local input with a small default delay. Rollback is enabled by
  default, and the host can still select a higher fixed delay before hosting.
- [x] Predict missing remote inputs by reusing the remote player's last known
  input, or neutral input for disconnected/uninitialized players.
- [x] When late real input arrives and differs from the prediction, restore the
  saved state before the mismatch and replay to the present.
- [x] Keep gameplay/content deterministic and network-agnostic. Rollback should
  live in the simulation scheduler/state layer, not in items, ents, shops, or
  traps.
- [x] Suppress duplicate presentation/audio during rollback resimulation by
  replaying past frames with a dummy audio sink and restoring the pre-rollback
  presentation layer after gameplay catches up.
- [ ] Add correction/presentation smoothing only after deterministic rollback is
  correct. First priority is identical final state, not hiding corrections.
- [x] Define a maximum rollback window. Start with `8-12` frames for testing;
  increase only after measuring memory and replay cost.
- [x] Track initial rollback metrics in Debug Network: rollback count, last
  rollback span, max rollback span, last replay ms, and retained snapshot count.
- [x] Add richer rollback telemetry for tuning: average replay ms, prediction
  miss count, late predicted-input match count, and last miss span.
- [ ] Add per-second rollback count and latest mismatch source if live tuning
  needs more detail.
- [x] Add a focused rollback repair smoke that replaces a wrong predicted input
  with the real input and proves final state equality after replay.
- [x] Add a latency/jitter/reorder rollback smoke that forces repeated late
  remote input delivery, exercises multiple rollbacks, and proves final state
  equality.

Rollback prerequisites:

- [ ] Prove the deterministic replay smoke covers enough real gameplay:
  movement, jump/hang, pickup/drop/throw, tools, explosions, tile breaks,
  shops, water, stage transition, death/respawn.
- [ ] Audit gameplay RNG. Gameplay randomness must use `state.drng` or an
  equivalent deterministic stream, never process-local random calls.
- [ ] Audit non-deterministic timers and real-time reads. Simulation decisions
  must depend on frame/tick state, not wall-clock time.
- [x] Use broad gameplay snapshot scope first for correctness, then optimize
  entity/state storage only if memory or replay cost requires it.
- [x] During live rollback replay, preserve the current presentation layer
  instead of replaying old particles/audio/transient lights.
- [ ] Continue classifying presentation-only state. If presentation state affects
  gameplay, it is not presentation-only and must be deterministic.

Immediate resume checklist for the next `/goal` run:

1. Finish removing old host-authoritative packet/apply code that became
   unreachable after the packet-pump narrowing.
2. Remove or quarantine old world-snapshot/message-apply files instead of
   adapting them to lockstep.
3. Clean declarations in `src/network/net_lobby_internal.hpp`,
   `src/network/net_protocol.*`, and `src/network/net_session.*` only as
   references disappear. `src/network/net_message.*` should remain deleted.
4. Preserve the input lockstep path: join, leave, input frame records, relay,
   player-slot input tables, det stepping, stage transition.
5. Validate with:
   `cmake --build build --target splonks-cpp -j 8`,
   `./build/splonks-cpp --check-state-equality-smoke`,
   `./build/splonks-cpp --check-det-replay-smoke`, and
   `./build/splonks-cpp --check-input-lockstep-smoke`.
6. Commit after the green cleanup chunk before starting another large slice.

Recent validated commits on this branch:

- `5fd8c5e Remove stale gameplay authority stub`
- `eaeb95c Use input ownership terminology`
- `97a786d Remove legacy replication lanes`
- `0c05144 Remove legacy action request lane`
- `18b4cec Add debug input lane for lockstep validation`
- `564a3ac Remove legacy mutation network smokes`
- `1082eda Remove stale authority damage seams`
- `bc8500d Remove stale tile and damage replication hooks`
- `5d26df7 Simulate all player slots during lockstep`
- `d517157 Harden lockstep smoke topology`
- `cc34b6b Remove obsolete replication shims`
- `b895730 Document current lockstep solo checklist`
- `9add7d6 Add lockstep carry transition smoke`
- `37b2e03 Remove stale replica ent cruft`
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
  - Expands first-difference diagnostics for ents, `drng`,
    `stage.tile_change_generation`, and player input fields.
- `src/state_fingerprint.cpp`
  - Removes process-local player registry fields (`connection_kind` and
    `primary_local`) from gameplay determinism fingerprinting.
- `src/ent/manager.cpp`
  - Hardens `EntPool::SetInactive` against out-of-range ids and duplicate
    deactivation.

Resolved finding:

The stricter topology exposed a real process-local gameplay branch:

```text
state equality smoke input lockstep initial ok: stage=classic_mines_1 frame=0 stage_frame=0 ents=54 tiles=40x32 hash=15016652501064990246
input lockstep smoke clean hash mismatch at frame 342
  peer0 hash=9342928562898737137
  peer1 hash=358909475661019486
  first simple diff: ent 29 ident differs: active 1/1 type 13/13 vid 29:2/29:1
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
- `./build/splonks-cpp --check-det-replay-smoke`
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
  - `./build/splonks-cpp --check-det-replay-smoke`
  - `./build/splonks-cpp --check-input-lockstep-smoke`
- Commit that validated coverage chunk before moving to live two-window testing.

Current live validation status:

- `scripts/run_multiplayer_pair_i3.sh` builds and launches two windows.
- Host and peer connect with `input_lockstep_enabled: true`.
- Host role is `host`; peer role is `peer`.
- Both start on `classic_mines_1` with matching stage seed.
- The debug control server can inject ordinary local input with
  `scripts/splonksctl --port <port> input <frames> [buttons...]`. This feeds
  the same `InputFrame` path that lockstep transmits; it does not mutate
  ents, tiles, or stage state directly.
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

Latest validated cleanup:

- `18b4cec` added a debug input lane for lockstep validation:
  - `splonksctl input <frames> [buttons...]` injects ordinary local input.
  - The injection is consumed by normal input capture and transmitted as
    `InputFrame`; it does not mutate world state directly.
  - Live validation confirmed peer input can move player 2 and both processes
    report the same final position.
- `564a3ac` removed old mutation-lane network CLI smokes from the active build:
  - Deleted `src/cli_network_smoke*.{cpp,hpp}` files.
  - Removed obsolete CLI flags:
    `--check-network-protocol-smoke`, `--check-network-action-smoke`,
    `--check-network-packet-smoke`, and `--check-network-frame-smoke`.
  - Core validation passed after the deletion:
    `cmake --build build --target splonks-cpp -j 8`,
    `./build/splonks-cpp --check-state-equality-smoke`,
    `./build/splonks-cpp --check-det-replay-smoke`, and
    `./build/splonks-cpp --check-input-lockstep-smoke`.

Current cleanup status:

- Active `StepPlaying` and `StepGameOver` no longer drain the old
  `GameplayActionRequested` mutation queue.
- Live `StepNetworkLobby` is lockstep-only for open sessions. Old snapshot,
  ent patch, fluid patch, ordered message, and peer request paths remain as
  legacy code debt but are no longer part of the live lobby step.
- Old CLI network smokes that validated mutation-message lanes were removed
  from the active build and deleted.
- The old gameplay action-request lane has been removed from active source:
  - deleted `src/world_ops/action.cpp`
  - deleted `src/gameplay_messages.hpp`
  - deleted `src/network/net_lobby_packets_action.cpp`
  - removed `State::pending_gameplay_actions`
  - removed `world_ops::{QueueGameplayAction,QueuePendingGameplayAction,ProcessPendingGameplayActions}`
  - removed `ReplicateActionRequest`
  - removed action-request packet send/decode/ack handling from live lobby code
  - removed `NetMessageType::ActionRequest`, `NetActionKind`,
    `ActionRequestMessage`, and `ActionRequestMessages` / `ActionRequestAck`
    protocol structs.
- Non-action legacy replication payload structs and the old
  `src/network/net_replication_payloads.hpp` quarantine were later deleted
  with the old snapshot/repair code.
- Local interaction behavior previously stored in `world_ops/action.cpp` was
  retained as local-only gameplay in `src/world_ops/ent.cpp`.
- Validation for the action-lane deletion:
  - `cmake --build build --target splonks-cpp -j 8`
  - `./build/splonks-cpp --check-state-equality-smoke`
  - `./build/splonks-cpp --check-det-replay-smoke`
  - `./build/splonks-cpp --check-input-lockstep-smoke`

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
    connected player now emits the same player lamp detally.
  - Network player lifecycle still used old host-authoritative rules:
    host revived/moved all players, while peers only revived/moved local
    players. While input lockstep is enabled, respawn/revive applies to all
    connected slots on every process.
- Latest validation status:
  - Build passed.
  - State equality smoke passed.
  - Det replay smoke passed.
  - Input lockstep smoke passed, including `input lockstep carry transition
    final` on `classic_mines_2`.

## Unattended Run Contract

When continuing this work under `/goal`, keep running until the active checklist
is materially advanced, not merely until the first compile succeeds.

Rules for long solo runs:

- Make the smallest clean architectural change that advances input lockstep.
- Prefer deleting or quarantining old host-authoritative code over
  adapting it.
- Keep ent/content code local-only. If a gameplay file starts asking whether
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
   - [x] Both players remain active and det while one player is carried.
   - [x] Both players transition to the next stage together in fake/headless
     lockstep coverage.
   - [x] Stage transition must not create orphan players, duplicate held
     players, or process-local player divergence.
   - [x] Add explicit fake/headless drop/throw coverage for player-carry if
     live testing exposes gaps.
   - [x] Add explicit multiplayer respawn policies:
     `GenerousNextLevel`, `NoRespawn`, and `RespawnAtEntrance`.
   - [x] Cover respawn policies in fake/headless lockstep smoke: easy respawns
     a dead player at the entrance, generous revives dead players on next-stage
     transition, and no-respawn leaves dead players as spectators while living
     players advance.
2. Keep the multi-local-player architecture intact.
   - A process may own multiple local `PlayerId`s.
   - Lockstep input packets batch independent player input streams.
   - Do not reintroduce a single-primary-player assumption.
3. Continue removing old host-authoritative baggage from active gameplay.
   - No item-specific replication hooks.
   - No content branches for host/peer/predicted/remote mutation.
   - No no-op wrappers kept solely for old network compatibility.
   - Quarantine old tests/tools if they are not useful for lockstep.
   - [x] Delete the old host-authoritative `GameplayActionRequested`
     mutation queue and request/ack protocol.
   - `StepNetworkLobby` is now lockstep-only for open live sessions. The old
     snapshot/ent-patch/fluid-patch/ordered-message send path remains as
     legacy code for deletion, but it is no longer part of the live lobby step.
   - Debug control ent JSON now reports `input_owner` instead of old
     `net_owner: local-authority/remote-authority` terminology.
4. Reuse the old network impairment profiles for lockstep input transport.
   - Same-house, same-city, same-state, Texas-to-California,
     California-to-Florida, and Texas-to-Japan profiles should apply to input
     packets/fake transport.
   - Delay, jitter, loss, duplicate, and reorder tests should exercise the
     lockstep scheduler, not retired mutation-replication lanes.
5. Preserve single-player behavior.
   - Any cleanup must keep offline gameplay local-only and unchanged.
   - Ent/content code should read like ordinary local gameplay again.
6. Commit periodically.
   - Commit after each validated checkbox or stable sub-checkbox.
   - Do not leave hours of uncommitted lockstep surgery in the working tree.
   - If a known gap remains, write it here before committing.

## Why Reconsider

The current multiplayer model has become expensive in exactly the wrong place:
normal gameplay code. Every tool, held item, thrown ent, shop interaction,
trap, particle, light, pickup, tile break, and stage transition can create a new
sync edge. Even when the protocol lanes are broad, gameplay code still has to
know whether it is offline, host, peer, predicted, repaired, locally
owned, remotely owned, or pres-only.

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
ordinary det simulation and does not know whether the input came from
local hardware or the network.

## Expected Deletions

This is the main upside. Based on the current code shape, a successful lockstep
branch should allow deleting or dramatically shrinking:

- Most host message apply files: ent/player/run/tile/fluid/pres
  patch application should no longer be normal gameplay sync.
- Most gameplay replication emitters: ent spawned/damaged/thrown/held,
  tile changed/broken, stage light, fluid patch, run-state patch, and pres
  command lanes become unnecessary as authoritative gameplay sync.
- Most `world_ops` request/apply networking seams. `world_ops` may remain useful
  as clean gameplay helpers, but it should not exist because peers need to ask a
  host for every durable mutation.
- Most ent net-id mapping for ordinary runtime ents. Det runtime
  allocation should produce the same local ids on every peer.
- Most local/remote authority branches in content.
- Most snapshot repair machinery for in-stage ent/tile state. Snapshots may
  still exist for late join or recovery, but not as the normal correctness path.
- Much of the current network smoke harness that asserts message-lane equality,
  replacing it with det replay/hash tests.

The current rough size is about `12k` lines in `src/network`, plus about `800`
lines in `src/world_ops`, plus many content call sites mentioning net authority,
requests, replication, or prediction. We should not expect all of that to vanish:
lobby, UDP transport, packet fuzzing, input packets, hashes, state snapshots,
debug UI, and test harnesses still exist. But a large fraction of per-content
sync code should become dead.

## Compatibility With Mods

This model is much more mod-compatible than mutation replication.

If all peers load the same mod set and the modded simulation is det,
new ents/items/tools can work in multiplayer without adding protocol fields.
The network does not need to know that a modded bow spawned a modded arrow which
exploded into modded water. It only needs all peers to receive the same inputs and
step the same det code.

Required mod rules:

- All peers must agree on content ids, spec tables, scripts, assets that
  affect gameplay, and initial stage seed.
- Gameplay-affecting randomness must come from det game RNG streams,
  not wall-clock time or process-local RNG.
- Gameplay logic must not depend on pointer addresses, unordered container
  iteration, filesystem order, thread timing, audio timing, render timing, or
  local-only debug state.
- Floating-point behavior must be det enough for our target platforms,
  or critical simulation should move toward fixed-point/integer choices over
  time.

Floating-point determinism note:

- Same-machine/same-build replay is expected to be stable enough for the first
  lockstep tests.
- Cross-machine lockstep can diverge from different math library behavior,
  fused-multiply-add choices, compiler flags, CPU floating-point modes, or
  platform-specific `sin`/`cos`/`sqrt`/`atan2` results.
- Do not enable `fast-math` for det builds.
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
- Great det test target.
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

- Gameplay code can remain det and network-agnostic.
- Local controls can feel immediate.
- Most content/mods need no netcode.
- Desync bugs become det-state bugs, not per-item sync bugs.

Cons:

- Requires efficient state snapshot/restore.
- Requires det simulation discipline.
- Rollback can be expensive if the whole `State` is huge.
- Pres/audio/particles need rollback-aware handling or separation.
- Late joins still need a full state snapshot or spectator/wait policy.

### 3. Current Host Mutation Replication

Host owns broad shared state; peers request actions and receive patches.

Pros:

- Does not require whole-world determinism.
- Easier to support late join by snapshot.
- Better for server-authoritative anti-cheat.

Cons:

- Content code keeps accumulating authority and request seams.
- New content can need new sync behavior.
- High latency creates prediction/repair fights.
- Harder mod story because network protocol must understand durable categories.

Decision: pursue option 2, but implement option 1 first as the det
baseline. Rollback without proven det lockstep is noise.

## Architecture Target

Networking should own these systems:

- Session/lobby membership.
- Player slot assignment.
  - A network peer/process is not the same thing as a player. One process may
    own zero, one, or many local `PlayerId`s. Example supported shapes: two
    players on one machine and three players on another in the same session.
- Per-frame local input capture.
- Input packets for every local player owned by a peer/process.
  - Packets should carry a compact `PlayerId -> InputFrame` batch for the
    frame, not assume one player per connection.
- Remote input buffering.
- Frame scheduling / lockstep barrier.
- Det seed and stage ident.
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
- Ent spawning/deactivation.
- Damage/death/stun.
- Tile breaking/changing.
- Fluid sim.
- Shops.
- Altars.
- Stage progression.
- Lights/particles/pres, with a rollback-aware pres policy.

Gameplay code should consume an input table keyed by `PlayerId`, not local
hardware directly and not network messages directly.

## Determinism Requirements

The first milestone is not rollback. The first milestone is proving that two
processes starting from the same state and receiving the same ordered inputs
produce the same gameplay hash every frame.

Required work:

- [x] Define a det gameplay hash that excludes local-only render/audio
  data but includes every gameplay-affecting ent, tile, fluid, player, RNG,
  stage, and progression field.
- [ ] Make all det RNG use explicit det streams stored in `State`.
  Started with `State::drng` and snake AI. Remaining call sites must be
  classified and either converted to state-owned det RNG or explicitly kept
  pres/stagegen-only.
- [ ] Audit places that read wall-clock time, frame time, random device, pointer
  address, unordered iteration, or local debug flags during gameplay.
- [ ] Ensure ent iteration order is stable.
- [ ] Ensure runtime ids allocate detally from the same code paths.
- [ ] Classify particles/audio/lights as gameplay-affecting or pres-only.
- [ ] Make fluid stepping det under the same inputs.
- [ ] Make stage transition det from agreed inputs.
  - Lockstep-active stage transitions now apply the same pending transition
    locally on every peer after the transition delay, instead of relying on the
    old host stage-sync path.
  - Network transition seeds are derived from the current stage seed and target
    stage ident, not local wall-frame timing.
  - Stage-transition frames are lockstep-gated while a transport is active, so
    one peer cannot count down and load the next stage while another peer is
    stalled waiting on input.
- [ ] Add a headless same-process det replay test.
- [x] Add a fake-transport det replay test.
  Implemented by `--check-input-lockstep-smoke`: two independent `State`s,
  two player input streams, fake impaired packet delivery, and per-frame
  gameplay hash comparison. Real two-process UDP remains Phase 6.

## Rollback Requirements

Rollback should be added only after det lockstep passes.

Required work:

- [ ] Split or snapshot `State` into gameplay state and non-rollback pres
  state.
- [ ] Implement a save-state ring for the last `N` frames.
- [ ] Store local and remote inputs per frame.
- [ ] Predict missing remote input. Initial policy: repeat last input.
- [ ] On late real input mismatch, restore the last agreed frame and resimulate.
- [ ] Suppress duplicate one-shot pres/audio during resimulation.
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

## Pres Policy

Pres must not poison det gameplay.

- Gameplay particles/lights that affect collision, damage, AI, or visibility
  must be det gameplay ents or det state.
- Pure audio, screen shake, sparkles, bubbles, transient light flashes, and HUD
  effects should be pres-only and either not rolled back or replayed with
  duplicate suppression.
- During rollback resimulation, avoid emitting duplicate sounds/particles for
  old frames unless explicitly requested by debug.

## Execution Checklist

This is the working checklist. Do these in order. Do not skip ahead to rollback
or keep patching the old host-replication model.

### Phase 0: Active Plan Alignment

- [x] Create branch `net-lockstep-experiment`.
- [x] Archive old host-authoritative docs under
  `docs/legacy_authoritative_networking/`.
- [x] Remove active-doc references to old host-authoritative cleanup and
  parity checklists.

Exit gate: active docs point only to lockstep/rollback. Old host docs may
reference each other inside `docs/legacy_authoritative_networking/`, but they are
not active implementation targets.

### Phase 1: Make Player Input A First-Class Frame Table

Goal: gameplay reads det per-player input from state, not from
`state.playing_inputs` as a single global player input.

- [ ] Define `PlayerId` / local player slot structs independent of old
  host/peer ownership.
  - Current state: stable `PlayerId` and `PlayerSlot` already exist, but slots
    still carry old `Local`/`Remote` connection classification. That can stay
    until the old host transport is removed.
  - Required invariant: `PlayerId` ownership is many-to-one with network
    processes. A single peer can own multiple local players, and every phase of
    lockstep must treat those as separate player input streams batched by the
    owning process.
- [x] Define `InputFrame` as the compact det input record.
- [x] Add `State` storage for current frame inputs keyed by `PlayerId`.
  - Implemented on `State::players.slots`: each slot carries
    `input_frame`, `previous_input_frame`, and derived `PlayingInputs`.
- [x] Keep edge/down/released derivation det from previous input
  frame.
- [x] Route offline primary player through the table.
- [x] Route local multiplayer/debug bots through the table.
- [x] Update player control lookup to get inputs by controlled ent/player id.
- [x] Replace `state.player_vid` assumptions with player-slot or primary-player
  helpers where needed.
  - No `state.player_vid` member remains; `controlled_ent_vid` remains as a
    camera/audio/debug selected ent, not as player ident.
- [x] Keep camera/audio listener semantics working for one local player.
- [x] Keep existing gameplay behavior unchanged in offline play.
  - Validation so far: `cmake --build build --target splonks-cpp -j 8`,
    `build/splonks-cpp --check-state-fingerprint-smoke`, and
    `build/splonks-cpp --check-state-equality-smoke` pass. Manual feel test is
    still useful after more lockstep cleanup.

Exit gate: single-player/offline build works, local player controls feel the
same, and no networking code is needed to step multiple local player inputs.

### Phase 2: Det State Fingerprint And Same-Process Replay

Goal: prove same initial state plus same inputs gives same gameplay state before
we touch real networking.

- [x] Define `ComputeGameplayDeterminismFingerprint(State&)`.
  Det replay uses this stricter hash, including the det RNG
  cursor. Canonical/network equality fingerprints intentionally ignore the RNG
  cursor because old result-application smoke tests compare resulting world
  state, not replayed draw history.
- [x] Include gameplay-affecting state: stage ident, tiles, rotations,
  fluids, ents, player slots, tools, effects, RNG streams, progression, and
  stage frame.
- [x] Exclude local-only pres/debug state: camera, audio emitters,
  particles if purely visual, debug UI, net transport queues.
- [x] Add scripted input sequences for one player.
- [x] Add scripted input sequences for multiple local players.
- [x] Add a CLI smoke that runs a stage for 1,000+ frames, records inputs/hashes,
  resets to the same seed, replays inputs, and verifies every hash.
- [x] Add an initial CLI smoke that replays a 180-frame movement/jump script
  with first-difference diagnostics.
- [x] Extend the CLI smoke with a 240-frame two-local-player replay script.
- [x] Add diagnostics that print first divergent frame and a small state summary.

Exit gate: det same-process replay passes for at least movement,
jump/climb/hang, tool use, pickup/throw, tile break, explosion, fluid, shop, and
stage transition scenarios. Current broad replay covers movement, pickup/drop,
attack, bomb, rope, spawned ents, spikes, ladder, containers, and enemies.
Still add explicit fluid, shop, and stage-transition scripted scenarios.

### Phase 3: Determinism Audit And Cleanup

Goal: remove or isolate obvious nondeterminism before network lockstep hides it.

- [ ] Audit gameplay use of wall-clock time, render frame timing, SDL state, mouse
  state, OS state, pointer addresses, unordered iteration, and random device.
- [ ] Route gameplay randomness through det RNG stored in `State`.
  `State::drng` is the det RNG stream used by simulation. It must be
  state-owned, not `base_seed + frame`, because a
  frame may perform multiple random draws and skipped/double draws must show up
  as hash divergence. It is now fingerprinted by det replay and seeded
  from the stage seed.
- [ ] Ensure ent allocation and iteration order are det under the
  same inputs.
- [ ] Ensure fluid updates are det under the same inputs.
- [ ] Classify particles/lights/audio as gameplay or pres.
- [ ] Isolate pres-only side effects so replay can suppress or ignore
  them.
- [ ] Keep debug/editor tools outside det gameplay unless explicitly
  part of the scripted test.

Exit gate: det replay stays green after enabling broad gameplay
coverage.

#### DRNG Conversion Checklist

Convert gameplay-affecting files to `State::drng`. Leave stagegen on the
stage seed path until stagegen itself is moved to explicit streams. Leave
pres-only files on local RNG unless the result affects gameplay state.

- [x] `src/ents/snake.cpp`
- [x] `src/ents/caveman.cpp`
- [x] `src/ents/monkey.cpp`
- [x] `src/ents/spider.cpp`
- [x] `src/ents/pot.cpp`
- [x] `src/ents/box.cpp`
- [x] `src/ents/dice.cpp`
- [x] `src/ents/mattock.cpp` gameplay break chance; dig particles remain
  pres RNG.
- [x] `src/ents/sac_altar.cpp` gameplay reward choice/velocity; sacrifice
  particles remain pres RNG.
- [x] `src/ents/baseball_bat.cpp` kill sound variation remains
  pres RNG.
- [x] `src/ents/bat.cpp` wake sound variation remains pres RNG.
- [x] `src/ents/block.cpp` particles remain pres RNG.
- [x] `src/ents/boulder.cpp` particles remain pres RNG.
- [x] `src/ents/chest.cpp` gameplay loot/trap rolls; sparkles remain
  pres RNG.
- [x] `src/ents/cobra.cpp` gameplay AI/spit cooldown; venom particles remain
  pres RNG.
- [x] `src/ents/craps_table.cpp`
- [x] `src/ents/door.cpp` particles remain pres RNG.
- [x] `src/ents/mantrap.cpp`
- [x] `src/ents/moving_platform.cpp` particles remain pres RNG.
- [x] `src/ents/skeleton.cpp` loose skull velocity; break/death particles
  remain pres RNG.
- [x] `src/on_damage_effects.cpp` particles remain pres RNG.
- [x] `src/step.cpp` debug local-player bot RNG is local debug input
  generation; exclude bots from lockstep determinism unless their scripted input
  is explicitly recorded.
- [x] `src/ents/common/explosion.cpp`, `src/effects/treasure_pickup.cpp`,
  `src/ents/jetpack.cpp`, `src/ents/pistol.cpp`,
  `src/ents/web_cannon.cpp`, and `src/pres_commands.cpp` use RNG for
  particles/transient pres only.
- [x] `src/render/tiles_and_ents.cpp` and `src/sprite.cpp` use RNG for
  render shake/random-frame pres only.
- [x] Debug-stage RNG remains debug-only and is outside lockstep sessions unless
  the resulting stage is snapshotted or generated from explicit seed streams.
- [ ] Stagegen/helper RNG in `src/stage.cpp`, `src/room.cpp`, `src/tile.cpp`,
  and `src/stage_gen/**` remains on the explicit stage-seed path for now; move
  it to named stagegen DRNG streams when stagegen determinism becomes the active
  task.

### Phase 4: Strip Host Mutation Replication From Gameplay

Goal: stop the codebase from being hybrid. Gameplay should no longer ask whether
it is host/peer before doing ordinary gameplay.

- [x] Remove peer action request/apply branches from content paths.
- [x] Remove `HasLocalAuthority...` checks from active ent/gameplay logic
  unless they are replaced by lockstep input ownership.
- [x] Remove `allow_remote_player_target` style damage exceptions created for
  host-authority networking.
- [x] Remove ent/tile/run/fluid/stage-light/pres replication emits from normal
  gameplay.
- [x] Skip old ordered mutation-message apply/drain while input lockstep is
  active.
- [x] Delete or quarantine old `world_ops` networking seams. Keep only gameplay
  helper pieces that remain useful without networking.
- [x] Rename remaining live local helper names that still say `Request` but no longer
  cross the network.
- [x] Delete old network smoke tests that only validate mutation-message lanes.
- [ ] Keep transport/fuzzer code only if it is useful for input packets.
- [ ] Remove ent/spec/debug baggage added only for the old
  host-authoritative model: replica stepping, local prediction flags,
  replicated runtime flag helpers, and mutation-message-only state fields.
  - Removed unused ent spec `replica_logic` / `step_as_replica` fields
    and their content initializers.
  - Renamed the stale carry helper
    `ReleaseEntFromHolderAndEmitNetwork` to
    `ReleaseEntFromHolderIfAttached`.
- [ ] Keep state fingerprint/replay code if it supports determinism.

Implementation note: `world_ops::{SpawnEnt,DeactivateEnt,SetForegroundTile}`,
stage lighting, stage fluids, and carry/damage/tool/tile-break content paths are
currently local det helpers. Old no-op patch/pres shims were
removed rather than kept as compatibility wrappers.

Exit gate: ordinary gameplay code has no host/peer mutation branches, and
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
- `./build/splonks-cpp --check-det-replay-smoke`
- `./build/splonks-cpp --check-input-lockstep-smoke`

Exit gate: two or more fake peers can run scripted gameplay for thousands of
frames with matching hashes while exchanging only input and control packets.

### Phase 6: Real Two-Process Input Lockstep

Goal: boot two game windows and play with delay-based lockstep.

- [x] Reuse or replace the current UDP transport for input packets.
- [x] Reuse the existing impairment/profile controls where possible:
  same-house/same-city/same-state/Texas-to-California/California-to-Florida/
  Texas-to-Japan latency, jitter, loss, duplicate, and reorder settings should
  feed the lockstep input transport/fuzzer instead of the retired authoritative
  mutation transport.
  ImGui Debug Network controls and `splonksctl net fuzzer ...` both write the
  lockstep transport fuzzer config.
- [ ] Remove or hide live controls that only make sense for the old
  authoritative snapshot/event lanes once lockstep owns live networking.
- [x] Add lobby start barrier: stage seed, player list, stage id, start frame,
  input delay.
- [ ] Disable active-stage late join initially; peers join before start or wait
  for next stage.
- [x] Add debug UI: local frame, remote input buffer depth, blocked frame count,
  hash, last agreed frame, packet loss/jitter profile.
  Debug Network and `splonksctl net` expose role, lockstep frame, local-input
  frame, input delay, confirmed hash/frame, input buffer totals, remote buffer
  depth, predicted record count, input-wait block count, and fuzzer loss/jitter
  profile.
- [x] Add a clean debug input/script lane for live validation.
  Implemented through `splonksctl input`, which writes a temporary
  `DebugInputOverrideState` consumed by normal input capture. This is not a
  world mutation/admin command.
- [ ] Keep current multiplayer pair launcher if useful.
- [x] Playtest same-machine two-window lockstep.
  Basic launch/query verified host and peer connect with the same stage seed,
  lockstep enabled, and matching player/link topology through
  `scripts/run_multiplayer_pair_i3.sh` plus `scripts/splonksctl`.
  Live client-side carry of the host player was verified by driving peer input
  through `splonksctl input`; both host and peer reported the same carry links.
  Stage-exit carry is covered by `--check-input-lockstep-smoke`
  carry-transition coverage.
- [x] Validate artificial latency profiles in a live two-process session.
  `splonksctl net fuzzer preset tx-japan` applied to host and peer, debug input
  advanced both players under ~320ms measured RTT, and rollback counters advanced
  without stalling.
- [x] Remove old authoritative ent/network baggage once lockstep is live:
  content should stop carrying special host/peer terms, stale
  request/result paths, and per-item replication hooks that are no longer part
  of the model.
  Current active source has no ent/world-op authority branches or old
  message/snapshot/repair lanes; network ent-link debug vocabulary now uses
  input ownership terminology.

Exit gate: two local windows can play a real stage without desync, using input
lockstep only.

### Phase 7: Rollback

Goal: reduce input-delay feel while keeping det correctness.

- [x] Add save-state ring for recent gameplay frames.
- [x] Store input history for every player.
- [x] Predict missing remote inputs. Initial policy: repeat last input.
- [x] On late real input mismatch, restore the mismatch frame and resimulate to
  current frame.
- [x] Add rollback debug: rollback count, last rollback span, max rollback span,
  last replay ms, and retained snapshot count.
- [x] Add fuzzer/smoke coverage for rollback under delayed, duplicated, and
  reordered remote inputs.
- [x] Suppress duplicate pres/audio during resimulation where needed.
- [x] Add richer rollback debug: prediction miss count, late predicted-input
  match count, last miss span, and average replay ms.
- [x] Add confirmed frame, per-second rollback count, and prediction miss rate.
- [x] Validate artificial latency profile plumbing with live host/peer and
  `splonksctl`.
- [ ] Human-playtest high-latency feel and tune default delay/prediction.

Exit gate: high-latency profile remains locally responsive and hashes converge.

### Phase 8: Late Join / Reconnect

Goal: add player/session usability once the base model works.

- [ ] Decide first production policy: wait until next stage, replay from stage
  start, or snapshot current gameplay state.
- [ ] If using snapshots, define det gameplay snapshot format.
- [ ] Reconnect to retained player slot at safe sync point.
- [ ] Add tests for disconnect/reconnect.

Exit gate: reconnect policy is explicit and does not compromise det
frame stepping.

## Non-Negotiable Rules

- Do not add new item-specific netcode while this experiment is active.
- Do not keep both architectures half-wired in gameplay content.
- If a gameplay system needs a network branch under lockstep, treat that as a
  design smell unless it is input capture, det seeding, save/restore,
  or pres duplicate suppression.
- Do not optimize rollback before det replay is proven.
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
