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
- Remove gameplay/common access to old render contact wrappers.
- Keep `GetEntBroadphaseAabb(...) -> sim::AABB`.
- Remove old render broadphase wrappers. The spatial index should consume fixed
  `GetEntBroadphaseAabb(...)`.
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

- Completed 2026-06-08. The old float/render rectangle type has been renamed
  from `AABB` to `RenderAABB`.
- `sim::AABB` remains the only unqualified gameplay rectangle spelling.
- Remaining `RenderAABB` call sites are render/debug annotations or other
  explicit presentation boundaries.

Cleanup:

- [x] Rename or quarantine the old type as `RenderAABB` / `FloatAABB`.
- Keep conversion helpers at boundaries, e.g. `ToRenderAABB(sim::AABB)`.
- Do not allow unqualified float `AABB` in authoritative gameplay after the
  migration.
- Consider moving render-only AABB helpers out of `utils.hpp` into a
  presentation geometry header once call sites are reduced.

### 3. Contact AABB render wrappers leaked through gameplay

Current state:

- `GetContactAabbForEnt(...)` now returns `sim::AABB`, which is correct.
- The old `GetRenderContactAabbForEnt(...)` and
  `GetRenderEntBroadphaseAabb(...)` wrappers have been removed.
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
- Completed 2026-06-08: the old float `AabbTouchesBlockingStageBounds(...)`,
  `AabbHitsImpassableEnts(...)`, `AabbHitsBlockingTiles(...)`,
  `AabbHitsBlockingWorldGeometry(...)`, and
  `AabbHitsBlockingWorldGeometryOrImpassableEnts(...)` overloads were removed
  after remaining flesh-guy wall-slide and DVD-logo bounce probes moved to fixed
  `sim::AABB`.
- Completed 2026-06-08: world-query raycast target collection now stores fixed
  target AABBs and uses fixed contact cboxes.
- Completed 2026-06-08: area-listener enter/exit overlap detection now uses
  fixed body AABBs for both the area entity and overlap candidates.
- Completed 2026-06-08: area-listener tile-change notification checks now use
  fixed tile-center points and fixed body AABBs.
- Completed 2026-06-08: tile-overlap effect queries now use fixed body AABBs.
- Completed 2026-06-08: fixed `QueryTileAtWorldPos(stage, sim::Vec2)` was added,
  and hanging-spider ceiling support now samples from fixed body bounds.
- Completed 2026-06-08: fixed `BreakStageTilesInRectWc(sim::AABB, ...)` was
  added, and mattock tile breaking now uses the fixed rectangle path.
- Completed 2026-06-08: boulder leading break-strip queries and tile breaking
  now use fixed body bounds and the fixed stage-break rectangle path.
- Completed 2026-06-08: shop area accessors now use fixed `sim::AABB`. Shop
  child escape checks and gold-idol shop overlap use fixed AABBs, while debug
  overlay converts to render AABB at the rendering boundary.
- Completed 2026-06-08: the old float `GatherBlockingContactsForAabb(...)`
  overload was removed; blocking contact gathering now exposes only the fixed
  `sim::AABB` API.
- Completed 2026-06-08: physics movement cleanup removed dead float tile-contact
  snap/one-way helpers, and moving-platform top/hang carry detection now uses
  fixed body/feet AABBs and fixed overlap comparisons.
- Completed 2026-06-08: physics ground-friction support checks, grounded-on-ent
  checks, and grounded-on-tile probes now use fixed body/feet/ground-probe
  AABBs.
- Completed 2026-06-08: flesh-guy meat-slime surface probes and meathead pickup
  collection overlap now use fixed AABBs. Meathead debug annotation converts
  its fixed sensor at the debug-render boundary.
- Completed 2026-06-08: hang wall/corner probe blocking, hangable impassable
  overlap checks, and plausible locomotion candidate solid-tile checks now use
  fixed body/probe AABBs and fixed world query paths.
- Completed 2026-06-08: bat roof/perch checks and meathead popup tile
  placement now use fixed body/probe geometry before querying tiles.
- Completed 2026-06-08: the old float `BreakStageTilesInRectWc(AABB, ...)`
  overload was removed after mattock and boulder tile breaking had moved to the
  fixed rectangle path.
- Completed 2026-06-08: climber step-sound tile selection now queries ladder
  and rope tiles from the entity's fixed body AABB.
- Completed 2026-06-08: boulder, block, and door particle/audio/shake
  placement helpers now derive their render positions from fixed body AABBs at
  explicit presentation boundaries.
- Completed 2026-06-08: damsel rescue-kiss and gold-idol reward particle
  placement now derive render positions from fixed body AABBs at explicit
  presentation boundaries.
- Completed 2026-06-08: water queries gained fixed-position overloads, and
  piranha swim probes now use fixed center/body coordinates instead of generic
  render `GetAABB()` geometry.
- Completed 2026-06-08: entity shake queries now use fixed query AABBs, fixed
  entity centers, and fixed zero-radius containment instead of generic render
  `GetAABB()` geometry.
- Completed 2026-06-08: explosion damage broadphase no longer builds a
  temporary old float `AABB`; it converts explicit render-space corners into a
  fixed query AABB while preserving the previous floor/ceil bounds.
- Completed 2026-06-08: teleporter tile-center lookup no longer constructs an
  old float tile `AABB`.
- Remaining contact AABB render conversions are explicit `ToRenderAABB(...)`
  calls at render/debug presentation boundaries.

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
- [x] Migrate shop area overlap checks to fixed body geometry.
- [x] Remove float blocking-contact gatherer after callers moved to fixed
      AABBs.
- [x] Remove float blocking-world query overloads after callers moved to fixed
      AABBs.
- [x] Migrate moving-platform carry/hang carry physics queries to fixed AABBs.
- [x] Migrate physics ground support and grounded checks to fixed AABBs.
- [x] Migrate flesh-guy slime surface probes and meathead pickup overlap to
      fixed AABBs.
- [x] Migrate hang probe blocking and locomotion candidate collision checks to
      fixed AABBs.
- [x] Migrate bat roof/perch and meathead popup tile queries to fixed geometry.
- [x] Remove the old float stage-break rectangle API after callers are fixed.
- [x] Migrate climber step-sound tile selection to fixed body geometry.
- [x] Move boulder/block/door presentation placement helpers off generic
      `GetAABB()` and onto fixed body geometry plus explicit render conversion.
- [x] Move damsel/gold-idol reward particle placement helpers off generic
      `GetAABB()` and onto fixed body geometry plus explicit render conversion.
- [x] Add fixed water-query overloads and migrate piranha swim probes to fixed
      body geometry.
- [x] Migrate entity shake broadphase and containment checks to fixed geometry.
- [x] Remove incidental old float AABB construction from explosion and
      teleporter helper code.
- Migrate these systems one at a time to fixed contact geometry.
- [x] Remove common-layer render contact/broadphase wrappers after gameplay
      callers moved to fixed geometry.
- Keep render conversions only in render/debug/tooling boundaries.
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
- Completed 2026-06-08: `Ent::GetAABB()` was renamed to
  `Ent::GetRenderAABB()`, and the remaining callers are render/debug
  presentation boundaries.
- Completed 2026-06-08: unused old float `Ent::GetFeet()` and
  `Ent::GetGroundProbe()` adapters were removed; fixed `GetSimFeet()` and
  `GetSimGroundProbe()` remain.
- Completed 2026-06-08: gameplay callers of old render `Ent::GetBounds()`
  moved to fixed body AABBs. The remaining render-style helper was renamed to
  `Ent::GetRenderBounds()`.
- Completed 2026-06-08: common-layer render contact/broadphase wrappers
  `GetRenderContactAabbForEnt(...)` and `GetRenderEntBroadphaseAabb(...)` were
  removed. Render/debug callers now convert fixed contact AABBs explicitly at
  their boundary.
- Completed 2026-06-08: old generic center helpers were renamed to
  `GetRenderCenter()` and `SetRenderCenter()`. This removes the ambiguous
  neutral names; remaining render-center use in gameplay files is still
  visible migration debt.
- Completed 2026-06-08: nearest-player queries gained fixed `sim::Vec2`
  overloads, and spider, hanging-spider, skeleton, piranha, caveman, cobra,
  shopkeeper, ghost-ball, DVD-logo, damsel, chest, gold-idol, and
  sacrifice-altar nearest-player or chase checks now use fixed positions/deltas
  where the decision is authoritative. Render-center calls remain only where
  presentation, raycast, audio, or still-unmigrated gameplay code requires them.
- Completed 2026-06-08: thwomp trap drop detection, drop-door sensor detection,
  and giant-tiki-head closest-player/facing decisions now use fixed
  positions/deltas. Their remaining render-center calls are presentation or
  spawn-placement boundaries.
- Completed 2026-06-08: bat target detection, target ranking, and chase
  acceleration now stay in fixed positions/deltas instead of selecting a render
  target and converting it back to sim space.
- Completed 2026-06-08: shopkeeper dropped-pistol recovery now uses fixed
  positions/deltas for facing, movement direction, and jump decisions.
- Completed 2026-06-08: body-contact knockback direction now uses fixed
  source/target centers and fixed wrapped deltas before producing the existing
  fixed knockback velocity.
- Completed 2026-06-08: explosion entity footprint checks and knockback
  direction now use fixed centers/deltas, and the unused float world-delta
  explosion footprint helper was removed. Tile footprint selection and visual
  effect placement remain on their existing render/int boundary paths.
- Completed 2026-06-08: monkey AI target selection, climbable tile probing,
  climb target scoring, climb snapping, player grab attachment, bounce/trip
  direction, and thrown loot velocities now use fixed centers, fixed world
  deltas, fixed tile queries, and fixed velocity writes. Remaining monkey render
  center use is presentation/audio or tool API boundary code.
- Completed 2026-06-08: carry back-item placement, thrown-entity placement,
  and spawned tool-throw placement now write fixed centers directly instead of
  using `SetRenderCenter(...)`/`GetRenderCenter(...)`. Held visual attachment
  still uses `SetVisualCenterForEnt(...)` because that path intentionally
  applies authored sprite-frame offsets and needs its own fixed visual-center
  adapter pass.

Cleanup:

- Prefer direct fixed fields or fixed helpers in gameplay.
- Keep render accessors explicitly named and limit them to presentation or
  migration boundary code.
- [x] Rename old render `GetAABB()` to explicit `GetRenderAABB()`.
- [x] Remove unused old render `GetFeet()` and `GetGroundProbe()` adapters.
- [x] Rename old render `GetBounds()` to explicit `GetRenderBounds()` and move
      gameplay callers to fixed body AABBs.
- [x] Rename old generic helpers (`GetCenter`, `SetCenter`) to render-named
      wrappers.
- [x] Add fixed nearest-player query overloads and migrate a first pass of AI
      chase/distance/target lookup callers to fixed centers and deltas.
- Migrate authoritative gameplay callers of `GetRenderCenter()` and
  `SetRenderCenter()` to fixed center helpers or fixed placement APIs.

### 6. World query has duplicate float/fixed overload sets

Current state:

- `world_query.cpp` has both float `AABB` and fixed `sim::AABB` overloads for
  nearest-world AABB, intersection, tile queries, blocking checks, and entity
  queries.
- Presentation-only tile raycasts still use render `Vec2` inputs for audio and
  debug occlusion visualization.
- Completed 2026-06-08: unused non-raycast float `AABB` overloads were removed
  from nearest-world AABB, world AABB contains/intersect, tile AABB query,
  entity AABB query, and one-way-support query APIs.
- Completed 2026-06-08: horizontal world raycasts gained a fixed `sim::Vec2`
  start-position API with fixed ray broadphase collection. Caveman, cobra,
  shopkeeper sight checks, and pistol hitscan now use fixed starts/deltas and
  the fixed horizontal raycast path. The old render horizontal overload and the
  unused render vertical/diagonal/world raycast APIs were removed.

Cleanup:

- Make fixed/int query APIs the canonical gameplay path.
- [x] Remove unused non-raycast float AABB overloads after callers migrated to
      fixed AABB APIs.
- [x] Add fixed horizontal raycast path and migrate AI sight checks that affect
      gameplay state.
- [x] Move the remaining old tile raycast behind an explicit
      `RaycastRenderTiles(...)` name for audio/debug presentation callers.
- [x] Remove unused render-space vertical, entity-only, and general world
      raycast APIs instead of keeping compatibility clutter.
- [x] Move pistol hitscan to the fixed horizontal raycast path.
- Audit any future gameplay raycast APIs separately. If they affect gameplay,
  convert their inputs, stepping, target bounds, and results to fixed/int types
  without changing raycast semantics.

### 7. Stage mutation APIs still accept float AABB

Current state:

- Completed 2026-06-08. `Stage::SetTilesInRectWc(sim::AABB, ...)` now accepts
  fixed world-space rectangles, and `Stage::SetTilesInRect(IAABB, ...)` now
  accepts integer tile rectangles. The old float rectangle setter surface was
  removed.
- `BreakStageTilesInRectWc(sim::AABB, ...)` now exists for fixed gameplay
  callers. Mattock and boulder tile breaking use it.
- Stage break and any future gameplay mutation callers should stay on the fixed
  world-space or integer tile-space paths.

Cleanup:

- [x] Add fixed or integer-tile rectangle APIs for gameplay stage mutation.
- [x] Remove old float rectangle stage mutation APIs.
- Keep any future render/tooling rectangle mutation adapters clearly named and
  outside authoritative gameplay.

### 8. Stage fluids still round-trip through render vectors

Current state:

- Completed 2026-06-08. `stage_fluids.cpp` now keeps fluid velocity,
  gravity, temporary gravity, direction, damping, and incoming velocity vector
  math in `sim::Vec2`/`sim::Scalar` during internal simulation.
- Completed 2026-06-08. Fluid amount, capacity, scoring, budget, and changed
  tile comparisons now use fixed `sim::Scalar` math during internal
  simulation.
- Completed 2026-06-08. Fluid gravity setter APIs now accept `sim::Vec2`; the
  remaining float conversions are in debug brush/test authoring callers.
- This was already called out in the determinism audit, but it is also API
  clutter because fixed state is being manipulated through float helpers.

Cleanup:

- [x] Move fluid velocity/gravity math to fixed `sim::Vec2` helpers.
- [x] Convert fluid amount/transfer scoring to fixed scalar math.
- [x] Convert fluid gravity mutation APIs to fixed `sim::Vec2`.
- Keep float only for rendering/debug visualization of fluid values.

### 9. Network and spawn code still converts through render `Vec2`

Current state:

- Stage spawning, lobby retained players, join handoff, player lifecycle, and
  stage init frequently build spawn positions as render `Vec2` and then call
  `sim::ToSimVec2`.
- Completed 2026-06-08. Stage spawning now has fixed `sim::Vec2` overloads for
  player placement, player spawning, and top-left/center entity spawning; the
  render `Vec2` overloads are adapters for existing debug/tooling callers.
- Completed 2026-06-08. Normal stage initialization now places connected
  players through fixed spawn positions and fixed integer-pixel spacing.
- Completed 2026-06-08. Network player spawn/reconnect helpers now use fixed
  `sim::Vec2` positions for retained reconnects, join accept positions, join
  barrier topology packets, synced stage reload placement, and player lifecycle
  respawns.
- Completed 2026-06-08. Stage entrance spawn lookup now returns fixed
  `sim::Vec2`; network respawn/revive and reconnect spawn paths consume the
  entrance position without render-vector round trips.
- Completed 2026-06-08. Stage rotation now rotates entity centers, fluid
  velocity/gravity vectors, and the rotation pivot in fixed coordinates. Render
  vectors remain only for particles, audio emitters, camera presentation, and
  animation interpolation.
- Completed 2026-06-08. The normal stage-spawning API names now accept only
  fixed `sim::Vec2` positions. Render-position adapters were renamed to
  `PlacePlayerAtRenderPosition(...)`, `SpawnPlayerAtRenderPosition(...)`,
  `SpawnPlayerForPlayerIdAtRenderPosition(...)`,
  `SpawnStageEntAtRenderTopLeft(...)`, and
  `SpawnStageEntAtRenderCenter(...)`, and their callers are debug/test fixture
  authoring paths.
- Some of this is harmless authoring/construction code, but much of it is
  gameplay topology and join-state code.

Cleanup:

- [x] Add fixed spawn position helpers for stage spawning APIs.
- [x] Move normal stage initialization player placement to fixed spawn
  positions.
- [x] Move network player lifecycle, retained reconnect, join accept, and join
  barrier spawn positions to fixed vectors.
- [x] Move stage entrance spawn lookup and its network callers to fixed vectors.
- [x] Move stage-rotation entity centers, fluid vectors, and pivot setup to
      fixed coordinates.
- [x] Use fixed spawn position helpers for remaining authoritative
      spawn/topology paths.
- [x] Keep render `Vec2` spawning only for explicitly named debug/test
      authoring adapters.
- Prefer `sim::PixelVec2(...)` and fixed offsets for common integer-pixel spawn
  spacing.

### 10. Generic entity counters remain generic

Current state:

- Completed 2026-06-08. `Ent::counter_a` through `counter_d` now store
  `sim::Scalar` instead of raw floats.
- Completed 2026-06-08. Counter fingerprinting and state-fingerprint debug
  comparison now hash/read fixed scalar values instead of quantized floats.
- Completed 2026-06-08. Debug playback recording, playback loading, spec
  initialization, spec restore, and lobby-retained player restore now preserve
  counter values as fixed scalars without fixed/render conversion churn.
- Completed 2026-06-08. Runtime counter users that assign frame counts,
  cooldowns, enum-like markers, ammo/roll values, and distance intervals now
  assign fixed scalars explicitly. Render/debug display code converts counters
  to floats only at presentation boundaries.
- The fields are still generic `counter_a` through `counter_d`. Many uses would
  be clearer and harder to misuse if split into typed entity-specific fields.

Cleanup:

- [x] Convert runtime generic counter storage from raw `float` to `sim::Scalar`.
- [x] Convert fingerprinting, replay I/O, spec restore, and lobby-retention
      code to preserve fixed counter values directly.
- [x] Update runtime counter assignments and comparisons so frame counts,
      enum-like values, ammo counts, cooldowns, and distance intervals use fixed
      scalars explicitly.
- Split generic counters into typed fields where possible.
- Convert frame counters and enum-like values to integer types where/when they
  become real named fields.
- Keep distance/time scalar counters that truly need fractions as `sim::Scalar`.

### 11. `ToSim*` / `ToRender*` helpers are useful but easy to overuse

Current state:

- Boundary helpers are necessary.
- Completed 2026-06-08: generic entity counters now store fixed scalars without
  fixed/render conversion churn.
- Completed 2026-06-08: chest loot/trap-bomb, giant-spider loot, loose skull,
  sacrifice-altar reward, craps prize/dice, carry throw, cobra spit, pistol,
  mattock, machete, bat return, door drop, thwomp return, explosion fallback,
  body-contact, and stomp gameplay velocities now build fixed vectors directly
  instead of detouring through render vectors or deterministic float RNG.
- Completed 2026-06-08: bow and web-cannon aim now carry fixed aim vectors for
  projectile/recoil velocities, and common tool/carry throw APIs accept fixed
  `sim::Vec2` velocities instead of render `Vec2`.
- Remaining conversion hits are mostly explicit authoring/spawn boundaries,
  debug/render rectangles, test fixture setup, or a small number of
  visual-authoring-to-gameplay bridges.
- `stage_spawning.cpp` still exposes render-position spawn overloads that
  convert into fixed positions, but their names now explicitly include
  `RenderPosition`/`RenderTopLeft`/`RenderCenter`. These are authoring/tooling
  boundaries; fixed spawn names are fixed-only for gameplay callers.
- `world_query.cpp` keeps `RaycastRenderTiles(...)` as an explicit
  presentation-only tile ray for audio/debug occlusion visualization. Gameplay
  horizontal raycasts use fixed `sim::Vec2` starts.
- Completed 2026-06-08: ball-and-chain force logic now derives chain anchor,
  ball center, and delta in fixed sim coordinates. The local rounded
  length/ratio algorithm still uses raw fixed components internally, but no
  render center or float re-quantization is part of the gameplay force path.
- Cobra sight currently converts an animation-derived emit point into a fixed
  ray origin. That is a real boundary between visual emit-point authoring and
  gameplay ray logic; clean replacement likely needs fixed emit points or a
  clear gameplay muzzle/eye-point API.
- Remaining `ToRenderAABB(...)`/`RenderAABB` uses in mattock, arrow trap,
  teleporter, trap block, machete, sac altar, meathead, and hang are debug,
  prompt, or render annotation boundaries unless a later audit proves otherwise.

Cleanup:

- Treat conversions inside authoritative gameplay as migration debt unless the
  function is clearly authoring, debug, tooling, audio, or rendering code.
- Prefer fixed constructors such as `sim::Scalar::from_int`,
  `sim::Vec2::from_pixels`, and `sim::PixelVec2` for gameplay constants.
- Keep `ToRender*` near render/debug/audio/UI, not in collision, damage,
  topology, stage mutation, or gameplay query code.
- [x] Remove deterministic `RandomFloat` from authoritative entity launch
      velocities.
- [x] Replace simple gameplay velocity/knockback vector conversions with direct
      fixed vector construction.
- [x] Add fixed aim/direction APIs for bow, web cannon, throw/carry aiming, and
      similar projectile launch paths.
- [x] Convert gameplay raycast entry points to fixed/int-friendly APIs and
      quarantine the remaining presentation tile ray as `RaycastRenderTiles`.
- [x] Prefer fixed spawn APIs in gameplay code and keep render spawn adapters
      explicitly named and limited to authoring/tooling fixtures.

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

Authoritative gameplay code should stay inside the fixed-point boundary. Any
function that can affect entities, tiles, damage, pickups, AI, topology, or
other lockstep-visible state should take and return fixed/int simulation types,
not old float `AABB` or render `Vec2`.

Float/render adapters are only for crossing out of that boundary. They belong at
places where simulation data is being presented to, or authored by,
non-authoritative systems: rendering, debug UI, audio, tooling, asset/data
import, test fixtures, and serialization/display text. Their names should make
the boundary crossing obvious, such as `ToRenderAABB(...)`,
`GetRenderCenter(...)`, or `SpawnDebug...(...)`.

If gameplay needs a query, collision check, AI decision, damage check, pickup
check, or topology change, it should not detour through a render/float adapter.
That is a sign the fixed/int gameplay API is missing and should be added.
