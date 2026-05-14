# Lockstep Human Playtest Checklist

Purpose: capture the remaining non-automated evidence for the input-lockstep
rollback plan. Automated harnesses prove convergence and perf budgets; this
checklist proves control feel and visible gameplay behavior.

## Setup

1. Build latest `net-lockstep-experiment`.
2. Launch a local pair with `scripts/run_multiplayer_pair_i3.sh`.
3. Keep `Debug: Network` available on both windows.
4. Before each profile, optionally run:
   `scripts/validate_lockstep_live.py --profile <profile> --report-json`
   against the live windows to confirm the session starts clean.
5. While manually playing each profile, run:
   `scripts/record_lockstep_playtest.py --profile <profile> --duration 300`
   to capture live telemetry into `logs/lockstep_playtest_samples.jsonl` and
   `logs/lockstep_playtest_summary.json`.

Record for each profile:

- Profile name.
- Input delay frames and rollback window.
- Rollbacks/sec, prediction misses, skipped inputs.
- Latest confirmed hash frame and hash mismatch count.
- Recovery mode counts, snapshot-resync active samples, and join-barrier active
  samples.
- Smoothed multiplayer sim, hash, and rollback replay ms.
- Any repro steps for visible artifacts.

The recorder summary should stay `ok: true`. If it reports hash mismatches,
fatal desync, or control-server errors, treat the profile as failed even if the
visible playtest seemed acceptable.
If `snapshot_resync_active_samples` grows during an already-started playtest,
record it as a failure unless it is explained by an intentional late join.

## Profiles

Run these in order:

1. `same-house`
2. `tx-ca`
3. `tx-japan`

Optional broad pass:

1. `same-city`
2. `same-state`
3. `ca-fl`
4. `us-cross-country`

## Core Flow

For each required profile:

1. Move both players independently for at least one minute.
2. Run, stop, short-hop, full-hop, and jump over one-tile ledges.
3. Grab and release ledges from both sides.
4. Climb ladders/ropes, then jump off and regrab.
5. Pick up, carry, drop, and throw another player horizontally and vertically.
6. Pick up, drop, and throw rocks, pots, arrows, and stunned/dead enemies.
7. Use ropes, bombs/grenades, bow/gun, mattock, knife/bat, jetpack/cape, and shop buying.
8. Break tiles with tools and explosions.
9. Kill or damage at least one enemy with a thrown item and one weapon.
10. Die with one player while the other continues.
11. Respawn through the configured respawn mode.
12. Exit the stage with one alive player and confirm every player transitions.
13. Repeat a stage transition while carrying a player or item.

## Pass Criteria

The profile passes if:

- No gameplay hash mismatch occurs.
- No fatal desync or snapshot catchup loop occurs.
- No peer freezes or remains stuck in a join/loading/game-over state.
- Stage transitions finish on every process.
- Local movement feels responsive enough to tune, not redesign.
- Jumping, ledge grab, climbing, carry/throw, and tool use have no persistent
  correction loops.
- High-latency artifacts are temporary and explainable by rollback/prediction,
  not durable state divergence.

## Failure Template

Use this format for each failure:

```text
Profile:
Players/processes:
Input delay / rollback:
What happened:
Expected:
Minimal repro:
Latest confirmed hash frame:
Hash mismatch count:
Recovery mode:
Perf: sim/hash/rollback ms:
Notes/screenshots:
```

## Completion Rule

Do not check off the human-playtest gates in
`docs/plans/input_lockstep_rollback_plan.md` until the required profiles pass
or failures are fixed and retested.
