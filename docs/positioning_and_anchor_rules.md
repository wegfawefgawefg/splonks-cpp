# Positioning And Anchor Rules

## Summary

This engine is currently a pixel-step physics engine, not a full subpixel collision engine.

That means:

- `vel` and `acc` being fractional is fine.
- Authoritative physics `pos` being fractional is not fine.
- Exact visual centers may still be fractional, especially for odd-sized sprites or boxes.

The important distinction is:

- visual center may be fractional
- physics anchor should be integral

## Why

Normal movement already behaves like a pixel-grid engine:

- `vel` is accumulated as a float
- movement is converted into integer pixel steps in `MoveEntPixelStep()`
- `pos` is then advanced one whole pixel at a time

So under normal simulation, physics ents tend to stay on integer pixel coordinates.

Problems appear when code bypasses the normal move/sweep path and writes `ent.pos` directly from:

- `SetCenter(...)`
- `SetVisualCenterForEnt(...)`
- helper functions that spawn or teleport by center

Those paths can easily produce `.5` positions when:

- the ent has an odd-sized collision box
- the ent has an odd-sized visual box
- frame-data pbox / draw-offset math is involved

Once that happens, grounded checks, rest positions, and rendering can disagree.

## Engine Rule

For active physics ents:

- `ent.pos` should end a frame on integer pixel coordinates
- direct placement APIs should snap the resulting authoritative position
- placement by visual center is approximate when mapped back onto the integer lattice

For non-physics or purely visual placements:

- fractional positions are acceptable

Examples:

- particles: fine to stay fractional
- camera: fine to stay fractional
- held/back attachs with `has_physics = false`: lower priority, mostly visual
- active colliding ents: should not be fractional

## What Is Not A Problem

Odd-sized sprites or collision boxes are not inherently a problem.

They just imply that:

- the exact visual center may be at `.5`
- but the authoritative anchor can still be snapped to an integer

The correct invariant is not "visual center must be integral".
The correct invariant is "physics anchor must be integral".

## Dangerous APIs

These are dangerous in a pixel-step physics engine unless they snap:

- [ent.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ent.cpp)
  `Ent::SetCenter`
- [frame.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/common/frame.cpp)
  `SetVisualCenterForEnt`

As currently written, both can leave `ent.pos` fractional.

## Scan Method

Scan target:

- `SetCenter(...)`
- `SetVisualCenterForEnt(...)`

Interpretation:

- "violation" means an unsnapped direct placement path for an active physics ent, or a generic helper that can place one
- "expected exception" means editor/debug or attach code where the ent is explicitly non-physics / non-colliding

## High-Confidence Violations

### Generic unsafe placement helpers

- [ent.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ent.cpp)
  `Ent::SetCenter` writes unsnapped `pos`
- [frame.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/common/frame.cpp)
  `SetVisualCenterForEnt` writes unsnapped `pos`

These are root-level violations because many callsites inherit the problem from them.

### Teleport / direct relocation

- [teleporter.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/teleporter.cpp)
  teleports the holder by visual center
  Note:
  this file currently has a local snap after placement, so the immediate bug is mitigated there, but the helper it relies on is still unsafe.

### Generic spawn-by-center helpers for active ents

- [stage_break.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage_break.cpp)
  `SpawnEntAtCenter`
- [tile_spec.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/tile_spec.cpp)
  `SpawnEntAtCenter`
- [chest.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/chest.cpp)
  `SpawnEntAtCenter`
- [spider.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/spider.cpp)
  `SpawnEntAtCenter`

These helpers create active ents, call `SetEntAs(...)`, then place with `SetCenter(...)` and do not snap.

### Throw / proj spawn paths

- [throw.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/common/throw.cpp)
  thrown ents spawn with `spawned_ent->SetCenter(thrower.GetCenter())`
- [cobra.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/cobra.cpp)
  `SpawnCobraSpitEnt`
- [web_cannon.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/web_cannon.cpp)
  `SpawnWebBallEnt`
- [web_cannon.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/web_cannon.cpp)
  `SpawnCobwebEnt`

These are active world ents being center-placed with no snap.

### Scripted world spawns

- [giant_tiki_head.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/giant_tiki_head.cpp)
  `SpawnBoulderForHead`
- [stage_init.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage_init.cpp)
  `SpawnBowlingCaveman`
- [stage_init.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage_init.cpp)
  `SpawnOpposingBodySmackCaveman`

These use direct center placement for active physics ents and do not snap.

### Suspected / Needs Review

These sites were found by the same scan and are likely relevant, but they are a bit more context-sensitive than the list above.

- [stage_init.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage_init.cpp)
  `SpawnStageEntAtCenter`
  Generic stage helper that creates active ents and center-places them unsnapped.
- [skeleton.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/skeleton.cpp)
  `SpawnEntAtCenter`
  Same generic pattern as the other spawn helpers.
- [sac_altar.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/sac_altar.cpp)
  `SpawnEntAtCenter`
  Likely fine for some rewards, but still an unsnapped active spawn helper.
- [hang.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/common/hang.cpp)
  `SnapEntToClimbTileCenterline`
  This one is probably intentional lattice alignment, but it currently relies on `SetCenter(...)`, so odd-size ents could still end up fractional depending on anchor math.

These should be reviewed after the root setter policy is decided, because some may become automatically correct once the setters themselves are fixed.

## Expected Exceptions / Lower Priority

These are not counted as primary violations because they are debug/editor or explicitly attach-style placement.

- [playback_ui_ents.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/debug/playback_ui_ents.cpp)
  debug/editor spawn placement
- [stage_init.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage_init.cpp)
  `GiveHeldRockToEnt`
- [stage_init.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage_init.cpp)
  `SnapAttachedItemsToPlayer`
- [carry.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/common/carry.cpp)
  held/back attach sync
- [shopkeeper.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/shopkeeper.cpp)
  held pistol placement
- [baseball_bat.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/baseball_bat.cpp)
  mounted held bat placement

These should still be reviewed for visual crispness, but they are not the same class of bug as active physics ents ending up fractional in the world.

## Plan

### Phase 1: Root Policy

- decide whether `Ent::SetCenter` and `SetVisualCenterForEnt` snap by default for physics ents
- keep the rule simple: active physics ents should not be left fractional

### Phase 2: Immediate Bug Fixes

- keep teleporter snapped
- patch the high-confidence active-physics spawn / proj helpers so they snap authoritative `pos` after placement

Targets:

- `throw.cpp`
- `stage_break.cpp`
- `tile_spec.cpp`
- `chest.cpp`
- `spider.cpp`
- `cobra.cpp`
- `web_cannon.cpp`
- `giant_tiki_head.cpp`
- `stage_init.cpp` bowling / body-smack helpers

### Phase 3: Review Suspected Sites

- review `SpawnStageEntAtCenter`
- review skeleton / sac altar local spawn helpers
- review climb centerline snap behavior
- review attach / held-item placement only for visual crispness, not as a blocking physics bug

### Phase 4: Debug Guardrail

- add an invariant check for active physics ents that are not attached / held
- flag any end-of-frame fractional `pos`

This should catch future regressions immediately.
