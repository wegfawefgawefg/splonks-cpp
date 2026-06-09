# Fx/FVec2 Naming Cleanup Plan

## Goal

Remove the noisy migration vocabulary that has grown around fixed-point
determinism. Gameplay code should read like normal game code again, while
float/render boundaries remain explicit.

Use two vector spellings:

- `FxVec2`: fixed-point authoritative/gameplay vector.
- `FVec2`: float presentation vector.

The same idea applies to rectangles and scalar conversions:

- `FxAABB`: fixed-point authoritative/gameplay rectangle.
- `FAABB` or `FRect`: float presentation rectangle. Pick one spelling before
  the rename. `FAABB` is the most mechanical continuation from current
  `RenderAABB`.
- `FxScalar`: fixed-point scalar if we decide `Scalar` is too vague after the
  bulk rename.

This plan is intentionally a rename/collapse pass. It should not change
gameplay behavior, lockstep protocol semantics, or rendering behavior.

This should be executed with broad scripted replacements per lane, not by
hand-converting individual call sites. Hand work is for fixing compile errors,
resolving rare name conflicts, and reviewing boundary mistakes after each
scripted sweep.

## Naming Rules

Default gameplay lane:

- Fixed-point is authoritative.
- Gameplay functions should take and return `FxVec2` / `FxAABB`.
- Names inside gameplay should not say `Sim` unless they are specifically
  about simulation phase/state machines rather than numeric type.
- Entity geometry helpers should be short:
  - `GetSimCenter()` -> `GetCenter()`
  - `SetSimCenter(...)` -> `SetCenter(...)`
  - `GetSimPos()` -> `GetPos()` or direct `ent.pos`
  - `SetSimPos(...)` -> `SetPos(...)` or direct `ent.pos = ...`
  - `GetSimAABB()` -> `GetAABB()`
  - `GetSimFeet()` -> `GetFeet()`
  - `GetSimGroundProbe()` -> `GetGroundProbe()`

Presentation lane:

- Float vectors are explicit as `FVec2`.
- Float rectangles are explicit as `FAABB` or `FRect`.
- Render/debug/audio/UI/particle code may use `FVec2`.
- Presentation helpers may keep `Render` in their names when the helper is
  genuinely render-specific, e.g. sprite draw bounds, camera transforms, debug
  overlays.

Boundary conversions:

- Prefer short conversion names:
  - `ToFVec2(fx)` instead of `sim::ToRenderVec2(fx)`
  - `ToFxVec2(f)` instead of `sim::ToSimVec2(f)`
  - `ToFloat(fx)` or `fx.to_float()` instead of `sim::ToRenderScalar(fx)`
  - `ToFxScalar(f)` instead of `sim::ToSimScalar(f)`
- Keep conversion names semantically neutral. A fixed vector converted to float
  may be used for render, audio, debug, UI, particles, or logs, not only render.
- Boundary conversions should appear near the boundary, not sprinkled through
  gameplay logic as a way to make gameplay decisions.

## Mechanical Rename Order

Do this in compile-checked batches. Each batch should be a mostly mechanical
scripted/regex sweep and commit, with no intentional behavior changes. Avoid
drifting back into one-entity-at-a-time cleanup for this naming work.

Progress:

- Completed 2026-06-09: first scripted lane renamed the old float `Vec2` type
  and constructor call sites to `FVec2`, while keeping fixed vectors spelled
  `sim::Vec2` for now.
- Completed 2026-06-09: second scripted lane renamed old float `RenderAABB` and
  `ToRenderAABB(...)` call sites to `FAABB` and `ToFAABB(...)`.
- Completed 2026-06-09: third scripted lane renamed fixed vectors from
  `sim::Vec2` to `sim::FxVec2`.
- Completed 2026-06-09: fourth scripted lane renamed fixed rectangles from
  `sim::AABB` to `sim::FxAABB`.

1. Rename the old float `Vec2` type to `FVec2`.
   - Update constructors and operators mechanically.
   - `Vec2::New(...)` becomes `FVec2::New(...)` where the old float type is
     intended.
   - Keep fixed vectors under `sim::Vec2` during this batch to avoid ambiguity.

2. Rename old float `RenderAABB` to `FAABB` or `FRect`.
   - Pick one before implementation.
   - `ToRenderAABB(...)` becomes `ToFAABB(...)` if `FAABB` is chosen.

3. Rename fixed aliases in `src/sim/fxp.hpp`.
   - `sim::Vec2` -> `sim::FxVec2`
   - `sim::AABB` -> `sim::FxAABB`
   - Consider `sim::Scalar` -> `sim::FxScalar`, or keep `Scalar` only if it
     remains obvious after the vector rename.

4. Shorten conversion helpers.
   - `sim::ToRenderVec2(...)` -> `ToFVec2(...)`
   - `sim::ToSimVec2(...)` -> `ToFxVec2(...)`
   - `sim::ToRenderScalar(...)` -> `ToFloat(...)`
   - `sim::ToSimScalar(...)` -> `ToFxScalar(...)`
   - `sim::PixelVec2(...)` -> `FxVec2::from_pixels(...)` if that call shape is
     readable enough everywhere.

5. Collapse entity fixed geometry accessor names.
   - Only after float `Vec2` is renamed, so `GetCenter()` can safely mean fixed.
   - `GetSimCenter()` -> `GetCenter()`
   - `SetSimCenter(...)` -> `SetCenter(...)`
   - `GetSimAABB()` -> `GetAABB()`
   - `GetSimFeet()` -> `GetFeet()`
   - `GetSimGroundProbe()` -> `GetGroundProbe()`
   - Remove or quarantine old render-center wrappers. If a render/debug caller
     still needs float center, make it call `ToFVec2(ent.GetCenter())`.

6. Rename helper APIs that are currently type-noisy.
   - `GetSimSpriteTopLeftForEnt(...)` -> `GetSpriteTopLeftForEnt(...)` if the
     fixed overload is the only gameplay-facing version.
   - Keep float helper names explicit, e.g. `GetFSpriteTopLeftForEnt(...)` or
     render-specific names if they are presentation-only.
   - `GetVisualCenterForEnt(..., FxVec2)` should become the default overload or
     a fixed-only helper.
   - `GetEmitPointForEnt(..., FxVec2)` should become the default overload or a
     fixed-only helper.

7. Update docs and comments.
   - Replace "sim vector" wording with "fixed vector" unless the text is about
     simulation phase.
   - Replace "render Vec2" with "float/presentation FVec2" unless the text is
     specifically about rendering.

## Regex-Friendly Mappings

These mappings are good candidates for broad scripted replacement after the
old float type has been renamed:

```text
sim::ToRenderVec2(   -> ToFVec2(
sim::ToSimVec2(      -> ToFxVec2(
sim::ToRenderScalar( -> ToFloat(
sim::ToSimScalar(    -> ToFxScalar(
sim::Vec2            -> sim::FxVec2
sim::AABB            -> sim::FxAABB
GetSimCenter(        -> GetCenter(
SetSimCenter(        -> SetCenter(
GetSimAABB(          -> GetAABB(
GetSimFeet(          -> GetFeet(
GetSimGroundProbe(   -> GetGroundProbe(
```

The old float `Vec2` -> `FVec2` pass should be scripted with token-aware care,
not a plain unbounded text replacement, because files will contain both old
float `Vec2` and fixed `sim::Vec2` during the transition.

For the simple lanes, prefer one command/script per mapping plus compile:

```text
RenderAABB -> FAABB
sim::Vec2 -> sim::FxVec2
sim::AABB -> sim::FxAABB
sim::ToRenderVec2( -> ToFVec2(
sim::ToSimVec2( -> ToFxVec2(
GetSimCenter( -> GetCenter(
```

If a lane cannot be handled mechanically, stop and document the conflict rather
than silently doing a long manual migration.

## Safety Checks

Run after every batch:

```sh
./scripts/build.sh
./build/splonks-cpp --check-state-fingerprint-smoke --project-root "$PWD"
./build/splonks-cpp --check-state-equality-smoke --project-root "$PWD"
git diff --check
```

Extra checks after the full rename:

```sh
rg -n "ToRenderVec2|ToSimVec2|ToRenderScalar|ToSimScalar" src
rg -n "GetSimCenter|SetSimCenter|GetSimAABB|GetSimFeet|GetSimGroundProbe" src
rg -n "\\bVec2\\b" src
```

Expected final state:

- No `ToRender*` / `ToSim*` conversion names in gameplay code.
- No `GetSim*` entity geometry names.
- `FxVec2` is the fixed-point gameplay vector spelling.
- `FVec2` is the float presentation vector spelling.
- Float conversions are visible only at render/audio/debug/UI/particle/tooling
  boundaries.

## Open Naming Decision

Pick rectangle spelling before implementation:

- `FxAABB` / `FAABB`: most mechanical and closest to the current vocabulary.
- `FxRect` / `FRect`: friendlier if we want to stop saying AABB everywhere.

Recommendation: use `FxAABB` / `FAABB` for this pass because it is mechanical
and preserves collision vocabulary. Rename to `Rect` later only if it actually
improves readability.
