# Display Anim Migration Plan

## Problem

Right now `EntDisplayState` is still the authoritative anim source for
any ent that has a display-state mapping.

The current path is:

1. game logic sets `ent.display_state` through `TrySetDisplayState(...)`
2. `CommonPostStep(...)` calls `StepAnimTimer(...)`
3. `StepAnimTimer(...)` looks up `type_ + display_state`
4. if a mapping exists, it overwrites `ent.aframe_animator`
5. only then does the animator step frames

That means direct calls like:

```cpp
ent.aframe_animator.SetAnim(aframe_ids::LiveGrenade);
```

are not really authoritative if the ent also has a display-state mapping.
They can be stomped on the same frame or the next frame by
`StepAnimTimer(...)`.

This makes it harder to add:

- one-off action anims
- short transitional anims
- ent-specific anim control
- richer authored anim behavior that does not fit a shared enum

## Current Authoritative Site

The important code is in:

- [src/ents/common_frame.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ents/common_frame.cpp)

Today `StepAnimTimer(...)` does this every post-step:

- compute `GetAFrameSelectionForDisplayState(...)`
- if one exists, call `ent.aframe_animator.SetAnim(...)`
- set animate / forced frame flags from that selection
- step the animator

So yes: `StepAnimTimer(...)` currently knows about display state and treats
it as the anim authority.

## Current Scope

Right now the display-state mapping table in
[src/ent_display_states.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/ent_display_states.cpp)
only covers:

- `Player`
- `Bat`
- `BaseballBat`

So the display-state migration is not a whole-codebase anim rewrite yet.
It is mostly about cleaning up the ownership model for these few ents before
more anim complexity gets added.

## Goal

Make `AFrameId` / `AFrameAnimator` the real render truth.

Display states should become an optional helper layer for common semantic cases,
not the mandatory owner of anim choice.

Target model:

- rendering reads `ent.aframe_animator`
- ent logic can set exact anims directly
- common logic may still use display-state helpers for broad cases
- display state no longer silently overwrites explicit anim choice

## Desired Rules

### Rule 1

`aframe_animator.anim_id` is the actual anim being rendered.

### Rule 2

`EntDisplayState` is only a semantic helper for common cases like:

- player locomotion
- bat hanging vs flying
- generic stunned / dead visuals where that abstraction still helps

### Rule 3

If ent logic sets an exact anim directly, that choice should persist
until logic changes it again.

### Rule 4

Geometry lookup should continue to use current frame data, but the current frame
data should come from the animator only, not from display-state remapping every
frame.

## Migration Shape

Do this in stages.

### Stage 1: Stop Automatic Display-State Override

Change `StepAnimTimer(...)` so it no longer remaps
`display_state -> anim_id` every frame.

After this stage:

- `StepAnimTimer(...)` should only step `aframe_animator`
- explicit `SetAnim(...)` calls become stable

This is the main ownership change.

### Stage 2: Add Explicit Helper For Semantic Mapping

Add a helper with a narrow job, something like:

```cpp
bool TrySetAnimFromDisplayState(Ent& ent, EntDisplayState display_state);
```

or:

```cpp
bool TryApplyDisplayStateAnim(Ent& ent);
```

The important part is:

- semantic mapping happens only when code explicitly asks for it
- it does not happen as a hidden post-step side effect

### Stage 3: Keep `TrySetDisplayState(...)` As Validation Only

`TrySetDisplayState(...)` can still be useful, but its job should shrink to:

- validate that a display state is meaningful for that ent
- store the semantic state if desired

It should not imply that anim ownership has changed forever.

Possible shape:

```cpp
bool TrySetDisplayState(Ent& ent, EntDisplayState display_state);
bool TrySetAnimFromDisplayState(Ent& ent, EntDisplayState display_state);
```

Then call sites pick the right one.

### Stage 4: Convert Common Logic To Explicit Anim Calls

Move common semantic cases to explicit helpers where they still make sense.

Examples:

- player movement code can still use display-state mapping helper
- bat AI can still use display-state mapping helper
- bomb / rope / jetpack / tools can directly set raw anim ids

### Stage 4A: Remove Baseball Bat From Display-State Mapping

`BaseballBat` should not really use semantic display states at all.

Reasons:

- it only has one authored swing anim
- gameplay already reads animator frame index directly for swing timing
- the display-state layer adds no real value here

Planned cleanup:

1. remove `BaseballBat` from `ent_display_states.cpp`
2. stop calling `TrySetDisplayState(...)` in baseball bat logic
3. set `aframe_animator` directly with `SetAnim(...)` or spec init only

This is the clearest example of an ent that should be raw-anim-driven.

### Stage 5: Reassess Whether `display_state` Still Belongs On `Ent`

After the cutover, decide whether `display_state` should remain as:

- a semantic runtime field for some ents
- or only a convenience concept used at call sites

No need to force this immediately.

## Good First Cut

The first safe pass should be:

1. remove display-state remap from `StepAnimTimer(...)`
2. add explicit helper to map display state to anim
3. update the player and bat logic to call that helper directly
4. move baseball bat to direct `SetAnim(...)`
5. leave other raw `SetAnim(...)` paths alone

That gets rid of the hidden stomp without requiring a giant rewrite.

## Open Questions

### Forced Frames / Animate Flags

`GetAFrameSelectionForDisplayState(...)` currently also returns:

- `animate`
- `forced_frame`

That means the display-state helper should still be allowed to write:

- `anim_id`
- `animate`
- optional forced frame

That is fine. It just should not happen implicitly every post-step.

### Geometry Size Changes

`ApplyAFrameGeometryToEnt(...)` currently resizes `ent.size` from the
current frame's `pbox`.

That behavior can stay, but it should follow the animator's actual current frame.
It should not depend on a hidden display-state remap step.

### Debug UI

If `display_state` becomes only a semantic helper, debug UI should present it as:

- semantic state
- not necessarily the currently rendered anim

That distinction is healthier.

## Recommendation

Keep `EntDisplayState`, but demote it.

Do not make it the global anim graph.

Use:

- raw `AFrameId` for actual anim authority
- display-state mapping only as explicit helper logic for shared semantic cases

That gives more anim freedom without losing the convenience of common
state-driven sprite selection where it still helps.
