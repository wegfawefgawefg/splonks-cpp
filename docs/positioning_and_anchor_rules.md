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
- movement is converted into integer pixel steps in `MoveEntityPixelStep()`
- `pos` is then advanced one whole pixel at a time

So under normal simulation, physics entities tend to stay on integer pixel coordinates.

Problems appear when code bypasses the normal move/sweep path and writes `entity.pos` directly from:

- `SetCenter(...)`
- `SetVisualCenterForEntity(...)`
- helper functions that spawn or teleport by center

Those paths can easily produce `.5` positions when:

- the entity has an odd-sized collision box
- the entity has an odd-sized visual box
- frame-data pbox / draw-offset math is involved

Once that happens, grounded checks, rest positions, and rendering can disagree.

## Engine Rule

For active physics entities:

- `entity.pos` should end a frame on integer pixel coordinates
- direct placement APIs should snap the resulting authoritative position
- placement by visual center is approximate when mapped back onto the integer lattice

For non-physics or purely visual placements:

- fractional positions are acceptable

Examples:

- particles: fine to stay fractional
- camera: fine to stay fractional
- held/back attachments with `has_physics = false`: lower priority, mostly visual
- active colliding entities: should not be fractional

## What Is Not A Problem

Odd-sized sprites or collision boxes are not inherently a problem.

They just imply that:

- the exact visual center may be at `.5`
- but the authoritative anchor can still be snapped to an integer

The correct invariant is not "visual center must be integral".
The correct invariant is "physics anchor must be integral".

## Dangerous APIs

These are dangerous in a pixel-step physics engine unless they snap:

- [entity.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entity.cpp)
  `Entity::SetCenter`
- [frame.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common/frame.cpp)
  `SetVisualCenterForEntity`

As currently written, both can leave `entity.pos` fractional.

## Scan Method

Scan target:

- `SetCenter(...)`
- `SetVisualCenterForEntity(...)`

Interpretation:

- "violation" means an unsnapped direct placement path for an active physics entity, or a generic helper that can place one
- "expected exception" means editor/debug or attachment code where the entity is explicitly non-physics / non-colliding

## High-Confidence Violations

### Generic unsafe placement helpers

- [entity.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entity.cpp)
  `Entity::SetCenter` writes unsnapped `pos`
- [frame.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common/frame.cpp)
  `SetVisualCenterForEntity` writes unsnapped `pos`

These are root-level violations because many callsites inherit the problem from them.

### Teleport / direct relocation

- [teleporter.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/teleporter.cpp)
  teleports the holder by visual center
  Note:
  this file currently has a local snap after placement, so the immediate bug is mitigated there, but the helper it relies on is still unsafe.

### Generic spawn-by-center helpers for active entities

- [stage_break.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage_break.cpp)
  `SpawnEntityAtCenter`
- [tile_archetype.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/tile_archetype.cpp)
  `SpawnEntityAtCenter`
- [chest.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/chest.cpp)
  `SpawnEntityAtCenter`
- [spider.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/spider.cpp)
  `SpawnEntityAtCenter`

These helpers create active entities, call `SetEntityAs(...)`, then place with `SetCenter(...)` and do not snap.

### Throw / projectile spawn paths

- [throw.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common/throw.cpp)
  thrown entities spawn with `spawned_entity->SetCenter(thrower.GetCenter())`
- [cobra.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/cobra.cpp)
  `SpawnCobraSpitEntity`
- [web_cannon.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/web_cannon.cpp)
  `SpawnWebBallEntity`
- [web_cannon.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/web_cannon.cpp)
  `SpawnCobwebEntity`

These are active world entities being center-placed with no snap.

### Scripted world spawns

- [giant_tiki_head.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/giant_tiki_head.cpp)
  `SpawnBoulderForHead`
- [stage_init.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage_init.cpp)
  `SpawnBowlingCaveman`
- [stage_init.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage_init.cpp)
  `SpawnOpposingBodySmackCaveman`

These use direct center placement for active physics entities and do not snap.

### Suspected / Needs Review

These sites were found by the same scan and are likely relevant, but they are a bit more context-sensitive than the list above.

- [stage_init.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage_init.cpp)
  `SpawnStageEntityAtCenter`
  Generic stage helper that creates active entities and center-places them unsnapped.
- [skeleton.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/skeleton.cpp)
  `SpawnEntityAtCenter`
  Same generic pattern as the other spawn helpers.
- [sac_altar.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/sac_altar.cpp)
  `SpawnEntityAtCenter`
  Likely fine for some rewards, but still an unsnapped active spawn helper.
- [hang.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common/hang.cpp)
  `SnapEntityToClimbTileCenterline`
  This one is probably intentional lattice alignment, but it currently relies on `SetCenter(...)`, so odd-size entities could still end up fractional depending on anchor math.

These should be reviewed after the root setter policy is decided, because some may become automatically correct once the setters themselves are fixed.

## Expected Exceptions / Lower Priority

These are not counted as primary violations because they are debug/editor or explicitly attachment-style placement.

- [playback_ui_entities.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/debug/playback_ui_entities.cpp)
  debug/editor spawn placement
- [stage_init.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage_init.cpp)
  `GiveHeldRockToEntity`
- [stage_init.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage_init.cpp)
  `SnapAttachedItemsToPlayer`
- [carry.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common/carry.cpp)
  held/back attachment sync
- [shopkeeper.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/shopkeeper.cpp)
  held pistol placement
- [baseball_bat.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/baseball_bat.cpp)
  mounted held bat placement

These should still be reviewed for visual crispness, but they are not the same class of bug as active physics entities ending up fractional in the world.

## Plan

### Phase 1: Root Policy

- decide whether `Entity::SetCenter` and `SetVisualCenterForEntity` snap by default for physics entities
- keep the rule simple: active physics entities should not be left fractional

### Phase 2: Immediate Bug Fixes

- keep teleporter snapped
- patch the high-confidence active-physics spawn / projectile helpers so they snap authoritative `pos` after placement

Targets:

- `throw.cpp`
- `stage_break.cpp`
- `tile_archetype.cpp`
- `chest.cpp`
- `spider.cpp`
- `cobra.cpp`
- `web_cannon.cpp`
- `giant_tiki_head.cpp`
- `stage_init.cpp` bowling / body-smack helpers

### Phase 3: Review Suspected Sites

- review `SpawnStageEntityAtCenter`
- review skeleton / sac altar local spawn helpers
- review climb centerline snap behavior
- review attachment / held-item placement only for visual crispness, not as a blocking physics bug

### Phase 4: Debug Guardrail

- add an invariant check for active physics entities that are not attached / held
- flag any end-of-frame fractional `pos`

This should catch future regressions immediately.
