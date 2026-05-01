# Lighting Enhancement Plan

## Current Problem

The live lighting grid works, but it still feels tile-discrete:

- Player light is seeded at a whole tile, so brightness can jump when crossing tile boundaries.
- Entities and embedded overlays sample one tile instead of interpolating nearby cells.
- Foreground tiles use one brightness value per tile, so lighting has hard cell edges.
- Embedded treasures should read as shiny, but should not behave like torches.
- The old openness lighting made caves readable, and we lost too much of that ambient shape.

## Near-Term CPU-Side Fixes

1. Sub-tile light source splatting
   - Convert world-position light sources into weighted contributions across the nearest four tiles.
   - This should apply to the player lamp and future entity-attached lights.
   - Goal: no visible snap when the player crosses a tile boundary.

2. Bilinear light sampling
   - Add a helper that samples `StageLighting` using world coordinates.
   - Entities, embedded treasure overlays, water, and particles can use this.
   - Goal: moving sprites receive smooth lighting instead of tile-stepped lighting.

3. Embedded treasure self-light
   - Remove embedded treasure from the propagated light-source grid.
   - Render visible embedded treasure with `max(sampled_light, treasure_min_brightness)`.
   - Optional later: rare sparkle particles can emit tiny temporary lights, but normal gold/gems should not illuminate nearby walls.

4. Temporal smoothing
   - Store the previous final light grid and blend toward the newly computed grid.
   - Add a debug slider for response speed.
   - Keep this after the spatial fixes, because smoothing can hide but not solve tile snapping.

5. Vertex/corner tile lighting
   - Sample brightness at tile corners or use neighboring cells to compute four vertex colors.
   - Draw foreground/backwall tiles with SDL geometry instead of a single texture color mod where needed.
   - Goal: tiles can be brighter on one edge and darker on another.

6. Openness ambient bias
   - Add a dim openness-derived ambient term back into the live light grid.
   - This should be additive and subtle, not the primary lighting model.
   - Suggested shape: `ambient = base_ambient + openness * openness_ambient_strength`.
   - This keeps caves readable and preserves some of the old terrain readability without making openness pretend to be an actual light.

## Renderer/Shader Question

We do not need Vulkan or custom shaders to get good lighting in the near term.

SDL renderer can support the next useful steps:

- Texture color modulation for whole-sprite brightness.
- Alpha/multiply overlays.
- `SDL_RenderGeometry` for vertex-lit quads.
- CPU-computed light grids.

Shaders become useful later if we want:

- Normal-mapped tiles and sprites.
- Colored dynamic lights with many overlapping sources.
- Smooth screen-space light falloff independent of tile cells.
- Soft shadows.
- Bloom/glow.
- Better water refraction or caustics.

The pragmatic path is:

1. Finish the CPU tile-light model.
2. Add bilinear sampling and vertex-lit tile quads.
3. Profile.
4. Only move to shader/GPU lighting once CPU-side visuals hit a real ceiling.

## Likely Implementation Order

1. Remove embedded treasure as a propagated source and make it self-lit.
2. Add world-position bilinear sampling for entities/overlays.
3. Add sub-tile source splatting for the player lamp.
4. Add openness ambient bias with a debug strength slider.
5. Add temporal smoothing if flicker remains.
6. Add vertex/corner lighting for terrain if cell edges are still too visible.
