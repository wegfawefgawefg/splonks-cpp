# Remote Multiplayer Plan

Status: active plan is input lockstep first, then rollback.

Source of truth: `docs/plans/input_lockstep_rollback_plan.md`.

The previous coordinator-authoritative / Terraria-style mutation replication
plan is archived under `docs/legacy_authoritative_networking/`. Do not use those
legacy docs as implementation targets.

## Current Direction

Remote multiplayer should be built around deterministic simulation from shared
inputs:

- Every player has a stable `PlayerId`.
- Every frame has a complete input record for every active player.
- Gameplay code reads player inputs from a frame input table.
- All peers start from the same stage seed/state and step the same deterministic
  gameplay code.
- Networking delivers input frames, frame agreement, hashes, and eventually
  rollback support.
- Gameplay content should not know whether an input came from local hardware or
  a remote peer.

## Why

The mutation-replication model made normal content code responsible for too many
networking facts: authority, prediction, coordinator requests, repair snapshots,
presentation commands, and per-category state patches. That makes new items,
entities, fluids, shops, traps, and mods fragile.

Input lockstep/rollback has a harder determinism requirement, but it moves
networking out of content code. If the simulation is deterministic, modded
content can work in multiplayer without new protocol fields, as long as all peers
load the same content and use deterministic gameplay logic.

## Near-Term Work

1. Route player controls through `PlayerId -> PlayerInputFrame` tables.
2. Prove same-process deterministic replay from recorded inputs.
3. Add networked delay-based input lockstep.
4. Delete coordinator-authoritative mutation replication paths from gameplay.
5. Add rollback after deterministic lockstep is proven.

See `docs/plans/input_lockstep_rollback_plan.md` for the detailed checklist.

