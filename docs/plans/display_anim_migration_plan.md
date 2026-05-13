# Display Animation Migration Plan

## Problem

Right now `EntityDisplayState` is still the authoritative animation source for
any entity that has a display-state mapping.

The current path is:

1. game logic sets `entity.display_state` through `TrySetDisplayState(...)`
2. `CommonPostStep(...)` calls `StepAnimationTimer(...)`
3. `StepAnimationTimer(...)` looks up `type_ + display_state`
4. if a mapping exists, it overwrites `entity.frame_data_animator`
5. only then does the animator step frames

That means direct calls like:

```cpp
entity.frame_data_animator.SetAnimation(frame_data_ids::LiveGrenade);
```

are not really authoritative if the entity also has a display-state mapping.
They can be stomped on the same frame or the next frame by
`StepAnimationTimer(...)`.

This makes it harder to add:

- one-off action animations
- short transitional animations
- entity-specific animation control
- richer authored animation behavior that does not fit a shared enum

## Current Authoritative Site

The important code is in:

- [src/entities/common_frame.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entities/common_frame.cpp)

Today `StepAnimationTimer(...)` does this every post-step:

- compute `GetFrameDataSelectionForDisplayState(...)`
- if one exists, call `entity.frame_data_animator.SetAnimation(...)`
- set animate / forced frame flags from that selection
- step the animator

So yes: `StepAnimationTimer(...)` currently knows about display state and treats
it as the animation authority.

## Current Scope

Right now the display-state mapping table in
[src/entity_display_states.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/entity_display_states.cpp)
only covers:

- `Player`
- `Bat`
- `BaseballBat`

So the display-state migration is not a whole-codebase animation rewrite yet.
It is mostly about cleaning up the ownership model for these few entities before
more animation complexity gets added.

## Goal

Make `FrameDataId` / `FrameDataAnimator` the real render truth.

Display states should become an optional helper layer for common semantic cases,
not the mandatory owner of animation choice.

Target model:

- rendering reads `entity.frame_data_animator`
- entity logic can set exact animations directly
- common logic may still use display-state helpers for broad cases
- display state no longer silently overwrites explicit animation choice

## Desired Rules

### Rule 1

`frame_data_animator.animation_id` is the actual animation being rendered.

### Rule 2

`EntityDisplayState` is only a semantic helper for common cases like:

- player locomotion
- bat hanging vs flying
- generic stunned / dead visuals where that abstraction still helps

### Rule 3

If entity logic sets an exact animation directly, that choice should persist
until logic changes it again.

### Rule 4

Geometry lookup should continue to use current frame data, but the current frame
data should come from the animator only, not from display-state remapping every
frame.

## Migration Shape

Do this in stages.

### Stage 1: Stop Automatic Display-State Override

Change `StepAnimationTimer(...)` so it no longer remaps
`display_state -> animation_id` every frame.

After this stage:

- `StepAnimationTimer(...)` should only step `frame_data_animator`
- explicit `SetAnimation(...)` calls become stable

This is the main ownership change.

### Stage 2: Add Explicit Helper For Semantic Mapping

Add a helper with a narrow job, something like:

```cpp
bool TrySetAnimationFromDisplayState(Entity& entity, EntityDisplayState display_state);
```

or:

```cpp
bool TryApplyDisplayStateAnimation(Entity& entity);
```

The important part is:

- semantic mapping happens only when code explicitly asks for it
- it does not happen as a hidden post-step side effect

### Stage 3: Keep `TrySetDisplayState(...)` As Validation Only

`TrySetDisplayState(...)` can still be useful, but its job should shrink to:

- validate that a display state is meaningful for that entity
- store the semantic state if desired

It should not imply that animation ownership has changed forever.

Possible shape:

```cpp
bool TrySetDisplayState(Entity& entity, EntityDisplayState display_state);
bool TrySetAnimationFromDisplayState(Entity& entity, EntityDisplayState display_state);
```

Then call sites pick the right one.

### Stage 4: Convert Common Logic To Explicit Animation Calls

Move common semantic cases to explicit helpers where they still make sense.

Examples:

- player movement code can still use display-state mapping helper
- bat AI can still use display-state mapping helper
- bomb / rope / jetpack / tools can directly set raw animation ids

### Stage 4A: Remove Baseball Bat From Display-State Mapping

`BaseballBat` should not really use semantic display states at all.

Reasons:

- it only has one authored swing animation
- gameplay already reads animator frame index directly for swing timing
- the display-state layer adds no real value here

Planned cleanup:

1. remove `BaseballBat` from `entity_display_states.cpp`
2. stop calling `TrySetDisplayState(...)` in baseball bat logic
3. set `frame_data_animator` directly with `SetAnimation(...)` or archetype init only

This is the clearest example of an entity that should be raw-animation-driven.

### Stage 5: Reassess Whether `display_state` Still Belongs On `Entity`

After the cutover, decide whether `display_state` should remain as:

- a semantic runtime field for some entities
- or only a convenience concept used at call sites

No need to force this immediately.

## Good First Cut

The first safe pass should be:

1. remove display-state remap from `StepAnimationTimer(...)`
2. add explicit helper to map display state to animation
3. update the player and bat logic to call that helper directly
4. move baseball bat to direct `SetAnimation(...)`
5. leave other raw `SetAnimation(...)` paths alone

That gets rid of the hidden stomp without requiring a giant rewrite.

## Open Questions

### Forced Frames / Animate Flags

`GetFrameDataSelectionForDisplayState(...)` currently also returns:

- `animate`
- `forced_frame`

That means the display-state helper should still be allowed to write:

- `animation_id`
- `animate`
- optional forced frame

That is fine. It just should not happen implicitly every post-step.

### Geometry Size Changes

`ApplyFrameDataGeometryToEntity(...)` currently resizes `entity.size` from the
current frame's `pbox`.

That behavior can stay, but it should follow the animator's actual current frame.
It should not depend on a hidden display-state remap step.

### Debug UI

If `display_state` becomes only a semantic helper, debug UI should present it as:

- semantic state
- not necessarily the currently rendered animation

That distinction is healthier.

## Recommendation

Keep `EntityDisplayState`, but demote it.

Do not make it the global animation graph.

Use:

- raw `FrameDataId` for actual animation authority
- display-state mapping only as explicit helper logic for shared semantic cases

That gives more animation freedom without losing the convenience of common
state-driven sprite selection where it still helps.
