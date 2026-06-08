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
- Limit `GetRenderEntBroadphaseAabb(...)` to render/debug or temporary adapter
  callers. The spatial index should consume fixed `GetEntBroadphaseAabb(...)`.
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
- Completed 2026-06-08: buying overlap/prompt selection now uses fixed contact
  AABBs, fixed entity queries, fixed world wrapping, and render conversion only
  for final prompt placement.
- Completed 2026-06-08: basic exit overlap/prompt selection now uses fixed
  contact AABBs, fixed entity queries, fixed world wrapping, and render
  conversion only for final prompt placement.
- Completed 2026-06-08: DVD-logo/player transition contact now uses fixed
  contact AABBs and fixed world wrapping.
- Completed 2026-06-08: generic touched-ent contact gathering and chest
  overlap/key-touch checks now use fixed contact AABBs.
- Completed 2026-06-08: mantrap eating, baseball bat hit overlap, and
  craps-table player overlap now use fixed contact/body AABBs.
- Completed 2026-06-08: push and stomp contact checks now use fixed
  contact/body AABBs.
- Completed 2026-06-08: carry pickup search and generic world-op interaction
  overlap now use fixed contact AABBs.
- Completed 2026-06-08: machete strike overlap and grounded sacrifice-altar
  favor deposit checks now use fixed contact/body AABBs. Sacrifice effect
  placement converts fixed contact bounds to render coordinates only at the
  particle/audio presentation boundary.
- Completed 2026-06-08: shopkeeper pistol recovery blocking and pickup overlap
  checks now use fixed contact AABBs.
- Completed 2026-06-08: sacrifice-altar sacrifice area and victim overlap
  checks now use fixed contact AABBs. Victim effect placement converts fixed
  bounds to render coordinates only at the presentation boundary.
- Completed 2026-06-08: mattock strike overlap now uses fixed contact AABBs.
  The remaining mattock render contact wrapper use is debug annotation.
- Completed 2026-06-08: cobweb contact and occupancy overlap checks now use
  fixed contact AABBs, and an unused float cobweb-overlap helper was removed.
- Completed 2026-06-08: arrow-trap sensor overlap now uses fixed sensor and
  contact AABBs. Sensor debug drawing converts fixed bounds at the render
  boundary.
- Completed 2026-06-08: teleporter probe destination blocking, telefrag, and
  splat overlap checks now use fixed contact AABBs. Debug probe rectangles
  convert fixed bounds at the render boundary.
- Completed 2026-06-08: projectile body-contact impact overlap now uses fixed
  contact AABBs.
- Completed 2026-06-08: hurt-on-contact body overlap and player-foot exclusion
  checks now use fixed contact AABBs.
- Completed 2026-06-08: spike-foot contact and authored spike tile contact
  cbox overlap now use fixed contact AABBs.
- Completed 2026-06-08: trap-block sensor construction, player sensing,
  crusher-blocker checks, and trigger-distance selection now use fixed contact
  AABBs and fixed vectors. Trap-block debug rectangles convert to render AABBs
  only when adding debug annotations.
- Completed 2026-06-08: the old float `AabbHitsImpassableEnts(...)` overload
  now delegates to the fixed query path instead of doing its own render-contact
  collision checks.
- Remaining code references to `GetRenderContactAabbForEnt(...)` are in
  world-query raycast target collection, debug rendering, the helper
  declaration/definition, and a mattock debug annotation.

Cleanup:

- [x] Migrate buying overlap/prompt selection to fixed contact geometry.
- [x] Migrate basic exit overlap/prompt selection to fixed contact geometry.
- [x] Migrate DVD-logo/player transition contact to fixed contact geometry.
- [x] Migrate generic touched-ent contact gathering and chest contact checks to
      fixed contact geometry.
- [x] Migrate mantrap, baseball bat, and craps-table overlap checks to fixed
      contact/body geometry.
- [x] Migrate push and stomp contact checks to fixed contact/body geometry.
- [x] Migrate carry pickup search and generic world-op interaction overlap to
      fixed contact geometry.
- [x] Migrate machete strike and sacrifice-altar deposit checks to fixed
      contact/body geometry.
- [x] Migrate shopkeeper pistol recovery checks to fixed contact geometry.
- [x] Migrate sacrifice-altar sacrifice area and victim overlap checks to fixed
      contact geometry.
- [x] Migrate mattock strike overlap checks to fixed contact geometry.
- [x] Migrate cobweb contact and occupancy overlap checks to fixed contact
      geometry.
- [x] Migrate arrow-trap sensor overlap checks to fixed contact geometry.
- [x] Migrate teleporter probe overlap checks to fixed contact geometry.
- [x] Migrate projectile body-contact impact overlap checks to fixed contact
      geometry.
- [x] Migrate hurt-on-contact body overlap checks to fixed contact geometry.
- [x] Migrate spike-foot and spike tile contact cbox overlap checks to fixed
      contact geometry.
- [x] Migrate trap-block sensor and blocker overlap checks to fixed contact
      geometry.
- Migrate these systems one at a time to fixed contact geometry.
- Keep render wrappers only in render/debug and temporary float adapter
  boundaries.
- Prefer fixed overloads in `world_query` rather than calling `ToRenderAABB`
  for query broadphase unless the underlying spatial index still requires it.

### 4. Spatial index still forces a render-space bridge

Current state:

- Completed 2026-06-08. `SID` stores fixed `sim::AABB` records, builds bucket
  coverage from fixed pixel floor coordinates, and queries fixed `sim::AABB`
  areas directly.
- `State::UpdateSidForEnt` now indexes
  `ents::common::GetEntBroadphaseAabb(...)` instead of the render wrapper.
- `QueryEntsInAabb(state, sim::AABB, ...)` now queries `state.sid` directly.
- The old float `QueryEntsInAabb(state, AABB, ...)` overload remains as a
  temporary migration boundary and converts into the fixed query path.
- Removed unused SID APIs that exposed float `AABB` records:
  `SID::Insert(...)` and `QueryForVIDAABBsExclude(...)`.

Cleanup:

- [x] Decide whether `SID` should store fixed `sim::AABB` or integer pixel
      cells: store fixed `sim::AABB` and derive integer bucket cells.
- [x] Migrate `SID` to a deterministic fixed/int broadphase so fixed gameplay
      queries do not need render conversions.
- [x] Remove the spatial-index render bridge from `State::UpdateSidForEnt`.
- [x] Remove unused float-AABB SID query surfaces.
- [x] Validate with `./scripts/build.sh`,
      `./build/splonks-cpp --check-state-fingerprint-smoke --project-root "$PWD"`,
      `./build/splonks-cpp --check-state-equality-smoke --project-root "$PWD"`,
      and `git diff --check`.

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
Old float `AABB` and render `Vec2` should stay outside those gameplay-facing
APIs. Conversion code belongs at named render, debug, UI, audio, tooling, data
import, or temporary migration boundaries, and those boundaries should not make
authoritative gameplay decisions.
