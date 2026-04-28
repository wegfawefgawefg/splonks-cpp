# Stage Rotation Notes

## Goal

World rotation is a two-phase transition:

- Visual phase: freeze simulation and render the whole gameplay world rotating around a pivot.
- Commit phase: apply an exact quarter-turn transform to stage data and entity positions, then resume simulation.

The first implementation is intentionally narrow:

- Full-stage only.
- Quarter turns only.
- Stage-center pivot only.
- Debug-triggered.
- 180-frame animation at 60 fps, roughly 3 seconds.
- Plays the `bigmachinerotate` one-shot when the rotation starts.
- Clears the exposed non-world corners to a dark void color during the spin.

## Why Quarter Turns

The stage is tile based. Arbitrary angles would need resampled tiles or temporary non-grid collision. Quarter turns keep every tile as a tile and let us remap data exactly.

## Pivot

The first pivot is the stage center. For future tile-targeted rotation, use tile centers rather than top-left tile corners. Tile centers avoid half-tile surprises when rotating entity centers and authored tile regions.

## Commit Data

A full-stage commit should transform:

- foreground tile grid
- foreground tile rotation grid
- backwall tile grid
- embedded treasure grid
- foreground/background tile shake grids
- room/debug metadata grids
- stage path tile coords
- stage tile trigger coords
- stage light tile coords
- background stamp positions
- stagegen annotation positions
- active entity centers
- fixed-position audio emitters
- particles and particle control points
- stage border sides and wrap axes
- lighting/acoustics/SID caches

Entity sizes are not rotated in the committed world. Entities visually rotate during the transition, then settle back into normal unrotated gameplay with transformed center positions.

Camera view state is not part of the committed world transform. In follow mode, the camera tracks the followed entity's visually rotated world position during the spin so the target stays centered.

Foreground tile instances carry a 2-bit quarter-turn rotation. Stage rotation remaps the rotation grid and increments each tile's stored orientation by the committed quarter-turn count.

The first rotation-aware tile semantics are intentionally limited:

- Ladder/rope/vine climbability is controlled by the tile archetype's climbable rotation mask.
- Ladder/rope/vine currently allow upright and upside-down rotations, but not sideways rotations.
- Spikes rotate their cbox and hazard direction. Up spikes hurt on downward contact, right spikes on rightward contact, down spikes on upward contact, and left spikes on leftward contact.
- Other tiles can render rotated after a committed stage rotation, but their gameplay behavior is unchanged.

## Future Work

- Square subsection rotation should be possible if the subsection is a square and the commit only remaps that square.
- Arbitrary-pivot full-stage rotation is possible on wrapped stages, but needs explicit clipping/expansion rules for non-wrapped stages.
- Entity velocity policy should be designed per use case. The first version zeroes velocity and acceleration on commit to prevent post-rotation physics surprises.
