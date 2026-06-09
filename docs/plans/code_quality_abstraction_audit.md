# Code Quality Abstraction Audit

## Purpose

This plan tracks code smells found during the broader post-gfxp cleanup audit.
The goal is to remove wrapper wrappers, redundant abstraction layers, stale
migration helpers, and duplicated local utility code without flattening useful
domain boundaries.

The rule of thumb:

- Keep helpers that add policy, ownership, validation, or shared behavior.
- Remove helpers that only rename another helper.
- Prefer one canonical gameplay helper returning fixed-point data.
- Convert to `FVec2` / `FAABB` only at render, debug, UI, audio, serialization,
  or authored-data boundaries.
- Do not duplicate fixed/render helper pairs unless the two paths truly do
  different work.

## High-Confidence Cleanup Targets

### Dead `FVec2` Nearest-Player Overloads

`src/player_queries.hpp` has `FVec2` overloads for:

- `FindNearestPlayerVid(...)`
- `FindNearestPlayer(...)`
- `FindNearestPlayerMut(...)`

These only call `ToFxVec2(...)` and forward to the fixed-point overloads. Current
call sites appear to already pass `FxVec2`, usually from `ent.GetCenter()` or
`ent.pos`.

Status:

- [x] Remove the `FVec2` overloads.
- [x] Keep the fixed-point overloads as the gameplay API.
- [x] If a render/debug caller later needs nearest-player queries, make that caller
  convert explicitly at the boundary.

### Duplicate Pixel-Trunc Helpers

`src/ents/baseball_bat.cpp` and `src/ents/mattock.cpp` both define:

```cpp
IVec2 ToWorldPixelTrunc(FxVec2 point)
```

The helper is just:

```cpp
IVec2::New(point.x.trunc_int(), point.y.trunc_int())
```

Status:

- [x] Move one shared helper into `src/fxp.hpp`, near `ToPixelIVec2Round(...)`.
- [x] Use the direct name `ToPixelIVec2Trunc(...)`.
- [x] Replace the local copies.

### Repeated `FloorDiv`

The same integer floor-division helper appears in several files:

- `src/stage_queries.cpp`
- `src/stage_bounds.cpp`
- `src/world_query.cpp`
- `src/ents/common/physics.cpp`
- `src/ents/web_cannon.cpp`
- `src/ents/flesh_guy.cpp`
- `src/sid.cpp`

Status:

- [x] Add one shared integer floor division helper in a low-level math utility.
- [x] Replace local copies.
- [x] Keep behavior identical for negative coordinates.

### One-Line AABB Center Wrapper

`src/world_query.cpp` has:

```cpp
FxVec2 GetAabbCenter(FxAABB aabb) {
    return aabb.center();
}
```

Status:

- [x] Delete the helper.
- [x] Call `aabb.center()` directly.

### One-Line Render Predicate Wrapper

`src/render/tiles_and_ents.cpp` has:

```cpp
bool ShouldRenderImmediateBorderBacking(...) {
    return IsImmediateBorderRingTile(...);
}
```

Status:

- [x] Inline the call because no policy is added.
- [x] Keep a named predicate only if immediate-border backing gets real render
  policy later.

## Medium-Confidence Cleanup Targets

### Duplicate 8-Way Aim Helpers

`src/ents/bow.cpp` and `src/ents/web_cannon.cpp` duplicate:

- `NormalizeDegrees(...)`
- `DiscreteAimDirection(...)`
- `DiscreteAimWorldAngle(...)`

Both weapons need the same 8-way aim semantics.

Status:

- [x] Extract a small shared helper for discrete weapon aim.
- [x] Return the fixed-point direction as the gameplay value.
- [x] Return or compute the world angle only where animation/rotation needs it.
- [x] Avoid making this more abstract than "8-way held-weapon aim" unless another
  weapon needs different semantics.

### Float And Fixed World-Wrap Query Overloads

`src/world_query.hpp` exposes both `FVec2` and `FxVec2` versions of:

- `GetNearestWorldDelta(...)`
- `GetNearestWorldPoint(...)`

The `FxVec2` versions are correct for gameplay. The `FVec2` versions are still
used by render, debug, presentation, and audio paths.

Status:

- [x] Keep both because the float versions remain presentation-side.
- [x] Do not use float overloads from authoritative gameplay files.
- [ ] Consider moving float overloads to a presentation helper if the boundary stays
  noisy.

### Flesh Guy Float/Fixed Tile Queries

`src/ents/flesh_guy.cpp` has both `FVec2` and `FxVec2` versions of:

- `WorldPosToUnwrappedTileCoord(...)`
- `QueryCollidableTileOrBorderSurfaceAtWorldPos(...)`

Status:

- [x] Audit whether the float overload is still needed.
- [x] If this file is purely gameplay, prefer fixed-point only.
- [x] If a float path is only for render/debug placement, move that conversion to
  the caller.

### Stage Spawning Authored-Position Naming

`src/stage_spawning.hpp` exposes helpers such as:

- `PlacePlayerAtAuthoredPosition(...)`
- `SpawnPlayerAtAuthoredPosition(...)`
- `SpawnPlayerForPlayerIdAtAuthoredPosition(...)`
- `SpawnStageEntAtAuthoredTopLeft(...)`
- `SpawnStageEntAtAuthoredCenter(...)`

Most call sites are debug stage authoring helpers and smoke/debug code, not
render code. The old `RenderPosition` name was stale terminology from before
fixed-point integration.

Status:

- [x] Rename float-position helper names from `Render...` to `Authored...`.
- [x] Do not create both fixed and render spawn APIs unless the float API is clearly
  an authored-data boundary.

## Things To Leave Alone

These looked wrapper-heavy in broad text scans, but they are not the same smell:

- `SimSnapshot`, `MakeSimSnapshot`, `RestoreSimSnapshot`, and related names.
  `Sim` means simulation snapshot domain, not fixed-point scaffolding.
- `gubsy_shell` render wrappers. They are boundary glue around the menu shell
  and C-ish API surface.
- `debug/playback_recording_io.cpp` read/write helpers. They are serialization
  structure and should stay explicit unless a separate serialization refactor is
  planned.
- SDL render wrappers such as `RenderWorldTexture(...)`. These add render-space
  policy and are not just conversion aliases.

## Suggested Cleanup Order

1. [x] Remove dead nearest-player `FVec2` overloads.
2. [x] Add shared `ToPixelIVec2Trunc(...)` and remove duplicate local copies.
3. [x] Centralize integer `FloorDiv(...)`.
4. [x] Delete one-line local wrappers in `world_query.cpp` and
   `render/tiles_and_ents.cpp`.
5. [x] Extract shared 8-way aim helper for bow/web cannon.
6. [x] Audit float world-query and flesh-guy overloads for authoritative gameplay
   use.
7. [x] Rename or reduce stage spawning float-position helpers.

## Validation

For the low-risk cleanup pass:

```bash
./scripts/build.sh
git diff --check
```

If any gameplay helper is moved across files, run a quick local playtest around
the affected mechanic: bow, web cannon, mattock, baseball bat, and stage spawn
debug scenarios.
