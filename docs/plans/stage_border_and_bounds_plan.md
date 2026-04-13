# Stage Border And Bounds Plan

## Goal

Keep border appearance and border behavior explicit at the stage level.

We already have a useful concept:

- `stage_border_tile`

That should stay settable by the stage or generator.

What is missing is the ability to express:

- no visible / no blocking border
- horizontal wrapping
- vertical wrapping
- void-fall maps where you can leave the stage and die later
- asymmetric border tiles, such as different left/right/top/bottom walls

The current codebase does not model those separately.
Instead, several systems hardcode "outside the stage means a blocking wall."

## Recommended Data Shape

Use a dedicated stage-owned struct.

```cpp
struct StageBorderSide {
    Tile tile = Tile::Air;
};

struct StageBorder {
    StageBorderSide left;
    StageBorderSide right;
    StageBorderSide top;
    StageBorderSide bottom;
    bool wrap_x = false;
    bool wrap_y = false;
    int void_death_y = -1;
};
```

Then `Stage` owns:

```cpp
StageBorder border;
```

## Why This Shape

This keeps the model simple:

- side tiles are visual + physical descriptions of each stage edge
- wrap is axis-level behavior, not per-side behavior
- `Tile::Air` means "no border tile on this side"
- `void_death_y` is independent from wrapping and border art

Important constraint:

- if `wrap_x` is true, left/right border tiles should not block movement
- if `wrap_y` is true, top/bottom border tiles should not block movement

That is why wrap belongs on the axis, not on each side.

## Semantics

### Border tile

Border tiles should continue to use tile archetype properties.

That means the side tile can define:

- solidity
- hangability
- climbability
- collide sound
- render appearance
- lighting behavior

### Air border

`Tile::Air` on a border side means:

- render nothing for that side
- do not block on that side
- do not emit border collision sound on that side

### Wrapping

There are two distinct wrap modes and they should not be conflated.

#### Delayed wrap through border air

This is the current target.

- leaving the stage does **not** immediately sample tiles from the opposite side
- outside the authored stage you should see border air plus the stage background
- collision queries outside the stage should stay outside the stage
- after the entity travels far enough past the edge, we normalize it back in on the opposite side

This is good for space-style maps.

#### Seamless torus wrap

This is a separate future feature.

- the left edge literally neighbors the right edge for rendering, collision, and entity queries
- tiles and entities from the opposite edge are visible and interactive at the seam
- this requires seam-aware rendering and broad collision sampling

That is much more invasive and should not reuse the same implementation path as delayed wrap.

For now:

- `wrap_x` means delayed left/right wrap
- `wrap_y` means delayed top/bottom wrap

If wrapping is enabled on an axis, movement/collision should not treat leaving the map on that axis as a block.

### Void death

`void_death_y` should be a world Y threshold.

Suggested semantics:

- negative value means disabled
- if an entity's relevant body position goes below `void_death_y`, kill it

This is separate from border collision.
It is what enables "fall out of the map and die later."

## Current Problems In Code

Right now outside-stage behavior is inconsistent and mostly hardcoded.

### Good / already tile-driven

These places already sample `stage_border_tile` or border tile archetype data:

- [src/render/tiles_and_entities.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/render/tiles_and_entities.cpp)
- [src/render/tile_lighting.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/render/tile_lighting.cpp)
- [src/render/terrain_lighting.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/render/terrain_lighting.cpp)
- [src/entities/common/tile_contact.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common/tile_contact.cpp)
- [src/entities/common/hang.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common/hang.cpp)

### Bad / still hardcoded as solid stage bounds

These are the main places that still assume leaving the map means a hard wall or floor:

- [src/entities/common/blocking_contact.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common/blocking_contact.cpp)
- [src/entities/common/physics.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common/physics.cpp)
- [src/entity.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entity.cpp)
- [src/stage.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage.cpp)
- [src/hitscan.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/hitscan.cpp)
- [src/entities/rope.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/rope.cpp)

## Implementation Plan

### 1. Replace single border tile with `StageBorder`

Change `Stage` to own the new border struct.

Do not hide this behind a generic helper layer.
The data should be visible and boring.

### 2. Remove `BorderTileForStageType(...)`

We no longer want a stage-type switch for border family selection.

Instead:

- each stage constructor sets its own border explicitly
- each generator sets its own border explicitly

Examples:

- caves/mines can set all four sides to cave dirt
- blank/debug maps can set sides independently
- space-style map can leave all four as `Tile::Air`

### 3. Make stage-bound collision side-aware

When a movement probe leaves the stage:

- detect which side was exited
- consult that side's border tile
- if the axis wraps, do not block
- if the side tile is `Air`, do not block
- otherwise use that side tile's solidity

This is the first place where left/right/top/bottom really matter.

### 4. Make grounding side-aware

Current code grounds entities at the bottom of the map automatically.
That is wrong for no-border or wrap maps.

Bottom-of-stage grounding should only happen when:

- bottom does not wrap
- bottom border tile is solid

Otherwise, falling past the stage should remain falling.

### 5. Make hang probes use side border data

Current hang logic samples only the old single border tile.
It should instead sample:

- left border tile when probing left outside stage
- right border tile when probing right outside stage
- top border tile when probing above stage

and only when the relevant axis does not wrap.

### 6. Make render wrapper use side tiles

Outside-stage wrapper rendering should:

- draw left side using left border tile
- draw right side using right border tile
- draw top side using top border tile
- draw bottom side using bottom border tile
- draw nothing for sides whose tile is `Air`

Corner behavior can be simple:

- prefer the side being iterated
- no need for special corner tiles yet

### 7. Add stage wrapping

After the border struct is in place:

- add X wrap in entity stepping / post-physics position normalization
- add Y wrap later if desired, but the data model should support it now

This should be explicit stage behavior, not hidden in collision queries.

### 8. Add void-death pass

After entity stepping, check:

- if `void_death_y >= 0`
- if an entity is below that threshold

and kill/deactivate according to the intended rule.

Player should die.
Other entities can probably die too unless we later want per-entity exemptions.

## First Debug Test Map

Add a new debug level preset with these properties:

- one room
- simple floor
- left/right wrap enabled
- top/bottom wrap disabled
- all border sides set to `Tile::Air`
- bottom `void_death_y` below the room so falling out kills eventually

This is the requested test area:

- a single chunk
- a floor
- no visible border walls
- wrap interaction on left/right

## Non-Goals For First Pass

Do not do these yet:

- corner-specific border tiles
- different wrap rules per side
- special camera wrap presentation
- projectile-only wrap
- fancy void effects

The first pass should just make the stage model correct and usable.

## Summary

Keep border tiles explicit and stage-owned.
Do not replace the border tile idea with something more abstract.

The real fix is:

- move border description into a stage border struct
- keep tiles per side
- keep wrap per axis
- keep void death separate
- make collision/ground/hang/render query that struct instead of hardcoding stage bounds
