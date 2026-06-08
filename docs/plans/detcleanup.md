# Determinism Cleanup Plan

## Purpose

The determinism audit and gfxp migration are moving Splonks toward fixed-point
authoritative simulation. That work should not leave behind duplicate geometry
APIs, ambiguous render/sim names, or long-term conversion clutter that makes
gameplay code harder to write.

This plan tracks cleanup that should happen alongside the determinism audit, not
after the codebase has normalized transitional APIs.

## Desired Geometry Model

Gameplay should use gfxp-backed fixed geometry by default:

- Use `sim::Scalar`, `sim::Vec2`, and `sim::AABB` for authoritative simulation.
- Treat `sim::AABB` as the Splonks gameplay alias for gfxp's Fixed12 AABB.
- Convert to float/render geometry only at render, debug, UI, audio, tooling,
  and other presentation boundaries.
- Do not make gameplay decisions using old float `AABB` or render `Vec2`.

The Splonks aliases should make it obvious that they are gfxp aliases, not new
wrapper types:

```cpp
using Scalar = gfxp::Fixed12;
using Vec2 = gfxp::Vec2_12;
using AABB = gfxp::Aabb_12;
```

`gfxp::BasicAabb<FixedT>` is the generic template, while `gfxp::Aabb_12` is the
concrete Fixed12 type Splonks wants. Splonks should prefer the concrete alias in
`sim/fxp.hpp` so readers do not wonder whether `sim::AABB` is a separate box
type.

## AABB Naming Policy

There are multiple rectangle meanings, but there should not be multiple
authoritative coordinate systems:

- Body/pbox: `ent.GetSimAABB()` returns the fixed body bounds from `pos + size`.
- Contact/cbox: `GetContactAabbForEnt(...)` returns the fixed current animation
  contact bounds.
- Draw/sprite bounds: render-only pixel bounds for presentation and debug.

Cleanup target:

- Keep `GetContactAabbForEnt(...) -> sim::AABB`.
- Keep `ent.GetSimAABB() -> sim::AABB`.
- Migrate gameplay callers off `GetRenderContactAabbForEnt(...)`.
- Move or limit `GetRenderContactAabbForEnt(...)` to render/debug adapter code
  only.
- Keep `GetEntBroadphaseAabb(...) -> sim::AABB`.
- Move or limit `GetRenderEntBroadphaseAabb(...)` to the spatial index adapter
  while the spatial index still stores old float `AABB`.
- Eventually old float `AABB` should not appear in authoritative gameplay
  files.

## Cleanup Findings

### 1. `sim` aliases still expose gfxp templates

Current state:

- Completed 2026-06-08. `src/sim/fxp.hpp` now aliases `sim::Vec2` to
  `gfxp::Vec2_12` and `sim::AABB` to `gfxp::Aabb_12`.

Cleanup:

- [x] Replace those aliases with `gfxp::Vec2_12` and `gfxp::Aabb_12`.
- [x] Keep `sim::Scalar = gfxp::Fixed12`.
- [x] Document `sim::*` as "Splonks gameplay Fixed12 aliases over gfxp."
- [x] Validate with `./scripts/build.sh`,
      `./build/splonks-cpp --check-state-fingerprint-smoke --project-root "$PWD"`,
      and
      `./build/splonks-cpp --check-state-equality-smoke --project-root "$PWD"`.

### 2. Old float `AABB` has a generic name

Current state:

- `src/utils.hpp` defines `struct AABB` as a float `Vec2` rectangle.
- The name `AABB` is too authoritative-looking now that fixed geometry exists.
- This type still appears in gameplay files, world queries, stage mutation,
  contact dispatch, hit/damage logic, and debug/render.

Cleanup:

- Rename or quarantine the old type as `RenderAABB` / `FloatAABB`.
- Keep conversion helpers at boundaries, e.g. `ToRenderAABB(sim::AABB)`.
- Do not allow unqualified float `AABB` in authoritative gameplay after the
  migration.
- Consider moving render-only AABB helpers out of `utils.hpp` into a
  presentation/adapter header once call sites are reduced.

### 3. Contact AABB render wrappers leaked through gameplay

Current state:

- `GetContactAabbForEnt(...)` now returns `sim::AABB`, which is correct.
- `GetRenderContactAabbForEnt(...)` exists as a transitional adapter.
- Many gameplay files still explicitly call `GetRenderContactAabbForEnt(...)`,
  including contact damage, ent contact, stomp, buying, exits, weapons, traps,
  sacrifice altar, teleporter, and web/cobweb logic.

Cleanup:

- Migrate these systems one at a time to fixed contact geometry.
- Keep render wrappers only in render/debug and temporary float adapter
  boundaries.
- Prefer fixed overloads in `world_query` rather than calling `ToRenderAABB`
  for query broadphase unless the underlying spatial index still requires it.

### 4. Spatial index still forces a render-space bridge

Current state:

- `State::UpdateSidForEnt` uses `GetRenderEntBroadphaseAabb(...)` because
  `SID` stores/query old float `AABB`.
- `QueryEntsInAabb(state, sim::AABB, ...)` currently bridges through
  `ToRenderAABB(area)`.

Cleanup:

- Decide whether `SID` should store fixed `sim::AABB` or integer pixel cells.
- Migrate `SID` to a deterministic fixed/int broadphase so fixed gameplay
  queries do not need render conversions.
- Until then, treat the `SID` render bridge as a known adapter boundary, not as
  permission for gameplay code to use float AABBs.

### 5. Entity sim/render accessor pairs are noisy

Current state:

- Runtime entity fields `pos`, `vel`, `acc`, and `size` are already fixed.
- `Ent` still exposes both `GetSimPos` / `SetSimPos` and
  `GetRenderPos` / `SetRenderPos`, plus old float helpers like `GetAABB`,
  `GetFeet`, `GetGroundProbe`, `GetCenter`, and `SetCenter`.
- `GetSimPos()` is functionally just `ent.pos`; it exists mostly as transition
  scaffolding.

Cleanup:

- Prefer direct fixed fields or fixed helpers in gameplay.
- Keep render accessors explicitly named and limit them to presentation or
  temporary adapter boundaries.
- Migrate old generic helpers (`GetAABB`, `GetCenter`, `SetCenter`,
  `GetBounds`, `GetFeet`, `GetGroundProbe`) to fixed defaults or render-named
  wrappers.

### 6. World query has duplicate float/fixed overload sets

Current state:

- `world_query.cpp` has both float `AABB` and fixed `sim::AABB` overloads for
  nearest-world AABB, intersection, tile queries, blocking checks, and entity
  queries.
- Raycasts still use old render `Vec2` / `AABB` paths.

Cleanup:

- Make fixed/int query APIs the canonical gameplay path.
- Move old float overloads behind render/debug names or remove them as callers
  migrate.
- Audit raycast APIs separately. If raycasts affect gameplay, convert their
  inputs, stepping, target bounds, and results to fixed/int types without
  changing raycast semantics.

### 7. Stage mutation APIs still accept float AABB

Current state:

- `Stage::SetTilesInRectWc(const AABB& area, ...)` and
  `Stage::SetTilesInRect(const AABB& area, ...)` accept old float AABB.
- Stage break and several entity systems still construct float tile/world
  rectangles for gameplay mutation.

Cleanup:

- Add fixed or integer-tile rectangle APIs for gameplay stage mutation.
- Make float rectangle stage mutation render/tooling-only or remove it.

### 8. Stage fluids still round-trip through render vectors

Current state:

- `stage_fluids.cpp` still converts fixed fluid gravity/velocity through
  `sim::ToRenderVec2` and `sim::ToSimVec2` during internal math.
- This was already called out in the determinism audit, but it is also API
  clutter because fixed state is being manipulated through float helpers.

Cleanup:

- Move fluid velocity/gravity math to fixed `sim::Vec2` helpers.
- Keep float only for rendering/debug visualization of fluid values.

### 9. Network and spawn code still converts through render `Vec2`

Current state:

- Stage spawning, lobby retained players, join handoff, player lifecycle, and
  stage init frequently build spawn positions as render `Vec2` and then call
  `sim::ToSimVec2`.
- Some of this is harmless authoring/construction code, but much of it is
  gameplay topology and join-state code.

Cleanup:

- Use fixed spawn position helpers for authoritative spawn/topology paths.
- Keep render `Vec2` spawning only for debug/test authoring adapters.
- Prefer `sim::PixelVec2(...)` and fixed offsets for common integer-pixel spawn
  spacing.

### 10. Generic entity counters remain float

Current state:

- `Ent::counter_a` through `counter_d` remain raw floats.
- Many uses are integer frame counters, enum-like states, cooldowns, or small
  gameplay values.
- Some counters are converted to/from fixed in retention/snapshot paths, which
  is a transitional mixed model.

Cleanup:

- Split generic counters into typed fields where possible.
- Convert frame counters and enum-like values to integer types.
- Convert distance/time scalar counters that truly need fractions to
  `sim::Scalar`.
- Remove snapshot/retention conversion churn once runtime storage is typed.

### 11. `ToSim*` / `ToRender*` helpers are useful but easy to overuse

Current state:

- Boundary helpers are necessary.
- Search results show many conversions inside gameplay folders, not only at
  render/debug/UI boundaries.

Cleanup:

- Treat conversions inside authoritative gameplay as migration debt unless the
  function is clearly an authoring/debug adapter.
- Prefer fixed constructors such as `sim::Scalar::from_int`,
  `sim::Vec2::from_pixels`, and `sim::PixelVec2` for gameplay constants.
- Keep `ToRender*` near render/debug/audio/UI, not in collision, damage,
  topology, stage mutation, or gameplay query code.

## Suggested Order

1. Clarify `sim/fxp.hpp` aliases to use `gfxp::Vec2_12` and `gfxp::Aabb_12`.
2. Migrate `GetRenderContactAabbForEnt` gameplay callers subsystem-by-subsystem.
3. Convert `SID` to fixed/int broadphase or explicitly quarantine it as the only
   fixed-query-to-render adapter.
4. Rename/quarantine old float `AABB`.
5. Convert world-query raycasts and remaining float query overloads.
6. Convert stage mutation rectangle APIs.
7. Convert stage fluids fixed state math.
8. Split generic entity counters.

## Rule Of Thumb

If a function can change entities, tiles, damage, pickups, AI, topology, or
lockstep-visible state, it should accept and return fixed/int simulation types.
Old float `AABB` and render `Vec2` are allowed only in explicit boundary
adapters for render, debug, UI, audio, tooling, data import, or temporary
migration bridges. Those adapters should be named as adapters and should not
contain authoritative gameplay decisions.
