# Live Stage Lighting Plan

## Goal

Replace fake terrain exposure/openness lighting with a live per-frame tile light grid. Openness stays useful for audio/acoustics, debug overlays, and possible ambient authoring, but it should not be the main source of terrain brightness.

## Terraria Reference Shape

Terraria-style lighting is a tile-coordinate visual system:

- Lights are added at tile/world positions each frame.
- The lighting engine computes colors on a tile grid around the view.
- Render code samples tile lighting, with sub-tile interpolation for smoother visuals.
- Light decays differently through air, solid, and water.
- Lighting is visual state, not gameplay-authoritative state.

Our stages are small enough that a full-stage pass every frame is acceptable until profiling proves otherwise.

## Splonks First Pass

Use `StageLighting` as the owner of live lighting data:

- Keep foreground topology for seam AO.
- Replace openness-derived brightness with live RGB/scalar light propagation.
- Rebuild at most once per `stage_frame`, even if multiple render passes call `EnsureStageLighting`.
- Keep existing menu toggles initially so the debug workflow does not churn.

The initial light seeds:

- Ambient air/backwall light so caves remain readable without torches.
- Dim solid/covered light so foreground tiles do not crush to black.
- Existing `StageLight` emitters, currently used by shop/store lights.

The initial propagation:

- Full-stage relaxation/flood-like propagation from seeded cells.
- Air/backwall cells lose a small amount per tile.
- Solid cells lose more per tile.
- Water/fluid cells can get their own decay/tint later.

## What This Replaces

The old foreground/backwall brightness caches were based on local tile openness. That made terrain faces readable, but it was a view-independent shading trick, not live lighting. The new system should make the same render paths sample a live grid instead.

The following should remain:

- Seam AO topology, because it is contact shadowing, not face shading.
- Stage acoustics openness cache.
- Debug openness rays/overlay.

## Later Work

- Add RGB color modulation instead of scalar brightness only.
- Add ent-attached light emitters.
- Add tile spec emissive light fields.
- Add fluid-specific absorption/tint.
- Add bilinear/sub-tile light sampling for ents and water.
- Add dirty-region/frontier updates only if full-stage per-frame lighting becomes expensive.
