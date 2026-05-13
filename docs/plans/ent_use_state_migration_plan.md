# Ent Use State Migration Plan

## Goal

Split item use intent from item internal state.

Right now several items treat `EntState::InUse` as if it means "the holder is
currently pressing use on me". That is too narrow and too lossy.

We want:

- a real `UseState` on `Ent`
- explicit writer helpers:
  - `UseEnt(...)`
  - `StopUsingEnt(...)`
- an explicit spec use callback
- item FSM state to stay in `EntState`

This gives us:

- press / hold / release edges
- hold duration in frames
- source information for held vs back use
- the user vid
- future chain-use / forced-use gimmicks without abusing `EntState`

## Current Problem

Today the use signal is smeared across:

- player input intent
- carry/back placement code
- item `EntState`

Examples:

- bomb arms itself when `state == InUse`
- rope unfolds when `state == InUse`
- jetpack thrusts while `state == InUse`

That means `EntState` is doing two jobs:

- internal item behavior state
- external "someone is currently using this" signal

Those should be separate.

## Target Shape

### `Ent` Runtime Data

Add a packed runtime struct on `Ent`:

```cpp
struct UseState {
    bool down = false;
    bool pressed = false;
    bool released = false;
    std::uint32_t frames = 0;
    std::optional<VID> user_vid;
    AttachMode source = AttachMode::None;
};
```

and:

```cpp
UseState use_state;
```

Use frames, not seconds. The sim is fixed-step and the authored timings are
already frame-minded.

### Writer API

Add two explicit helpers:

```cpp
void UseEnt(
    Ent& ent,
    std::optional<VID> user_vid,
    AttachMode source
);

void StopUsingEnt(Ent& ent);
```

Meaning:

- `UseEnt(...)`:
  - sets `down = true`
  - computes `pressed`
  - clears `released`
  - increments `frames`
  - stores `user_vid`
  - stores `source`
- `StopUsingEnt(...)`:
  - emits the release edge if use was active
  - clears active use
  - clears `user_vid`
  - resets `source`
  - resets `frames`

If we later need a hard clear with no release edge for destruction paths, add a
third helper:

```cpp
void ClearUseState(Ent& ent);
```

That should only exist if a real callsite needs it.

### Spec Callback

Add an explicit use callback in specs:

```cpp
using EntOnUse =
    void (*)(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio);
```

and:

```cpp
EntOnUse on_use = nullptr;
```

The callback is optional.

If an ent does not need use behavior, it pays nothing.

## Design Rules

### Rule 1

`EntState` is the ent's own FSM.

Examples:

- `Idle`
- `WindingUp`
- `Attacking`

It should not mean "the holder is pressing the use button".

### Rule 2

`UseState` is input/intention applied to the used ent.

Examples:

- pressed this frame
- currently held down
- released this frame
- held for 14 frames
- used from the back slot

### Rule 3

Only explicit writers may mutate `use_state`.

That means:

- carry code
- back-item code
- future chain-use systems

Do not introduce a hidden global use-authority pass that rewrites every ent.

### Rule 4

Cleanup is ownership-driven, not damage-driven.

Clear or stop use when:

- an item is dropped
- an item is thrown
- an item is detached from the user
- a holder loses control and drops it
- a holder disappears

Do not blindly clear use on every damage event if the item is still attached and
still legitimately being used.

### Rule 5

Ent deletion does not need to synthesize a release event.

If the used ent dies that frame, its `released` edge does not matter because
there is no surviving receiver to observe it.

## Threading Points

These are the concrete places to thread the new API through.

### 1. Ent Data Definition

- [src/ent.hpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ent.hpp)

Work:

- add `UseState`
- add `use_state` to `Ent`
- declare `UseEnt(...)` and `StopUsingEnt(...)`

### 2. Ent Reset / Initialization

- [src/ent.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ent.cpp)

Work:

- zero `use_state` in `Ent::New()`
- ensure `Ent::Reset()` and `SetEntAs(...)` produce a clean use state

### 3. Spec Shape

- [src/ent_spec.hpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ent_spec.hpp)

Work:

- add `EntOnUse`
- add `on_use` field to `EntSpec`

### 4. Spec Registry

- [src/ent_spec_registry.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ent_spec_registry.cpp)

Work:

- no behavior change needed beyond carrying the new field
- later ents opt in by setting `on_use`

### 5. Carry / Held Item Writer Path

- [src/ents/common_carry.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/common_carry.cpp)

This is the main writer.

Work:

- held item path:
  - replace `state = InUse / Idle` writes with `UseEnt(...)` and
    `StopUsingEnt(...)`
- back item path:
  - replace `state = InUse / Idle` writes with `UseEnt(...)` and
    `StopUsingEnt(...)`
- when taking an item off the back:
  - stop using it
- when throwing a held item:
  - stop using it
- when moving held item to back:
  - stop the held path and let the back path become the writer
- when carry references are cleaned because the other ent went inactive:
  - stop using the orphaned item if it still exists

This file should stay the explicit authority for attach-driven use.

### 6. Item Logic Migration

These ents currently read use out of `EntState` and should move to
`use_state`.

- [src/ents/bomb.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/bomb.cpp)
- [src/ents/rope.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/rope.cpp)
- [src/ents/jetpack.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/jetpack.cpp)

Migration shape:

- bomb:
  - arm on `use_state.pressed`
  - once armed, own the rest through `EntState::WindingUp`
- rope:
  - unfold on `use_state.pressed`
  - once unfolding, own the rest through `EntState::WindingUp`
- jetpack:
  - thrust while `use_state.down`
  - fuel / travel sound stays internal

These are the first three that matter. They prove the split.

### 7. Step Dispatch

- [src/step_ents.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/step_ents.cpp)

Use should get its own explicit dispatch path.

Recommendation:

- add `spec.on_use(...)`
- call it from step dispatch when the ent has a meaningful use event:
  - `use_state.down`
  - `use_state.pressed`
  - `use_state.released`
- keep normal `step_logic` for the ent FSM and non-use behavior

Reason:

- this matches the clean shape from `gauche`: one narrow public use path, then
  explicit item-specific handlers
- keeps use behavior separate from generic per-ent logic
- avoids re-overloading `EntState` or smearing use checks back through step
  code
- makes future chain-use / forced-use gimmicks cleaner because they target one
  well-defined callback path

Rule:

- writer systems only update `UseState`
- the dispatcher decides whether `on_use` runs
- item FSM state still lives in `EntState`

### 8. Damage / Death / Deactivation

- [src/ents/common_damage.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/common_damage.cpp)
- [src/ents/common_step.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/common_step.cpp)

Work:

- do not add broad damage-driven clears
- if a specific path forcibly detaches or destroys a still-existing used item,
  use `StopUsingEnt(...)` or `ClearUseState(...)`
- deactivation of the used ent itself does not need a release event

### 9. Future Non-Carry Writers

Not required for the first pass, but this design supports:

- chain-use ents
- forced-use traps
- linked item clusters
- AI-driven use

Any future system that wants to drive use should call the same helpers instead
of writing `use_state` fields by hand.

## Suggested Implementation Order

1. add `UseState`, `UseEnt(...)`, `StopUsingEnt(...)`
2. thread them through [src/ents/common_carry.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/common_carry.cpp)
3. migrate bomb
4. migrate rope
5. migrate jetpack
6. delete the remaining `EntState::InUse` use-as-input assumptions
7. migrate bomb, rope, and jetpack to `on_use`

## Why `on_use` Is Worth It

`gauche` is clean here because item use has its own narrow dispatch path.
`Splonks` should keep that property.

The clean split is:

- `UseEnt(...)` / `StopUsingEnt(...)` only mutate `UseState`
- `on_use(...)` handles use-driven behavior
- `step_logic(...)` handles normal ent behavior

That does add one callback slot, but it buys a clearer separation of concerns and
makes use-driven ents easier to reason about.
