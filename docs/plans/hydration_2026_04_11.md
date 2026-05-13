# Hydration 2026-04-11

Use this to resume work on another machine.

## Current Context

Repo:

- `/home/vega/Coding/GameDev/Splonks/splonks-cpp`

Recent work has focused on:

- HD mines / Cave 1 generation
- spec table cleanup
- display-state vs raw anim ownership

## Current Git State

Uncommitted modified files right now:

- [src/ents/common_frame.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/common_frame.cpp)
- [src/ent.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ent.cpp)
- [docs/plans/display_anim_migration_plan.md](/home/vega/Coding/GameDev/Splonks/splonks-cpp/docs/plans/display_anim_migration_plan.md)

Build status:

- `cmake --build build` passes

## Important Architecture Decisions

### Spec table

We want:

- enum-indexed lookup
- actual contiguous spec storage
- spec definitions to remain in per-ent files
- explicit startup population, not lazy fill wrapper

Current shape:

- [src/ent_spec_registry.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ent_spec_registry.cpp)
  has a module-level
  `std::array<EntSpec, kEntTypeCount> g_ent_specs`
- it is filled by `PopulateEntSpecsTable()`
- `GetEntSpec(...)` indexes that array directly
- `main.cpp` calls `PopulateEntSpecsTable()` before `State::New()`

This is intentional. Do not move spec definitions into the registry file.

### Display-state ownership

The old bug was that `StepAnimTimer(...)` in
[src/ents/common_frame.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/common_frame.cpp)
used to remap:

- `ent.type_ + ent.display_state -> anim`

every post-step.

That meant direct calls to `aframe_animator.SetAnim(...)` could get
silently stomped.

Current fix:

- `StepAnimTimer(...)` now only steps the animator
- `TrySetDisplayState(...)` in
  [src/ent.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ent.cpp)
  now immediately applies the mapped anim / animate flag / forced frame

So now:

- semantic callers use `TrySetDisplayState(...)`
- exact authored anim callers use raw `SetAnim(...)`
- post-step no longer silently overrides exact anim choice

## Display-State Migration Plan

Main doc:

- [docs/plans/display_anim_migration_plan.md](/home/vega/Coding/GameDev/Splonks/splonks-cpp/docs/plans/display_anim_migration_plan.md)

Key point:

- only `Player`, `Bat`, and `BaseballBat` are currently in
  [src/ent_display_states.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ent_display_states.cpp)

Recommended next cleanup in that area:

1. remove `BaseballBat` from `ent_display_states.cpp`
2. stop calling `TrySetDisplayState(...)` in
   [src/ents/baseball_bat.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/baseball_bat.cpp)
3. let baseball bat be fully raw-anim-driven

Reason:

- it already uses the animator frame index directly for swing gameplay timing
- it only really has one authored swing anim
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

- many Cave 1 ents are still stub behavior-wise:
  - [src/ents/snake.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/snake.cpp)
  - [src/ents/caveman.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/caveman.cpp)
  - [src/ents/shopkeeper.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/shopkeeper.cpp)
  - [src/ents/spider_hang.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/spider_hang.cpp)
  - [src/ents/scarab.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/scarab.cpp)
  - [src/ents/arrow_trap.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/arrow_trap.cpp)
- no key / Udjat chain yet

So:

- Cave 1 generation is mostly there as content placement
- Cave 1 gameplay parity is not complete yet

## Suggested Next Step

If resuming immediately, do this first:

1. finish the baseball bat anim cleanup

Then choose one of:

2. continue display-state migration for `Player` / `Bat`
3. move back to Cave 1 gameplay parity and implement trap / enemy behavior

## Short Prompt To Resume

Use something like:

```text
Read docs/plans/hydration_2026_04_11.md and docs/plans/display_anim_migration_plan.md.
We already removed the hidden display-state overwrite from common_frame.
Pick up by moving BaseballBat fully off display-state mapping and onto direct SetAnim.
Then build.
```
