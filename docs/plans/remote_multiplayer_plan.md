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
- A network peer/process may own multiple local `PlayerId`s. The architecture
  must support shapes like two players on one client and three players on
  another, not just one player per connection.
- Every frame has a complete input record for every active player.
- Input packets batch all local `PlayerId -> PlayerInputFrame` records owned by
  that process for the frame.
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
3. Finish deleting/quarantining leftover coordinator-authoritative mutation
   replication paths, including fluids and obsolete packet smokes.
4. Add networked delay-based input lockstep.
5. Add rollback after deterministic lockstep is proven.

See `docs/plans/input_lockstep_rollback_plan.md` for the detailed checklist.

## Current Branch Goal

On `net-lockstep-experiment`, the immediate target is full delay-based input
lockstep, not rollback yet:

- Fake/headless lockstep first, then real two-process UDP.
- Any process may own multiple local players.
- Host/client can connect, carry each other, and transition stages together.
- Gameplay/content code should become network-agnostic again.
- Old coordinator-authoritative mutation sync should be deleted or quarantined,
  not kept half-wired.
- Commit after meaningful chunks and keep the active plan updated with known
  gaps.

## Next Goal Handoff

When resuming this work under `/goal`, continue from the current branch instead
of resetting to an older commit. The player-slot/input-table refactors are part
of the new lockstep foundation and should be kept.

Working rules:

- Finish delay-based input lockstep first. Do not start rollback until live and
  fake lockstep are deterministic and playable.
- Prefer deleting or quarantining old coordinator-authoritative mutation code
  over adapting content to another hybrid model.
- Keep ordinary gameplay/entity code local and network-agnostic. If content code
  needs to ask whether it is host, peer, coordinator, or authoritative, that is a
  cleanup target.
- Use deterministic state and `State::drng` for gameplay randomness. Presentation
  randomness can stay local if it does not affect gameplay state.
- Commit after each meaningful chunk: deterministic cleanup, live transport,
  stage transition, carry/player interaction validation, old-cruft removal, or
  test expansion.

Required validation before calling the goal complete:

- Build passes.
- Existing state equality, deterministic replay, and input-lockstep smokes pass.
- Two live windows can connect into the same stage using input lockstep.
- The peer can carry the host player without desync.
- Both players can transition stages together.
- Live lockstep runs with no ordered mutation backlog from the old authoritative
  replication lanes.
- Remaining old networking code is either removed, hidden behind inactive legacy
  tests, or documented as intentionally retained transport/debug scaffolding.
