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
- `sim::Scalar`: fixed-point scalar. Keep this spelling because it is always
  scoped through `sim::`, while vectors and rectangles need the shorter
  `Fx`/`F` contrast at call sites.

This plan is intentionally a rename/collapse pass. It should not change
gameplay behavior, lockstep protocol semantics, or rendering behavior.

This should be executed with broad scripted replacements per lane, not by
hand-converting individual call sites. Hand work is for fixing compile errors,
resolving rare name conflicts, and reviewing boundary mistakes after each
scripted sweep.

This is also a helper-collapse pass. The fixed-point migration should not leave
behind two versions of every helper. When a helper is meaningful in gameplay,
the canonical helper should return fixed-point values. Presentation code should
call that helper and convert at the boundary with `ToFVec2(...)`, `ToFloat(...)`,
or `ToFAABB(...)`. Add a separate float helper only when it performs genuinely
presentation-only work that cannot sensibly be represented by the fixed helper.
Do not grow helper pairs like `GetThing(...)` and `GetFThing(...)` just because
one render call site needs a float. Render/debug/audio/UI code can call the
fixed helper and convert the returned value locally.

Important anti-pattern: do not "fix" a `GetSimThing(...)` name by creating a
canonical `GetThing(...)` plus a mirror `GetFThing(...)`. That repeats the
migration clutter under a new prefix. The intended cleanup is:

```cpp
// Good: one fixed helper, local presentation conversion.
sim::FxVec2 GetCoreSizeWc(const Stage& stage);
const FVec2 render_size = ToFVec2(GetCoreSizeWc(stage));

// Bad: duplicate helper that only returns a float copy.
FVec2 GetFCoreSizeWc(const Stage& stage);
```

The same rule applies to scalar helpers such as stage void-death Y. Keep the
fixed helper as the default and use `ToFloat(stage.GetVoidDeathY())` in
render/debug code instead of adding `GetFVoidDeathY()`.

Also remove one-line alias wrappers that only rename another conversion or
method. These are not abstractions; they hide the boundary and make the codebase
look like it has more geometry APIs than it really does.

Examples to remove:

```cpp
// Bad: entity render wrappers that only cast fields or fixed helpers.
FVec2 Ent::GetRenderPos() const { return ToFVec2(pos); }
void Ent::SetRenderPos(const FVec2& value) { pos = ToFxVec2(value); }
FAABB Ent::GetRenderAABB() const { return ToFAABB(GetAABB()); }
FVec2 Ent::GetRenderCenter() const { return ToFVec2(GetCenter()); }

// Use the actual value and boundary conversion where it is needed.
const FVec2 render_pos = ToFVec2(ent.pos);
ent.pos = ToFxVec2(authored_pos);
const FAABB render_aabb = ToFAABB(ent.GetAABB());
const FVec2 render_center = ToFVec2(ent.GetCenter());
```

The same applies inside `gfxp`: do not wrap `ceil_int()`, `round_int()`, or
`trunc_int()` with `to_pixels_ceil()`, `to_pixels_round()`, or
`to_pixels_trunc()` when the wrapper does nothing but change the name. Call the
real rounding method directly.

## Naming Rules

Default gameplay lane:

- Fixed-point is authoritative.
- Gameplay functions should take and return `FxVec2` / `FxAABB`.
- Names inside gameplay should not say `Sim` unless they are specifically
  about simulation phase/state machines rather than numeric type.
- Entity geometry helpers should be short:
  - `GetSimCenter()` -> `GetCenter()`
  - `SetSimCenter(...)` -> `SetCenter(...)`
  - `GetSimPos()` / `SetSimPos(...)` -> direct `ent.pos` access
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
- Do not duplicate gameplay/fixed helpers just to return floats. Prefer one
  canonical fixed helper and convert at render/audio/debug/UI boundaries.
- Graphics-side code may call fixed helpers directly. The wall is not "render
  cannot see fixed"; the wall is "authoritative gameplay should not depend on
  floats." If a render caller only needs a float copy of an authoritative value,
  call the authoritative helper and convert at that call site.

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
- Completed 2026-06-09: fifth scripted lane shortened scalar, vector,
  color, and AABB conversion helper names.
- Completed 2026-06-09: sixth scripted lane collapsed entity fixed geometry
  accessors from `GetSim*`/`SetSim*` to short fixed-default names.
- Completed 2026-06-09: seventh scripted lane renamed the fixed sprite
  top-left helper from `GetSimSpriteTopLeftForEnt` to `GetSpriteTopLeftForEnt`
  and removed the duplicate float helper.
- Completed 2026-06-09: eighth docs lane updated older fxp/determinism plan
  references from the migration vocabulary to the current `FxVec2`/`FxAABB`,
  `FVec2`/`FAABB`, and `ToFx*`/`ToF*` spellings.
- Completed 2026-06-09: scalar naming decision made. Keep `sim::Scalar` rather
  than renaming more than a thousand scalar uses to `sim::FxScalar`; the
  namespace already communicates the fixed domain, and `ToFxScalar(...)`
  remains the explicit float-to-fixed boundary.
- Completed 2026-06-09: ninth helper-collapse lane removed duplicate float
  overloads for `GetVisualCenterForEnt(...)` and `GetEmitPointForEnt(...)`.
  Presentation callers now use the fixed helper and convert with `ToFVec2(...)`
  at the call site.
- Completed 2026-06-09: documented and corrected the `GetFCoreSizeWc` /
  `GetFStagePixelDims` / `GetFVoidDeathY` anti-pattern. Renaming away `Sim`
  should not create new float mirror helpers.

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
   - Keep `sim::Scalar` as the fixed scalar spelling; it remains obvious because
     scalar uses are scoped through `sim::`, and the conversion helper is still
     explicit as `ToFxScalar(...)`.

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
   - Do not add a matching float helper unless it is genuinely
     presentation-only. Render callers can use the fixed helper and convert the
     result with `ToFVec2(...)`.
   - `GetVisualCenterForEnt(..., FxVec2)` should become the default overload or
     a fixed-only helper.
   - `GetEmitPointForEnt(..., FxVec2)` should become the default overload or a
     fixed-only helper.

7. Update docs and comments.
   - Replace "sim vector" wording with "fixed vector" unless the text is about
     simulation phase.
   - Replace "render Vec2" with "float/presentation FVec2" unless the text is
     specifically about rendering.

8. Audit duplicated migration helpers.
   - Search for fixed/float helper pairs that differ only by return type or
     naming prefix.
   - Delete the float helper when render/audio/debug/UI can call the fixed
     helper and convert the result at the boundary.
   - Keep one-off adapter helpers only when they encode a real boundary or
     domain concept, not just because a call site needed a float for rendering.
   - Examples of suspicious names to review: `GetF*`, `GetRender*`,
     `SetRender*`, `ToF*`, and `ToFx*`. Some are valid boundary APIs; the point
     is to remove duplication, not to ban the words outright.

Audit notes from 2026-06-09:

- `src` has no remaining `ToRenderVec2`, `ToSimVec2`, `ToRenderScalar`,
  `ToSimScalar`, `GetSimSpriteTopLeftForEnt`, or `GetFSpriteTopLeftForEnt`
  symbols.
- The duplicate sprite top-left helper was removed. Render now calls
  `GetSpriteTopLeftForEnt(...)` and converts with `ToFVec2(...)`.
- The one-line `Ent::GetRender*` / `Ent::SetRender*` methods were removed.
  Render/debug code now reads the fixed field or fixed helper and converts at
  the actual boundary.
- The one-line `Ent::GetPos` / `SetPos` / `GetVel` / `SetVel` / `GetAcc` /
  `SetAcc` methods were also removed. `pos`, `vel`, and `acc` are public fixed
  simulation state; wrapping those fields in no-op accessors only hides the
  model.
- `Stage::GetVoidDeathY()` and `Stage::GetSimVoidDeathY()` are a small
  float/fixed boundary pair. The fixed side is gameplay; the float side is
  currently only used by debug rendering. This is acceptable for now, but a
  later pass could rename the fixed side to `GetVoidDeathY()` and make debug
  render convert locally if we want the same fixed-default style on `Stage`.
- `GetVisualCenterForEnt(...)` and `GetEmitPointForEnt(...)` now have only
  fixed-returning helper APIs. Render/audio/debug callers convert with
  `ToFVec2(...)` at their presentation boundary.
- Climbing/hanging still contains float probe helpers next to fixed probe
  helpers. That is a real follow-up for the determinism cleanup, not a quick
  naming-only change. The goal should be one fixed authoritative probe path,
  with debug annotations converting to `FVec2` only when emitted.

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
- No duplicate float/fixed helper pairs where the float side can be replaced by
  fixed helper plus boundary conversion.
- `FxVec2` is the fixed-point gameplay vector spelling.
- `FVec2` is the float presentation vector spelling.
- Float conversions are visible only at render/audio/debug/UI/particle/tooling
  boundaries.

## Closed Naming Decision

Rectangle spelling chosen for this pass:

- `FxAABB`: fixed-point authoritative/gameplay rectangle.
- `FAABB`: float presentation rectangle.

This pass keeps AABB terminology because it is mechanical and preserves
collision vocabulary. Rename to `Rect` later only if it improves readability
without reopening the fixed/float helper split.
