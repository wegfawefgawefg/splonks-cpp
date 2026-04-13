# Toroidal Wrap Plan

## Goal

Replace the current fake delayed-wrap behavior with a real seamless toroidal world mode,
while keeping the existing hard-border mode.

This means we want two valid stage boundary styles:

- hard bordered stage
- wrapped stage

Hard bordered stage remains useful because explicit left/right/top/bottom border tiles are good
for authored arenas, challenge rooms, and normal map behavior.

Wrapped stage should become a true torus:

- tiles wrap
- collision wraps
- rope / climbing / hanging can traverse seams
- rendering can see across seams
- camera follows normally without clamping

The current "leave the stage into air, then get normalized back later" behavior is not the real target.
That mode is only one special case of a larger toroidal-stage idea.

## World Model

The wrapped world should still just be a normal stage.

Do not invent separate "outside space", "wrap bounds", or ghost stage storage.

The model is:

- a stage has one tile grid
- a stage has one entity list
- a stage may wrap on X and/or Y

That is all.

If a wrapped stage wants one chunk of empty space around the interesting center area,
that is represented by making the stage itself larger and filling the apron with air.

So:

- a `4x4` chunk cave map with one chunk of padding becomes a real `6x6` chunk stage
- the old `4x4` lives in the middle
- the apron is just normal stage space
- the wrapped seam is the edge of the real larger stage

This is strictly more powerful than the current fake air-wrap mode, because the apron can later hold:

- air
- walls
- mirrored copies
- authored setpieces
- anything else

## Boundary Styles

### 1. Hard border mode

This is what the current border system is for.

Properties:

- explicit left/right/top/bottom border tiles
- no seam wrapping on those axes
- camera clamp may be enabled
- `void_death_y` may optionally kill below some threshold

Use cases:

- normal caves
- test rooms
- authored challenge maps
- one-way or asymmetric bounds behavior

### 2. Toroidal wrap mode

This is the new target.

Properties:

- stage wraps seamlessly on chosen axes
- border tiles are not the mechanism for those wrapped axes
- camera clamp is disabled by default
- apron / padding is represented as real stage area

If `wrap_x` is enabled:

- queries that move past the left edge sample the right edge
- queries that move past the right edge sample the left edge

If `wrap_y` is enabled:

- queries that move past the top edge sample the bottom edge
- queries that move past the bottom edge sample the top edge

This should be true for:

- tile rendering
- tile collision
- tile contact callbacks
- rope placement and climbing
- hanging checks
- hitscan
- entity broadphase queries near seams

## Chunk Padding

Wrapped stages should support an integer chunk padding amount.

Example:

- original stage: `4x4` chunks
- padding: `1`
- wrapped stage becomes `6x6` chunks

The padding amount applies on wrapped axes.

If only `wrap_x` is enabled:

- add padding on left and right only

If only `wrap_y` is enabled:

- add padding on top and bottom only

If both are enabled:

- add padding on all four sides

The padding area is initially just normal stage content.
For the first implementation it is fine if the padded tiles are initialized as:

- `Tile::Air`
- empty embedded treasure
- no entities
- no background stamps

Later we can support copying or authoring into the apron.

## Enabling Wrap

Turning wrap on should perform a real stage transform.

Given a stage and a padding amount:

1. Compute the new tile dimensions.
2. Allocate a new larger tile grid.
3. Copy the old tile grid into the centered core area.
4. Copy embedded treasure into the same shifted positions.
5. Shift entity positions by the world-space offset.
6. Shift background stamp positions by the same offset.
7. Shift any room/path metadata that is tile/chunk indexed.
8. Enable the requested wrapped axes.
9. Disable camera clamp.

The key point is:

- we are not creating a fake wrapped view
- we are literally transforming the stage into a larger wrapped stage

## Disabling Wrap

Turning wrap off should do the inverse transform.

Given the current wrapped stage and the remembered core area:

1. Crop tiles back down to the core region.
2. Crop embedded treasure to the core region.
3. Shift entity positions back by the inverse world-space offset.
4. Shift background stamps back as well.
5. Shift room/path metadata back.
6. Disable wrap on the chosen axes.
7. Restore camera clamp settings.

Important unresolved policy:

- what do we do with entities that are currently outside the core crop area?

This must be explicit.

Reasonable options:

- wrap them back into the crop before disabling wrap
- clamp them into the crop
- delete entities outside the crop

The best default is probably:

- wrap positions back into the crop on wrapped axes before the crop is applied

That preserves gameplay state without surprising deletions.

## Camera Behavior

Camera clamp should be a camera setting, not an implicit consequence of wrap.

That said, the intended default is:

- hard-border mode: clamp on
- wrapped mode: clamp off

Wrapped gameplay feels wrong if the camera is pinned against the edge of the larger toroidal stage.

So:

- wrap toggling should update the default camera clamp behavior
- but the user should still be able to toggle clamp independently for debugging or experimentation

## Border Tiles In Wrapped Mode

Border tiles still matter for hard-border mode.

For wrapped axes, they are not the primary mechanism anymore.

That means:

- if `wrap_x` is on, left/right border tile choice should not control collision on X
- if `wrap_y` is on, top/bottom border tile choice should not control collision on Y

We may still keep the stored border tiles around for:

- later turning wrap back off
- debug editing
- non-wrapped axes in mixed modes

Example:

- wrap X on, wrap Y off
- left/right borders do not block because X is toroidal
- top/bottom borders still use the normal border tiles

## What Must Change

### Tile sampling

All tile queries need a wrapped variant when the stage wraps on that axis.

This includes:

- single-tile world lookups
- rect tile queries
- terrain lighting neighborhood sampling
- render wrapper sampling
- hitscan tile stepping

The main rule:

- wrapped stages should sample canonical tiles through modulo on wrapped axes

### Entity queries

Entity interaction near seams cannot rely only on the current unwrapped AABB broadphase.

We need seam-aware querying for:

- pushing
- crushing
- contact damage
- pickup checks
- rope interactions
- hitscan entity scans

Two reasonable approaches:

- duplicate query regions near seams
- duplicate entity presentation in the SID near seams

The simpler first pass is:

- keep canonical entity positions
- for seam-touching queries, issue mirrored query rectangles on the wrapped axis

### Rendering

Rendering must be able to show opposite-edge content at the seam.

For tiles this is straightforward:

- wrapped tile sampling during render

For entities this is the same problem as broadphase:

- entities near a seam may need one or more mirrored render passes

Example:

- entity near left seam on wrapped X
- also render it shifted by `+stage_width`

Likewise for the right seam:

- render it shifted by `-stage_width`

And similarly for Y if wrapped vertically.

Corners will need diagonal duplicates when both axes wrap.

### Rope and seam traversal

This is one of the main reasons to do this.

A rope thrown at the top edge on wrapped Y should:

- place rope segments across the seam
- be climbable continuously
- allow exiting through top and entering from bottom

So rope code should not special-case stage top/bottom as absolute bounds when that axis wraps.

## Metadata To Track

To make wrap on/off reversible, we should store a small amount of transform metadata.

Suggested stage metadata:

- original pre-wrap chunk dimensions or tile dimensions
- current padding amount per axis
- current core origin inside the larger stage

This is not a second stage.
It is just enough information to reverse the resize/crop transform later.

## Debug / UI Goals

The border window should eventually support:

- hard border tile editing
- wrap axis toggles
- chunk padding amount
- camera clamp toggle
- `void_death_y`

For wrapped stages, useful debug overlays are:

- core region rectangle
- stage seam guides
- optional duplicate render visualization near seams

The current `void_death_y` guide is already in the right direction.

## Implementation Sequence

### Phase 1

Keep hard-border mode working.

- do not regress side tile behavior
- do not regress `void_death_y`

### Phase 2

Add the wrap transform.

- expand stage with padding
- copy tiles / treasure / entities / stamps / room metadata into centered core
- disable camera clamp by default

No fake delayed wrap after this point.

### Phase 3

Make tile sampling seamless.

- wrapped tile lookups
- wrapped rect sampling
- wrapped render sampling

This gets terrain and seam visibility working.

### Phase 4

Make entity interactions seamless.

- seam-aware entity broadphase
- seam-aware contact logic
- seam-aware pushing / crushing / pickups / hitscan

### Phase 5

Make rope / climb / hang fully seam-safe.

This is the main gameplay validation pass.

### Phase 6

Implement wrap-off inverse transform cleanly.

- crop back to core
- move everything back
- settle edge cases for entities outside crop

## Non-Goals For First Pass

Do not overcomplicate this with:

- infinite procedural wrapped space
- separate wrap-space vs core-space simulation layers
- authored apron editing tools
- perfect seam lighting polish

The first useful target is:

- real toroidal stage behavior
- optional chunk padding
- rope and player traversal across seams
- hard-border mode still intact

## Summary

The real target is not "air outside the map and snap later."

The real target is:

- a normal stage
- optionally larger than the central authored gameplay area
- optionally wrapped on X and/or Y

Hard border mode remains valid and valuable.

Wrapped mode becomes a real torus.

Chunk padding just means:

- make the real stage larger
- place the interesting area in the middle
- let the apron be normal stage space

That gives us one clean world model instead of a fake delayed-wrap special case.
