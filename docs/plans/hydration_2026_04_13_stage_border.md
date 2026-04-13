# Hydration 2026-04-13: Stage Border And Bounds

Use this to resume the stage border / wrapping work on another machine.

## Repo

- `/home/vega/Coding/GameDev/Splonks/splonks-cpp`

## Current Status

The requested border/bounds refactor has not been implemented yet.

Important:

- there was discussion about adding stage-level boundary behavior
- no partial code for that landed
- `src/stage.hpp` still has the old single field:
  - `Tile stage_border_tile = Tile::CaveDirt;`

So there is no half-migrated state to clean up.

## What Was Just Decided

The intended direction is now:

1. keep border description stage-owned
2. split border tiles by side
3. allow `Tile::Air` to mean "no border on this side"
4. keep wrapping as axis-level behavior
5. keep void death as a separate stage behavior

Planned shape:

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

The design note for this is here:

- [docs/plans/stage_border_and_bounds_plan.md](/home/vega/Coding/GameDev/Splonks/splonks-cpp/docs/plans/stage_border_and_bounds_plan.md)

## Important Architectural Point

The user explicitly wants stage/generator code to set border tiles directly.

That means:

- remove reliance on `BorderTileForStageType(...)`
- do not replace settable border tiles with a hidden family lookup
- stage constructors and generators should assign border sides themselves

## Current Code Reality

Some systems already sample the old border tile's tile-archetype properties:

- render wrapper
- terrain lighting
- border collision sound
- hang probes

But core movement still hardcodes stage bounds as blocking.

Main files to inspect first:

- [src/entities/common/blocking_contact.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common/blocking_contact.cpp)
- [src/entities/common/physics.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common/physics.cpp)
- [src/entity.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entity.cpp)
- [src/entities/common/tile_contact.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common/tile_contact.cpp)
- [src/entities/common/hang.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common/hang.cpp)
- [src/render/tiles_and_entities.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/render/tiles_and_entities.cpp)
- [src/render/tile_lighting.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/render/tile_lighting.cpp)
- [src/render/terrain_lighting.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/render/terrain_lighting.cpp)
- [src/stage.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage.cpp)

## Tile Family Work Already Landed

Separate from the border discussion, the tile-family cleanup did land and build.

That work:

- moved tile family to `TileArchetype`
- changed dirt/gold/block family helpers to derive from a family anchor tile
- removed the old repeated stage-type family switch from the hot path
- threaded family tile through old room generation

Relevant files:

- [src/tile_archetype.hpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/tile_archetype.hpp)
- [src/tile_archetype.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/tile_archetype.cpp)
- [src/tile.hpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/tile.hpp)
- [src/tile.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/tile.cpp)
- [src/room.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/room.cpp)
- [src/stage.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage.cpp)
- [src/stage_gen/hd_mines.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage_gen/hd_mines.cpp)

Build status after that work:

- `cmake --build build -j4` passed

## Suggested Next Steps

Do this in order:

1. read the stage border design doc
2. replace `Stage::stage_border_tile` with a `StageBorder` struct
3. remove `BorderTileForStageType(...)`
4. update stage constructors/generators to set border sides explicitly
5. fix hardcoded stage-bound blocking/grounding to consult border sides + wrap flags
6. fix hang probes and wrapper rendering to use side border tiles
7. add the requested debug map:
   - one room
   - simple floor
   - no border tiles
   - left/right wrap
   - eventual void death if you fall out

## Good Prompt To Resume

Use something like:

```text
Read docs/plans/stage_border_and_bounds_plan.md and docs/plans/hydration_2026_04_13_stage_border.md.
Do not invent a new hidden border system.
Replace Stage::stage_border_tile with a StageBorder struct that has left/right/top/bottom tiles plus wrap_x, wrap_y, and void_death_y.
Then wire collision, grounding, hang probes, and wrapper rendering to that data.
After that, add the single-room wrap debug test map.
Build when done.
```
