# Hydration 2026-04-11

Use this to resume work on another machine.

## Current Context

Repo:

- `/home/vega/Coding/GameDev/Splonks/splonks-cpp`

Recent work has focused on:

- HD mines / Cave 1 generation
- archetype table cleanup
- display-state vs raw animation ownership

## Current Git State

Uncommitted modified files right now:

- [src/entities/common_frame.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common_frame.cpp)
- [src/entity.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entity.cpp)
- [docs/plans/display_animation_migration_plan.md](/home/vega/Coding/GameDev/Splonks/splonks-cpp/docs/plans/display_animation_migration_plan.md)

Build status:

- `cmake --build build` passes

## Important Architecture Decisions

### Archetype table

We want:

- enum-indexed lookup
- actual contiguous archetype storage
- archetype definitions to remain in per-entity files
- explicit startup population, not lazy fill wrapper

Current shape:

- [src/entity_archetype_registry.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entity_archetype_registry.cpp)
  has a module-level
  `std::array<EntityArchetype, kEntityTypeCount> g_entity_archetypes`
- it is filled by `PopulateEntityArchetypesTable()`
- `GetEntityArchetype(...)` indexes that array directly
- `main.cpp` calls `PopulateEntityArchetypesTable()` before `State::New()`

This is intentional. Do not move archetype definitions into the registry file.

### Display-state ownership

The old bug was that `StepAnimationTimer(...)` in
[src/entities/common_frame.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common_frame.cpp)
used to remap:

- `entity.type_ + entity.display_state -> animation`

every post-step.

That meant direct calls to `frame_data_animator.SetAnimation(...)` could get
silently stomped.

Current fix:

- `StepAnimationTimer(...)` now only steps the animator
- `TrySetDisplayState(...)` in
  [src/entity.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entity.cpp)
  now immediately applies the mapped animation / animate flag / forced frame

So now:

- semantic callers use `TrySetDisplayState(...)`
- exact authored animation callers use raw `SetAnimation(...)`
- post-step no longer silently overrides exact animation choice

## Display-State Migration Plan

Main doc:

- [docs/plans/display_animation_migration_plan.md](/home/vega/Coding/GameDev/Splonks/splonks-cpp/docs/plans/display_animation_migration_plan.md)

Key point:

- only `Player`, `Bat`, and `BaseballBat` are currently in
  [src/entity_display_states.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entity_display_states.cpp)

Recommended next cleanup in that area:

1. remove `BaseballBat` from `entity_display_states.cpp`
2. stop calling `TrySetDisplayState(...)` in
   [src/entities/baseball_bat.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/baseball_bat.cpp)
3. let baseball bat be fully raw-animation-driven

Reason:

- it already uses the animator frame index directly for swing gameplay timing
- it only really has one authored swing animation
- display state adds little value there

## Cave 1 / HD Mines Status

Generator file:

- [src/stage_gen/hd_mines.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage_gen/hd_mines.cpp)

What is in:

- 4x4 mines layout/path generation
- room templates for start/end/main/side/drop/shop/snake pit
- glyph interpretation for tiles, shops, altar/kali, idol, tiki, dice/craps, signs,
  lanterns, chest, damsel, mattock
- embedded treasure pass
- visible treasure pass
- block to arrow-trap conversion
- ambient spawns for bats, spiders, giant spider, snakes, cavemen, rocks, etc.

What is not feature-complete yet:

- many Cave 1 entities are still stub behavior-wise:
  - [src/entities/snake.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/snake.cpp)
  - [src/entities/caveman.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/caveman.cpp)
  - [src/entities/shopkeeper.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/shopkeeper.cpp)
  - [src/entities/spider_hang.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/spider_hang.cpp)
  - [src/entities/scarab.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/scarab.cpp)
  - [src/entities/arrow_trap.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/arrow_trap.cpp)
- no key / Udjat chain yet

So:

- Cave 1 generation is mostly there as content placement
- Cave 1 gameplay parity is not complete yet

## Suggested Next Step

If resuming immediately, do this first:

1. finish the baseball bat animation cleanup

Then choose one of:

2. continue display-state migration for `Player` / `Bat`
3. move back to Cave 1 gameplay parity and implement trap / enemy behavior

## Short Prompt To Resume

Use something like:

```text
Read docs/plans/hydration_2026_04_11.md and docs/plans/display_animation_migration_plan.md.
We already removed the hidden display-state overwrite from common_frame.
Pick up by moving BaseballBat fully off display-state mapping and onto direct SetAnimation.
Then build.
```
