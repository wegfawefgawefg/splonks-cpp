# Physics Port From `splonks-old`

## Status

This backport worked.

The current `splonks-cpp` build now uses the old gameplay physics semantics from
`splonks-old`, and runtime playtesting confirmed that the physical feel is back
where it should be.

This document is now a record of what was wrong, what got restored, and what
still remains outside the core physics stack.

## Canonical Source

The canonical gameplay physics source is the old Rust build checked out at:

- `/home/vega/Coding/GameDev/Splonks/splonks-old`
- commit `88c8beb` (`sprites infra redesign`)

That build is the current reference for:

- player movement feel
- jump / grounded behavior
- tile collision snap behavior
- entity collision behavior
- hanging / climbing behavior

The current `splonks-cpp` physics inherited late-Rust semantics from the 2024
refactor era, not the known-good 2023 behavior. That mismatch is what this
backport corrected.

## What Was Wrong

These were the important drifts from the known-good Rust path:

1. Fixed-step drift

- `src/step.cpp` runs a fixed-step accumulator, but passes frame `dt` into
  gameplay instead of `kTimestep`.

2. Shared integrator drift

- `src/entities/common.cpp`
- `PrePartialEulerStep` uses `vel += acc * dt`
- `ApplyGravity` uses `gravity * dt`
- `PostPartialEulerStep` uses `pos += vel * dt`

The old gameplay used fixed-tick integer-ish semantics:

- `vel += acc`
- `acc.y += gravity`
- `pos += vel.as_ivec2()`

3. Bounds semantics drift

- `src/entity.cpp`
- current top-left / center / foot bounds are effectively exclusive
- known-good Rust used inclusive bottom-right semantics for collision queries

This affects:

- grounded checks
- tile collision
- entity collision
- feet / stomp / hang probes

4. Collision regression

- `src/entities/common.cpp`
- current entity Y resolution still does `pos.y += min_displacement.x`

That is a real bug and causes overlap / sticking artifacts.

5. Player behavior slices removed or disabled

- `src/entities/common.cpp`
  - `HangHandsStep` stubbed out
  - climbing logic stripped out of `JumpingAndClimbingStep`
- `src/entities/player.cpp`
  - old friction branch not preserved
  - jump-on-heads logic missing
  - other carry / push / use logic also missing

## What Was Restored

The working fix was not “tune the current physics until it feels close.”

The working fix was to port the known-good behavior slices directly from
`splonks-old`.

Restored behavior:

1. Old fixed-step semantics in `src/step.cpp`
   - fixed-step gameplay now advances on `kTimestep`
2. Old shared integration behavior in `src/entities/common.cpp`
   - fixed-tick velocity / gravity / integerized position stepping
3. Old top-left / inclusive bounds semantics in `src/entity.cpp`
   - feet, grounded, hang, and collision probes now match the old Rust model
4. Old collision fixes in `src/entities/common.cpp`
   - restored practical snap/query behavior
   - fixed the bad Y displacement bug
5. Old player physical behavior in `src/entities/player.cpp`
   - hanghands
   - climb/jump behavior
   - grounded-vs-air friction behavior
   - stomp / jump-on-heads behavior
6. Old control-facing entity flags in `src/systems/controls.cpp`
   - player physics once again sees the same intent flags the old Rust logic
     expected

## Port Strategy Used

The important lesson here is that a partial semantic rollback was not enough.

Restoring only `dt` behavior did improve fall speeds, but collision still felt
wrong. The successful fix required porting whole behavior slices together:

1. entity geometry / bounds conventions
2. fixed-step helper semantics
3. collision semantics
4. grounded / hang / climb / jump behavior
5. the player behavior slices that physically depend on those systems

## Target Files

Primary files:

- `src/step.cpp`
- `src/entity_core_types.hpp`
- `src/entity.hpp`
- `src/entity.cpp`
- `src/utils.hpp`
- `src/utils.cpp`
- `src/entities/common.hpp`
- `src/entities/common.cpp`
- `src/entities/player.cpp`
- `src/systems/controls.cpp`

Reference files in the canonical Rust source:

- `splonks-old/src/step.rs`
- `splonks-old/src/entity.rs`
- `splonks-old/src/utils.rs`
- `splonks-old/src/entities/common.rs`
- `splonks-old/src/entities/player.rs`
- `splonks-old/src/controls.rs`

## Remaining Scope

The immediate physics goal is complete enough for the project to move forward.

The remaining work is mostly outside the core collision stack:

- broader carry / use / push behavior
- the rest of the literal entity-module port
- render/backend parity cleanup
- whatever intentional design changes happen later in C++

If physics feel regresses again, `splonks-old` at `88c8beb` remains the
canonical comparison point.
