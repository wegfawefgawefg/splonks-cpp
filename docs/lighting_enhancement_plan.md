# Lighting Enhancement Plan

## Current State

The live lighting grid is the active terrain/entity lighting path. It supports:

- CPU-computed RGB brightness grid.
- World-position light splatting for stage lights, player lamp, and emitting entities.
- Transient world lights for short-lived flashes.
- Bilinear world-position sampling for moving sprites and overlays.
- Foreground and backwall vertex/corner lighting.
- Water/liquid light attenuation.
- Openness ambient bias.
- Temporal smoothing.
- Embedded treasure self-brightness without making every treasure a propagated torch.
- Particle lighting modes: scene-lit, unlit, and emissive.
- RGB source colors for stage lights, entity lights, transient lights, tiles, entities, particles, water, and overlays.

## Completed

- [x] Remove face-shading terrain trick.
- [x] Replace openness-only lighting with a live propagated light grid.
- [x] Merge player lamp into the normal live light source path.
- [x] Add live light source support for entity archetypes and runtime entities.
- [x] Add transient lights for explosion and gunshot flashes.
- [x] Add self-light for entities that should read bright without necessarily lighting the world.
- [x] Add bilinear lighting samples for entity and overlay rendering.
- [x] Add vertex lighting for foreground tiles.
- [x] Add vertex lighting for backwall tiles.
- [x] Add border lighting samples.
- [x] Add liquid attenuation to light propagation.
- [x] Add openness ambient bias.
- [x] Add temporal light smoothing.
- [x] Add embedded treasure brightness control.
- [x] Add particle lighting modes.
- [x] Add RGB lighting.
  - Scalar ambient/openness settings still enter the grid as white light.
  - Light sources now carry separate strength and color.
  - Terrain, backwall, entity, particle, water, tile cap, and embedded overlay render paths sample RGB.
- [x] Add debug visibility for live light sources.
  - The existing light overlay now shows stage lights, player lamp, entity-emitted lights, and transient flashes.
- [x] Add vertex lighting for tile caps.
  - Caps now sample their exact rendered rect corners instead of using one owning-tile brightness value.

## Remaining

- [ ] Expand transient light coverage.
  - Current coverage: explosion flash and gunshot flash.
  - Useful next cases: sparks, magic effects, lava bursts, and future electrical effects.

- [ ] Profile many live lights.
  - Current entity lights are fine at normal counts.
  - If we start lighting every coin/gem/particle, add culling, caps, or light buckets before it becomes a frame-time issue.

## RGB Implementation

Implemented shape:

1. `LiveLightSource` stores `{strength, color}`.
2. `StageLighting` caches use `Color3` grids.
3. Propagation runs per channel with the same decay rules.
4. Scalar ambient/openness stays white light.
5. Terrain and backwall vertex lighting writes per-vertex RGB.
6. Entity, particle, water, tile cap, and overlay lighting use RGB samples.
7. Old scalar sample helpers remain as brightness wrappers for compatibility.

This lets gems, explosions, teleporter phase effects, lava, water caustics, and weird quest-specific lights tint the world without special render cases.

## Transient Light Plan

Do not make every flash a real entity. Add a small frame-owned or state-owned list like:

```cpp
struct TransientLight {
    Vec2 pos;
    float strength;
    int radius_tiles;
    int frames_remaining;
    int total_frames;
};
```

Then collect it into the same live light source list before lighting recomputes.

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

1. Profile dense light scenes before adding many more always-on entity emitters.
2. Expand transient light coverage and add colored source data to new light-emitting content as it is implemented.
3. Consider shaders only after RGB CPU lighting or normal mapping creates a real renderer ceiling.
