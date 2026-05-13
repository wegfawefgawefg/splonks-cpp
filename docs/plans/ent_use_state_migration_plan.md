# Entity Use State Migration Plan

## Goal

Split item use intent from item internal state.

Right now several items treat `EntityState::InUse` as if it means "the holder is
currently pressing use on me". That is too narrow and too lossy.

We want:

- a real `UseState` on `Entity`
- explicit writer helpers:
  - `UseEntity(...)`
  - `StopUsingEntity(...)`
- an explicit archetype use callback
- item FSM state to stay in `EntityState`

This gives us:

- press / hold / release edges
- hold duration in frames
- source information for held vs back use
- the user vid
- future chain-use / forced-use gimmicks without abusing `EntityState`

## Current Problem

Today the use signal is smeared across:

- player input intent
- carry/back placement code
- item `EntityState`

Examples:

- bomb arms itself when `state == InUse`
- rope unfolds when `state == InUse`
- jetpack thrusts while `state == InUse`

That means `EntityState` is doing two jobs:

- internal item behavior state
- external "someone is currently using this" signal

Those should be separate.

## Target Shape

### `Entity` Runtime Data

Add a packed runtime struct on `Entity`:

```cpp
struct UseState {
    bool down = false;
    bool pressed = false;
    bool released = false;
    std::uint32_t frames = 0;
    std::optional<VID> user_vid;
    AttachmentMode source = AttachmentMode::None;
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
void UseEntity(
    Entity& entity,
    std::optional<VID> user_vid,
    AttachmentMode source
);

void StopUsingEntity(Entity& entity);
```

Meaning:

- `UseEntity(...)`:
  - sets `down = true`
  - computes `pressed`
  - clears `released`
  - increments `frames`
  - stores `user_vid`
  - stores `source`
- `StopUsingEntity(...)`:
  - emits the release edge if use was active
  - clears active use
  - clears `user_vid`
  - resets `source`
  - resets `frames`

If we later need a hard clear with no release edge for destruction paths, add a
third helper:

```cpp
void ClearUseState(Entity& entity);
```

That should only exist if a real callsite needs it.

### Archetype Callback

Add an explicit use callback in archetypes:

```cpp
using EntityOnUse =
    void (*)(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio);
```

and:

```cpp
EntityOnUse on_use = nullptr;
```

The callback is optional.

If an entity does not need use behavior, it pays nothing.

## Design Rules

### Rule 1

`EntityState` is the entity's own FSM.

Examples:

- `Idle`
- `WindingUp`
- `Attacking`

It should not mean "the holder is pressing the use button".

### Rule 2

`UseState` is input/intention applied to the used entity.

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

Do not introduce a hidden global use-authority pass that rewrites every entity.

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

Entity deletion does not need to synthesize a release event.

If the used entity dies that frame, its `released` edge does not matter because
there is no surviving receiver to observe it.

## Threading Points

These are the concrete places to thread the new API through.

### 1. Entity Data Definition

- [src/entity.hpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entity.hpp)

Work:

- add `UseState`
- add `use_state` to `Entity`
- declare `UseEntity(...)` and `StopUsingEntity(...)`

### 2. Entity Reset / Initialization

- [src/entity.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entity.cpp)

Work:

- zero `use_state` in `Entity::New()`
- ensure `Entity::Reset()` and `SetEntityAs(...)` produce a clean use state

### 3. Archetype Shape

- [src/entity_archetype.hpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entity_archetype.hpp)

Work:

- add `EntityOnUse`
- add `on_use` field to `EntityArchetype`

### 4. Archetype Registry

- [src/entity_archetype_registry.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entity_archetype_registry.cpp)

Work:

- no behavior change needed beyond carrying the new field
- later entities opt in by setting `on_use`

### 5. Carry / Held Item Writer Path

- [src/entities/common_carry.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common_carry.cpp)

This is the main writer.

Work:

- held item path:
  - replace `state = InUse / Idle` writes with `UseEntity(...)` and
    `StopUsingEntity(...)`
- back item path:
  - replace `state = InUse / Idle` writes with `UseEntity(...)` and
    `StopUsingEntity(...)`
- when taking an item off the back:
  - stop using it
- when throwing a held item:
  - stop using it
- when moving held item to back:
  - stop the held path and let the back path become the writer
- when carry references are cleaned because the other entity went inactive:
  - stop using the orphaned item if it still exists

This file should stay the explicit authority for attachment-driven use.

### 6. Item Logic Migration

These entities currently read use out of `EntityState` and should move to
`use_state`.

- [src/entities/bomb.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/bomb.cpp)
- [src/entities/rope.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/rope.cpp)
- [src/entities/jetpack.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/jetpack.cpp)

Migration shape:

- bomb:
  - arm on `use_state.pressed`
  - once armed, own the rest through `EntityState::WindingUp`
- rope:
  - unfold on `use_state.pressed`
  - once unfolding, own the rest through `EntityState::WindingUp`
- jetpack:
  - thrust while `use_state.down`
  - fuel / travel sound stays internal

These are the first three that matter. They prove the split.

### 7. Step Dispatch

- [src/step_entities.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/step_entities.cpp)

Use should get its own explicit dispatch path.

Recommendation:

- add `archetype.on_use(...)`
- call it from step dispatch when the entity has a meaningful use event:
  - `use_state.down`
  - `use_state.pressed`
  - `use_state.released`
- keep normal `step_logic` for the entity FSM and non-use behavior

Reason:

- this matches the clean shape from `gauche`: one narrow public use path, then
  explicit item-specific handlers
- keeps use behavior separate from generic per-entity logic
- avoids re-overloading `EntityState` or smearing use checks back through step
  code
- makes future chain-use / forced-use gimmicks cleaner because they target one
  well-defined callback path

Rule:

- writer systems only update `UseState`
- the dispatcher decides whether `on_use` runs
- item FSM state still lives in `EntityState`

### 8. Damage / Death / Deactivation

- [src/entities/common_damage.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common_damage.cpp)
- [src/entities/common_step.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common_step.cpp)

Work:

- do not add broad damage-driven clears
- if a specific path forcibly detaches or destroys a still-existing used item,
  use `StopUsingEntity(...)` or `ClearUseState(...)`
- deactivation of the used entity itself does not need a release event

### 9. Future Non-Carry Writers

Not required for the first pass, but this design supports:

- chain-use entities
- forced-use traps
- linked item clusters
- AI-driven use

Any future system that wants to drive use should call the same helpers instead
of writing `use_state` fields by hand.

## Suggested Implementation Order

1. add `UseState`, `UseEntity(...)`, `StopUsingEntity(...)`
2. thread them through [src/entities/common_carry.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common_carry.cpp)
3. migrate bomb
4. migrate rope
5. migrate jetpack
6. delete the remaining `EntityState::InUse` use-as-input assumptions
7. migrate bomb, rope, and jetpack to `on_use`

## Why `on_use` Is Worth It

`gauche` is clean here because item use has its own narrow dispatch path.
`Splonks` should keep that property.

The clean split is:

- `UseEntity(...)` / `StopUsingEntity(...)` only mutate `UseState`
- `on_use(...)` handles use-driven behavior
- `step_logic(...)` handles normal entity behavior

That does add one callback slot, but it buys a clearer separation of concerns and
makes use-driven entities easier to reason about.
