# Hydration: 2026-04-12

Repo: `/home/vega/Coding/GameDev/Splonks/splonks-cpp`

Current commit:

`f151bb7`

Worktree status:

`git status --short` was clean when this file was written.

## Current Shape

Recent file-ownership cleanup is done and builds cleanly.

Important folder layout now:

- `src/ent/`: ent framework layer
- `src/ents/`: concrete ent implementations
- `src/render/`: render system
- `src/render/menu/`: menu drawing code
- `src/menu/`: menu logic/input/state mutation
- `src/debug/`: debug playback + debug UI
- `src/tools/`: tool specs and tool behaviors
- `src/ents/common/`: shared ent behavior

## Ent Layout

Ent framework files were moved out of root and into `src/ent/`.

Current framework files:

- `src/ent/spec.*`
- `src/ent/spec_registry.cpp`
- `src/ent/core_types.hpp`
- `src/ent/display_states.*`
- `src/ent/display_support.hpp`
- `src/ent/draw_layer.hpp`
- `src/ent/manager.*`

Two dumb alias headers were pulled out to root because they are not really ent-owned modules:

- `src/vid.hpp`
- `src/origin.hpp`

Those both currently just include `src/ent/core_types.hpp`.

Important caveat:

- `VID`
- `Origin`
- `DamageType`
- `DrawLayer`
- `EntMovementFlag`
- `EntDisplayState`

still physically live inside `src/ent/core_types.hpp`.

So the file ownership is cleaner, but the type ownership is still partially mixed.

## Render Layout

Render was folderified and then split by responsibility.

Current render files:

- `src/render/render.cpp`: top-level dispatcher + final pres
- `src/render/gameplay.cpp`: in-game scene render + stage transition/game over/win screens
- `src/render/postfx.*`
- `src/render/debug.*`
- `src/render/ui.*`
- `src/render/tiles_and_ents.*`
- `src/render/stone_overlay.*`
- `src/render/tile_lighting.*`: draw-side tile lighting helpers
- `src/render/terrain_lighting.*`: lighting cache/update logic

Menu draw files now live in `src/render/menu/`:

- `common.*`
- `top_level.cpp`
- `settings.cpp`
- `lighting.cpp`

Menu logic still lives separately in `src/menu/`, which is intentional.

## Debug Layout

Debug playback/UI was moved into `src/debug/`.

Current files there:

- `playback.hpp`
- `playback_internal.hpp`
- `playback_core.cpp`
- `playback_serialization.cpp`
- `playback_ui_common.cpp`
- `playback_ui_ents.cpp`
- `playback_ui_menu.cpp`
- `playback_ui_settings.cpp`

## State / Ent Runtime Cleanup Already Done

These behavior cleanups landed before the file-layout work and are part of the current codebase:

- removed stored `EntState`
- removed stored `ent.display_state`
- `TrySetAnim(ent, EntDisplayState)` still exists as the semantic mapper
- movement now uses `EntMovementFlag`
- hanging/climbing moved off old booleans into movement flags plus `hang_side`
- `EntCondition` is now the main durable condition bucket: `Normal`, `Dead`, `Stunned`
- render-side menu code is split from menu logic code
- ent specs are populated centrally via the spec table

## Build Status

This command passed after the latest cleanup:

```bash
cmake --build /home/vega/Coding/GameDev/Splonks/splonks-cpp/build --target splonks-cpp
```

## What Still Looks Weird

These are the obvious next cleanup candidates, not active breakages:

1. `src/ent/core_types.hpp` is still too mixed.
   It contains real ent-owned types and generic-ish types together.

2. `VID` is broader than ents.
   It probably wants to live in its own real header eventually instead of only being re-exported by `vid.hpp`.

3. `Origin` is really a sprite/render concept, not an ent concept.
   It probably wants its own real home eventually.

4. `DamageType` may also be broader than ents.
   Same issue as `VID`: current alias/file layout is better than before, but not fully resolved.

5. `src/render/menu/lighting.cpp` is still a bit large because the lighting screen itself has a lot of line rendering.

## Suggested Next Pass

If continuing the cleanup thread, the most sensible next step is:

1. split `src/ent/core_types.hpp` into a few real headers by ownership
2. make `vid.hpp` define `VID` for real
3. make `origin.hpp` define `Origin` for real
4. move `DamageType` to a better-owned header if desired

If switching back to gameplay/worldgen instead, the cleanup is in a good enough state to stop here.

## Good Prompt To Resume

Use something like:

> Read `docs/hydration_2026-04-12.md` first. We recently folderified debug/menu/render, moved the ent framework into `src/ent`, and kept concrete ents in `src/ents`. Build is currently clean. Continue from there.
