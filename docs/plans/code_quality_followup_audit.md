# Code Quality Follow-Up Audit

## Purpose

This is the next pass after `code_quality_abstraction_audit.md`,
`fxp_abstraction_cleanup.md`, and `detcleanup.md`.

The previous pass removed the obvious post-gfxp clutter: `Sim` names that only
meant fixed-point, duplicate fixed/render helper pairs, one-line render
wrappers, duplicate 8-way aim code, and repeated render wrapping helpers.

This follow-up looks for the same class of problem elsewhere:

- duplicated local utility code with identical behavior,
- fixed -> float -> fixed detours,
- wrappers that only rename a conversion or assignment,
- stale names such as `Legacy` or `Sim` where the code is current,
- deterministic math helpers copied into feature files instead of living in one
  shared place,
- helpers that imply a domain boundary without adding real policy.

The point is not to eliminate every helper. Keep helpers that encode policy,
ownership, validation, or a shared game concept. Remove or collapse helpers that
only rename another operation.

## Confirmed Cleanup Targets

### Duplicate Coordinate Wrapping

Local `WrapCoordinate(...)` helpers appear in:

- `src/stage_bounds.cpp`
- `src/stage_shake.cpp`
- `src/render/tiles_and_ents.cpp`

They implement the same positive modulo behavior for wrapped stage coordinates.
The exact invalid-size fallback currently differs slightly between files, so
the cleanup should centralize the helper and make call-site fallback explicit.

Plan:

- [x] Add one shared integer wrap helper near `FloorDiv(...)` in
  `src/math_types.hpp`, probably named `PositiveModulo(...)` or
  `WrapIndex(...)`.
- [x] Replace `Stage::WrapTileCoord(...)` / `Stage::WrapWorldCoord(...)` local
  wrapping with the shared helper.
- [x] Replace tile-shake and render tile wrapping local copies.
- [x] Keep out-of-range handling at the caller when a zero/negative size means
  something different from normal modulo.

### Duplicate Render Color Helpers

Render color utility code is duplicated between:

- `src/render/ents.cpp`
- `src/render/tiles_and_ents.cpp`

Examples:

- `ClampRenderColor(...)`
- `MaxRenderColor(...)`
- `LerpRenderColor(...)`

These are render-side float color helpers, so they do not belong in gameplay
math. They should still be shared instead of copied between render modules.

Plan:

- [x] Add a small render color helper, for example `src/render/color.hpp`.
- [x] Move `ClampRenderColor(...)`, `MaxRenderColor(...)`, and
  `LerpRenderColor(...)` there.
- [x] Replace local copies in render files.
- [x] Keep these helpers render-only; do not use them for authoritative light or
  simulation state.

### Fixed -> Float -> Fixed Detours

There are still places that convert fixed-point state to `FVec2` and
immediately back to `FxVec2`:

- `src/debug/debug_stage_scenarios.cpp`
  - held rock/mattock placement uses
    `ToFxVec2(ToFVec2(holder->GetCenter()) + FVec2::New(...))`.
- `src/debug/playback_ui_ents.cpp`
  - attached spawn placement uses
    `ToFxVec2(ToFVec2(player->GetCenter()))`.
- `src/debug/debug_stage_boulder_sacrifice.cpp`
  - idol position is converted through `FVec2` before becoming `IVec2`.
- `src/ents/boulder.cpp`
  - boulder position has the same `ToIVec2(ToFVec2(...))` shape.

These are mostly debug/tooling paths, but they are still needless boundary
noise. They make fixed-point feel like a special case instead of the normal
gameplay coordinate language.

Plan:

- [x] Replace fixed -> float -> fixed position offsets with fixed math such as
  `holder->GetCenter() + FxVec2::from_int(4, 1)`.
- [x] Replace `ToIVec2(ToFVec2(fx_vec))` with an explicit fixed-to-pixel helper
  such as `ToIVec2Trunc(...)` or `ToIVec2Round(...)`.
- [x] Choose trunc vs round per call site; do not blindly replace semantics.

### Stale `Sim` Names In Fingerprint Internals

`src/state_fingerprint.cpp` still has local lambdas:

- `read_sim_scalar_grid`
- `read_sim_vec2_grid`

They read `FxScalar` / `FxVec2` grids. This is the same stale naming shape we
cleaned up elsewhere.

Plan:

- [x] Rename them to `read_fx_scalar_grid` and `read_fx_vec2_grid`.
- [x] Leave `SimSnapshot` names alone; those refer to snapshot domain objects,
  not fixed-point types.

### Entity Spec Construction Wrappers

`src/ent/spec.hpp` contains one-line construction wrappers:

- `EntSpecSize(float, float) -> FxVec2`
- `EntSpecSize(FVec2) -> FxVec2`
- `EntSpecCounter(float) -> FxScalar`

These exist at the authored-data boundary, so they are less clearly wrong than
the old `GetRenderPos()` wrappers. Still, they are wrappers over `ToFxVec2(...)`
and `ToFxScalar(...)`, and they hide the fact that entity spec fields are fixed
state.

Plan:

- [x] Decide whether `EntSpecSize(...)` is a useful authored-spec DSL or just a
  rename wrapper.
- [x] Strongly consider replacing `EntSpecCounter(...)` with direct
  `ToFxScalar(...)` or fixed constants, because counters are not specifically an
  authored-size concept.
- [x] If `EntSpecSize(...)` stays, document that it is an authored-data boundary
  helper, not a general conversion wrapper.

Decision:

- `EntSpecCounter(...)` was removed and call sites now use `ToFxScalar(...)`.
- `EntSpecSize(...)` stays because entity specs read as authored pixel-size
  tables. Treat it as an authored-data helper, not a general conversion helper.

### Duplicated Deterministic Integer Math

Several files still carry local deterministic math helpers:

- `src/ents/moving_platform.cpp`
  - `RoundRatio(...)`
  - `PositiveModulo(...)`
- `src/ents/ball_and_chain.cpp`
  - `RoundRatio(...)`
  - `IntSqrtFloor(...)`
  - `IntSqrtRound(...)`
- `src/math_types.hpp`
  - already has `IntegerSqrtFloor(...)` and `DivRoundNearest(...)`.

Some of this duplication is just local naming around raw fixed math. It should
be centralized enough that new deterministic mechanics do not copy/paste their
own rounding policy.

Plan:

- [x] Move a signed round-ratio helper to shared math if it matches the current
  behavior needed by moving platforms and ball-and-chain.
- [x] Replace `moving_platform.cpp` local `PositiveModulo(...)` with the shared
  coordinate/index wrapping helper from the coordinate wrapping cleanup.
- [x] Reuse `IntegerSqrtFloor(...)` where behavior matches.
- [x] Keep `IntSqrtRound(...)` local or promote it only if another mechanic
  needs the same rounded integer sqrt policy.

Decision:

- Existing `DivRoundNearest(...)` already supplied the shared signed ratio
  behavior, so local `RoundRatio(...)` copies were removed instead of adding a
  second shared name.
- `IntSqrtRound(...)` remains local because only ball-and-chain currently needs
  rounded integer sqrt.

### Spec Restore One-Function-Per-Field API

`src/ent/spec_restore.cpp` and `.hpp` define many one-line helpers like:

- `RestoreEntHasPhysicsFromSpec(...)`
- `RestoreEntCanCollideFromSpec(...)`
- `RestoreEntDrawLayerFromSpec(...)`

Only a small subset appears to be used from carry/hold logic today. This is a
different smell than the gfxp wrappers, but it has the same shape: many helper
names that mostly assign one field from one source.

Plan:

- [x] Audit which restore helpers are actually used.
- [x] Keep high-level restore helpers that encode policy, such as restoring the
  exact fields that carrying changes.
- [x] Consider replacing one-field public helpers with direct assignments from
  `GetEntSpec(...)` where that is clearer.
- [x] Avoid expanding this file with more one-field wrappers unless the helper
  name describes a real gameplay policy.

Decision:

- The public one-field restore helpers were collapsed into grouped policy
  helpers:
  - `RestoreEntDetachedCarryStateFromSpec(...)`
  - `RestoreEntStageEntryStateFromSpec(...)`
  - `RestoreEntStoneStateFromSpec(...)`
  - `RestoreEntRuntimeCallbacksFromSpec(...)`

### Current Input Function Named Legacy

`src/inputs.cpp` has `PollLegacyPlayingInputSnapshot(...)`.

It is currently the normal keyboard/gamepad polling path used when external
input frames are disabled. If that path is no longer a compatibility fallback,
the name is misleading.

Plan:

- [x] Rename it to `PollDirectPlayingInputSnapshot(...)` or
  `PollLocalPlayingInputSnapshot(...)` if it is the current direct-input path.
- [x] Keep `Legacy` only if there is a concrete old input path it refers to.

## Candidates To Inspect Carefully

### `LengthDeterministic(...)` And `NormalizeOrZeroDeterministic(...)`

These helpers still operate on `FVec2` but quantize internally. Current call
sites include render/audio/debug-adjacent paths plus some gameplay/presentation
commands.

Do not remove these by regex. Decide per call site whether the code should be:

- pure fixed-point gameplay math,
- render/audio/debug float math,
- or a legitimate authored/presentation boundary that needs quantized float
  behavior.

Decision:

- Left intact for this pass. Current uses are render/audio/presentation
  adjacent, not obvious authoritative fixed-point state that should be
  converted mechanically.

### `FormatHudInt(...)`

`src/ents/bow.cpp` has a tiny `FormatHudInt(...)` helper. There are similar HUD
count formatters in `src/effects/specs.cpp`.

This is not urgent. A shared HUD formatting helper may be reasonable later, but
do not create an abstraction unless more weapons/effects need the same display
policy.

Decision:

- Left local. There is not enough repeated weapon HUD formatting policy to
  justify a new shared abstraction.

### `EntSpecSize(...)`

This is listed above as a target because it is a one-line wrapper. It may still
be worth keeping if we decide entity spec tables should read like authored
pixel data rather than fixed math. If kept, it should be named and documented as
an authored-data helper.

Decision:

- Kept as authored-size syntax. Do not use it as a general `FVec2` ->
  `FxVec2` wrapper outside entity spec declarations.

## Things To Leave Alone

- Serialization `Read*` / `Write*` helpers in playback and network protocol
  code. They encode binary format policy and are not wrapper clutter.
- `SimSnapshot` names. They refer to simulation snapshots, not fixed-point
  vectors.
- `ToFVec2(...)`, `ToFAABB(...)`, `ToFxVec2(...)`, `ToFxScalar(...)`,
  `ToIVec2Round(...)`, and `ToIVec2Trunc(...)`. These are boundary
  conversion helpers. The problem is wrapping them in one-line functions that
  only rename the conversion.
- Render, audio, UI, debug overlay, and authored-stage APIs that naturally deal
  in `FVec2` or `FAABB`.
- `GetLooseAnimId(...)` / `GetPullAnimId(...)` style helpers in weapons. They
  encode small but real weapon animation policy.

## Suggested Audit Commands

```bash
rg -n "WrapCoordinate|PositiveModulo|RoundRatio|IntSqrt|IntegerSqrt|LengthDeterministic|NormalizeOrZeroDeterministic" src --glob '*.{hpp,cpp}'
rg -n "ToFxVec2\\(ToFVec2|ToIVec2\\(ToFVec2|read_sim_|PollLegacy|EntSpecCounter|EntSpecSize" src --glob '*.{hpp,cpp}'
rg -n "ClampRenderColor|MaxRenderColor|LerpRenderColor" src/render --glob '*.{hpp,cpp}'
rg -n "RestoreEnt[A-Za-z0-9_]+FromSpec" src/ent src/ents --glob '*.{hpp,cpp}'
```

## Validation

For a cleanup pass that edits code:

- [x] `git diff --check`
- [x] `./scripts/build.sh`
- [x] `SPLONKS_PRESET=release ./scripts/dev_smoke.sh`
- [x] `./build/splonks-cpp --check-state-equality-smoke --project-root ...`
- [x] `./build/splonks-cpp --check-gameplay-snapshot-callback-rebind-smoke --project-root ...`
- [x] `./build/splonks-cpp --check-network-fresh-reload-ownership-smoke --project-root ...`
- [x] `./build/splonks-cpp --check-det-replay-smoke --project-root ...`
- [x] `./build/splonks-cpp --check-join-barrier-protocol-smoke --project-root ...`
- [x] `./build/splonks-cpp --check-join-barrier-next-stage-restart-smoke --project-root ...`

If the pass touches authoritative gameplay math, also do at least one local
host/client start and stage-transition sanity check before pushing.

Validation note:

- The dedicated join-barrier next-stage restart and deterministic stage-transition
  replay smokes cover the non-interactive stage-transition sanity path for this
  cleanup.
- `--check-input-lockstep-smoke` was also attempted, but the monolithic smoke
  bundles many long-running fuzzer/profile cases and was stopped after several
  minutes with no output. The smaller targeted smokes above passed.
