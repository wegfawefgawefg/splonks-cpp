# Fixed-Point Abstraction Cleanup

## Purpose

The gfxp integration and determinism audit moved authoritative gameplay state
to fixed-point types. That was the right direction, but the migration also left
behind some names and helpers that read like transitional scaffolding:

- `Sim*` names used only to mean "fixed-point".
- Duplicate fixed/render helper pairs.
- One-line wrappers that only rename a conversion.
- Repeated render helper code copied between files.

This plan tracks the follow-up cleanup. The target is not to hide fixed-point
math. The target is to make fixed-point geometry feel like the normal gameplay
language, with float conversion visible only at render/debug/UI/audio/tooling
boundaries.

## Naming Rules

- `FxScalar`, `FxVec2`, `FxAABB`, and `FxColor3` are the fixed-point types.
- `FVec2`, `FAABB`, and float colors are presentation/render-side types.
- In gameplay code, fixed-point is implied by the type. Do not add `Sim` to a
  helper name just because it returns `Fx*`.
- Keep `SimSnapshot`, `MakeSimSnapshot`, `RestoreSimSnapshot`,
  `SerializeSimSnapshotToBytes`, and similar names. In those cases, `Sim`
  means "simulation snapshot" as a domain object, not "fixed-point version".
- Do not create mirror helper pairs such as `GetThing()` plus `GetFThing()` or
  `ThingAabb()` plus `RenderThingAabb()` unless both functions contain real
  distinct logic.
- Prefer one canonical gameplay helper returning `Fx*`; render/debug call sites
  can use `ToFVec2(...)` or `ToFAABB(...)` at the boundary.

## Cleanup Targets

### Stale `Sim` Names For Fixed Types

These names should be changed because they describe fixed-point payloads, not a
simulation snapshot/domain:

- [x] `RandomSimScalar` in `src/utils.hpp` / `src/utils.cpp`.
  - Rename to `RandomFxScalar`.
  - Update call sites in `sac_altar.cpp`, `skeleton.cpp`, and `spider.cpp`.
- [x] `ParseSimScalar` in `src/settings.cpp`.
  - Rename to `ParseFxScalar`.
- [x] `SliderSimScalar` and `DragSimScalar` in
  `src/debug/playback_ui_settings.cpp`.
  - Rename to `SliderFxScalar` and `DragFxScalar`.
- [x] `ReadSimScalar`, `WriteSimScalar`, `ReadSimVec2`, `WriteSimVec2`,
  `ReadSimColor3`, and `WriteSimColor3` in
  `src/debug/playback_recording_io.cpp`.
  - Rename to `ReadFxScalar`, `WriteFxScalar`, `ReadFxVec2`, `WriteFxVec2`,
    `ReadFxColor3`, and `WriteFxColor3`.
  - Keep `ReadSimSnapshot` / `WriteSimSnapshot` names.
- [x] `SimVec2DebugString` in `src/cli.cpp`.
  - Rename to `FxVec2DebugString`.
- [x] `SimPointAabb` in `src/ents/common/hang.cpp`.
  - Rename to `PointAabb`.

### Stale `kSim` Constants

These constants are typed as `FxScalar`, so the type already communicates that
they are fixed-point:

- [x] `kSimTriggerDistance` and `kSimTriggerHalfWidth` in
  `src/ents/thwomp_trap.cpp`.
- [x] `kSimShopkeeperRecoverPistolJumpHeightThreshold` in
  `src/ents/shopkeeper.cpp`.
- [x] `kSimRightSensorMinX`, `kSimRightSensorMaxX`, `kSimRightSensorMinY`, and
  `kSimRightSensorMaxY` in `src/ents/door.cpp`.

For each file, inspect nearby float constants before renaming. If a float
constant exists only to feed `ToFxScalar(...)`, collapse it into one fixed
constant where that improves readability. Do not remove a float constant that is
also used for presentation math.

### Duplicate Fixed/Render Helper Pairs

`src/ents/mattock.cpp` currently has:

- `RenderTileAabbForTilePos(...) -> FAABB`
- `SimTileAabbForTilePos(...) -> FxAABB`

This is the helper-pair shape we want to remove. The cleanup should be:

- [x] Replace both with one canonical `TileAabbForTilePos(...) -> FxAABB`.
- [x] Convert at render/debug call sites with `ToFAABB(TileAabbForTilePos(...))`.
- [x] Keep gameplay strike/query logic fixed all the way through.

`src/ents/bow.cpp` currently has:

- `DiscreteAimDirection(...) -> FVec2`
- `DiscreteSimAimDirection(...) -> FxVec2`
- `BowAim::direction`
- `BowAim::sim_direction`

This should be reviewed carefully because arrow velocity is gameplay state,
while render rotation is presentation. Likely cleanup:

- [x] Make fixed aim direction the canonical gameplay value.
- [x] Keep float/vector conversion only where presentation needs it.
- [x] Avoid storing both fixed and float direction if the float value can be
  derived or replaced by the existing world-angle path.

The same cleanup was also applied to `src/ents/web_cannon.cpp`, which had the
same `DiscreteAimDirection(...)` / `DiscreteSimAimDirection(...)` and
`direction` / `sim_direction` split. Web cannon now keeps one fixed aim
direction and converts it to `FVec2` only for web spray particles.

### One-Line Conversion Wrappers

Avoid reintroducing wrappers like:

```cpp
FVec2 Ent::GetRenderPos() const { return ToFVec2(pos); }
FAABB Ent::GetRenderAABB() const { return ToFAABB(GetAABB()); }
```

The current codebase should keep conversions explicit at call sites unless a
helper adds real logic, policy, validation, or shared behavior.

Audit command:

```bash
rg -n "GetF[A-Z]|SetF[A-Z]|GetRender[A-Z]|SetRender[A-Z]|ToRender|ToSim|GetSim|SetSim|return ToF|return ToFx|return ToFAABB|return ToFxAABB" src --glob '*.{hpp,cpp}'
```

### Duplicated Render Wrapping Helpers

The render code has repeated copies of:

- `VisibleWorldRect`
- `GetVisibleWorldRect`
- `FloorDivByFloat`
- `GetVisibleWrappedRenderOffsets`

Copies appear in:

- `src/render/debug.cpp`
- `src/render/ents.cpp`
- `src/render/particles.cpp`
- `src/render/tiles_and_ents.cpp`

This is not strictly gfxp migration debris, but it is abstraction redundancy
that showed up during this audit.

Cleanup target:

- [x] Move shared visible-world/wrapped-render-offset helpers into a small
  render helper module.
- [x] Keep it render-side only; this helper should not become gameplay world
  wrapping policy.
- [x] Replace local duplicate implementations with the shared helper.

## Things To Leave Alone

- `SimSnapshot` and related snapshot functions/types are valid domain names.
- Network lockstep references to simulation snapshots should keep `Sim` where
  they refer to the snapshot domain.
- `ToFVec2(...)`, `ToFAABB(...)`, `ToFxVec2(...)`, and `ToFxScalar(...)` are
  valid boundary/construction helpers. The problem is wrapping them in
  one-line functions that only rename the conversion.

## Validation

After cleanup:

- [x] `./scripts/build.sh`
- [x] `git diff --check`
- [x] Audit for stale migration names:

```bash
rg -n "\\bRandomSim|\\bParseSim|\\bReadSimScalar|\\bWriteSimScalar|\\bReadSimVec2|\\bWriteSimVec2|\\bSliderSim|\\bDragSim|\\bGetSim|\\bSetSim|\\bToSim|\\bToRender|\\bkSim" src --glob '*.{hpp,cpp}'
```

Remaining hits should either be legitimate `SimSnapshot` domain names or
documented intentional exceptions.

Current exception:

- `GetSimulationTickInterval(...)` in `src/step.cpp` is a legitimate simulation
  timing name, not a fixed-point wrapper.
