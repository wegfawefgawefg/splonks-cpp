# Water / Fluid Simulation Notes

## Target Implementation: Vector / Density Fluid

The old byte-grid water was useful for proving the ownership split, but the
next implementation should be cleaner:

- Store fluid amount as a floating value in `0..255`.
- Store fluid velocity as a `Vec2` per cell.
- Store terrain separately from fluid, as we do now:
  `Stage::tiles` is terrain and `Stage::fluid_tiles` / amount / velocity are
  the fluid overlay.
- Use a global gravity vector first. A per-cell gravity grid can be added later
  without changing the amount/velocity storage shape.
- Remove binary momentum and scan-order direction hacks.
- Simulate from a snapshot and apply transfers after all proposals are known.
- Render from a scalar density field with marching squares, not from a
  gravity-specific "filled from bottom" assumption.

Simulation shape:

1. Normalize authored fluid tiles into the overlay grid.
2. Copy source amount, velocity, and fluid type.
3. For each fluid cell, update velocity with gravity and damping.
4. Propose transfers to the 8 neighboring cells from:
   - velocity projected onto the neighbor direction;
   - pressure from amount difference;
   - destination capacity.
5. Scale proposals so no source gives away more liquid than it has.
6. Scale proposals again so no target exceeds its capacity.
7. Apply all scaled proposals at once.
8. Accumulate target velocity from incoming flow direction and damp retained
   source velocity.

This makes the solver mostly order-independent. It also means no-gravity fluid
is natural: set gravity to `(0, 0)` and the fluid only moves from retained
velocity and pressure.

Rendering shape:

- Smooth `fluid_display_amount` remains a render cache only.
- Build a density value at grid vertices by sampling the four neighboring
  fluid cells.
- Run marching squares against that scalar density threshold.
- Draw water polygons from the density contour.
- Draw `watertop` ribbons on contour segments that are mostly horizontal for
  now. Later, if we support arbitrary local gravity fields visually, orient
  surface foam/ripples from the local gravity vector or contour normal.

Debug knobs for the vector model:

- Simulation interval.
- Global fluid gravity X/Y.
- Pressure strength.
- Velocity damping.
- Transfer cap per step.
- Temporal smoothing and response.
- Render cutoff / density threshold.

Per-cell gravity plan:

- Add an optional `Stage::fluid_gravity` grid with `Vec2` cells.
- Use global gravity when the cell override is zero/unset.
- Add a debug gravity brush after the base vector solver feels good.
- The renderer should not assume down is gravity; it should keep using scalar
  density and only use gravity for optional surface decoration.

## Current Implementation: Vector Amount Overlay

- Store fluid in `Stage::fluid_tiles`, a level-sized overlay grid parallel to `Stage::tiles`.
- Store per-cell fill in `Stage::fluid_amount`, as a float in `0..255`.
- Store per-cell velocity in `Stage::fluid_velocity`.
- Terrain stays in `Stage::tiles`, so a cell can be `terrain = Ladder` and `fluid = WaterSwim`.
- Mark fluid-capable tile archetypes with `simulated_fluid`.
- Every N gameplay frames, run one full pass over the stage tile grid. The current validation default is every frame and is live-tunable through the debug fluid brush.
- Read terrain blockage from `stage.tiles`, read/write fluid type, amount, and velocity through the fluid overlay grids.
- Fluids move into air or transparent non-solid terrain cells, so water can occupy ladders/ropes without replacing them.
- Build transfer proposals from a snapshot, scale them by source amount and target capacity, then apply the whole transfer set at once.
- Transfers are driven by velocity projected onto neighbor directions plus pressure from amount differences.
- Gravity is a tunable global `Vec2` for now. Pressure is blocked from flowing opposite gravity; with zero gravity, pressure can spread in any direction.
- Apply changed fluid cells after the pass, then batch lighting and acoustics updates for those changed cells.
- Render water as a late transparent pass:
  - smooth `fluid_display_amount` as an optional render cache;
  - build a scalar density field from nearby display amounts;
  - run marching squares against that density field;
  - render textured water polygons and `watertop` ribbons on mostly-horizontal contours;
  - `watertop` uses the animated `watertop` sprite, with tile-position tick offsets so every surface tile does not animate in lockstep.
- Surface checks must use wrapped neighbor lookup. A top-row water cell should not draw `watertop` if Y wrap makes the bottom-row cell directly above it contain water.

Pros:

- Very simple and easy to debug.
- No support-trigger ownership problem.
- No row/zone metadata to maintain when the level changes.
- Works with arbitrary tile destruction and stage edits.
- Keeps terrain and fluid ownership separate.

Cons:

- Still uses a full-stage copy/pass. This is fine for current stage sizes, but a large Terraria-scale world needs active-cell queues.
- Pressure is intentionally simple and local.
- Does not yet model source/sink pumps, liquid mixing, or local per-cell gravity.

Initial rules:

- The simulation is generic: it asks tile archetypes whether a tile is simulated fluid.
- `WaterTop` is drawn at render time from marching-squares contour data.
- Entity control changes come from tile overlap effects, not from water-specific physics checks in the engine.
- Water-specific entities, like piranhas, may still use water query helpers in their own content logic.
- Fluids render as a late transparent pass over terrain/entities.

Legacy transfer rule notes:

- Terraria's runtime update first tries to fill the cell below by its full remaining capacity, up to `255`, before doing horizontal equalization.
- Terraria's horizontal path averages liquid amounts over neighboring same-liquid cells and requeues changed cells.
- Splonks currently keeps the simpler full-stage pass, but its transfer rates should still use full `0..255` capacity while we validate behavior.

Source notes:

- `TerrariaDecompiled` is useful because it exposes decompiled vanilla Terraria source, including `Liquid.cs`. It is still decompiled code, not an official engine design document.
- tModLoader source/docs are a strong public reference for Terraria-compatible APIs and modern rendering internals, but not every renderer detail should be assumed to be byte-for-byte vanilla Terraria.
- For simulation parity, prefer the decompiled `Liquid.cs` flow rules. For rendering ideas, tModLoader `LiquidRenderer` / `LiquidEdgeRenderer` are useful design references.
- Terraria's `LiquidRenderer.InternalPrepareDraw` builds a `LiquidCache`, derives `VisibleLiquidLevel`, extends visible liquid downward up to `WATERFALL_LENGTH`, computes smoothed edge walls, then draws source-rectangle slices with per-cell offsets.
- tModLoader documents `WATERFALL_LENGTH` as controlling how far visual liquidfalls draw. Liquidfalls are emitted from source liquid when the tile underneath is not solid; they are not a separate liquid amount in the target cell.
- Splonks intentionally keeps the current renderer simpler than Terraria's full `VisibleLeftWall` / `VisibleRightWall` / edge-frame cache because we do not currently have a Terraria-style liquid texture atlas. The important constraint is to avoid diagonal target inference: full 255 columns must not emit side waterfalls just because a diagonal neighbor is open.
- References:
  - `TerrariaDecompiled/Liquid.cs`: https://infinitynichto.github.io/TerrariaDecompiled/dc/dad/Liquid_8cs_source.html
  - `TerrariaDecompiled/LiquidRenderer.cs`: https://infinitynichto.github.io/TerrariaDecompiled/d2/d2c/LiquidRenderer_8cs_source.html
  - tModLoader `LiquidRenderer.cs.patch`: https://raw.githubusercontent.com/tModLoader/tModLoader/stable/patches/tModLoader/Terraria/GameContent/Liquid/LiquidRenderer.cs.patch
  - tModLoader `LiquidRenderer`: https://docs.tmodloader.net/docs/stable/class_liquid_renderer.html
  - tModLoader `LiquidEdgeRenderer`: https://docs.tmodloader.net/docs/preview/class_liquid_edge_renderer.html

## Terraria 1.4.4.9 Renderer Flow

Terraria does not draw directly from raw liquid amounts. It builds a draw cache first.

1. Build `LiquidCache` from tile liquid state.
   Each cache cell stores liquid level, visible liquid level, opacity, solidity, half-brick state, real-liquid presence, wall presence, liquid type, frame offset, and edge-wall values.

2. Derive `VisibleLiquidLevel`.
   Real liquid cells copy their actual `0..255` amount as a normalized level. Some empty cells can receive a derived visible level from neighboring liquid so gaps and half-block cases look continuous.

3. Extend visible liquid downward for liquidfalls.
   If a visible liquid cell has no solid tile below, visible liquid propagates downward up to `WATERFALL_LENGTH` cells. For water this is `10`. This is render-only; those lower cells do not necessarily contain real liquid.

4. Compute edge walls.
   Terraria computes `LeftWall`, `RightWall`, `TopWall`, and `BottomWall` from neighboring visible-liquid cells and solid cells. These values describe which part of the tile should actually be drawn.

5. Smooth edge walls and pick frame offsets.
   Neighboring edge-wall values are averaged and adjusted to avoid blocky seams. `FrameOffset` selects a region from Terraria's liquid texture atlas based on which edges are visible.

6. Build and draw `LiquidDrawCache`.
   Each visible cache cell becomes a source rectangle plus a pixel offset. Rendering draws that source-rect slice from the liquid atlas at tile position plus `LiquidOffset`, using per-corner lighting and opacity.

## Previous Terraria-Style Renderer Prototype

That prototype matched:

- We have a real fluid overlay grid: `Stage::fluid_tiles` plus `Stage::fluid_amount`.
- We normalize authored water tiles into the overlay grid.
- We build a per-frame visible-liquid render cache before drawing.
- We extend visible liquid downward up to `10` cells for liquidfall visuals.
- We keep waterfalls render-only; they do not create collision/drowning liquid by themselves.
- We render water after terrain as a transparent pass.

That prototype still deviated:

- We do not store or compute Terraria's full edge-wall set: `LeftWall`, `RightWall`, `TopWall`, `BottomWall`, and their smoothed visible variants.
- We do not compute Terraria-style `FrameOffset`.
- We do not have a Terraria-style liquid texture atlas with edge/body/waterfall source regions.
- We draw pool water as textured geometry quads/trapezoids, while Terraria draws source-rect slices with offsets.
- We currently draw waterfall-classified cells as a centered strip from the same `water` tile, not from a dedicated atlas waterfall region.
- We expose some raw sim oscillation because the sim can flip cells between tiny and near-full amounts; Terraria's visible cache and atlas slicing hide more of that.

Needed assets if we want closer Terraria parity:

- A liquid atlas or equivalent frame set with at least body/interior, surface, side-edge, corner/diagonal transition, and vertical waterfall regions.
- A waterfall strip region that can tile vertically without looking like bottom-filled pool water.
- Optional edge variants matching Terraria's frame-offset cases if we want to port the edge-wall renderer directly.
- Our existing animated `watertop` can remain as a stylistic overlay, but a direct Terraria-like renderer would use atlas source slices for most surface/edge work instead.

Temporary validation defaults:

- Boot into `classic_mines_1`.
- Enable Y wrapping with one tile of wrap padding and disable camera clamp.
- Use `StageFit` camera.
- Show and enable the fluid brush by default.
- Default fluid brush settings: radius `0`, simulation interval `1`, replace non-fluid terrain while painting.
- These defaults are temporary and should be removed after the water pass is validated.

## Previous Option: Binary Tile Cellular Automata

This was the initial implementation.

- One cell was either empty or full.
- Movement copied whole water tiles.
- It was very simple and good for proving terrain/fluid ownership, but whole 16x16 cubes visibly moved.

## Terraria-Scale Optimization Notes

Terraria-style worlds avoid scanning the entire world every frame.

- tModLoader/Terraria exposes `Tile::LiquidAmount` as a byte-like `0..255` value per tile, with `255` meaning full.
- Terraria's liquid system exposes `Liquid::AddWater(x, y)`, `Liquid::UpdateLiquid()`, and bounded liquid work arrays such as `Liquid::maxLiquid` and `Liquid::maxLiquidBuffer`.
- Terraria also has a `LiquidBuffer::AddBuffer(x, y)` path, which is the same broad class of design as an active liquid frontier: cells become scheduled when liquid or terrain changes instead of every world cell being simulated every frame.
- It also has quick-settle / panic-style paths for large startup or recovery cases.

For Splonks, the current full-stage pass is intentionally simpler while we validate the amount-transfer rules and visuals. If stages become huge, switch `StepStageFluids` to maintain an active frontier without changing the renderer or public stage storage.

Practical active-frontier shape:

- Store fluid amount as byte-like state per tile.
- Keep active liquid cells in queues/buffers.
- Wake nearby cells when terrain or liquid changes.
- Run bounded work per frame.
- Drop cells from the queue once they settle.

## Future Rendering

- Waterfall visuals should stay separate render-only effects, not collision/drowning liquid.
- Source cells should own waterfall rendering. Do not infer a waterfall solely from an empty target cell having liquid above, or the visual can drift one tile and create 0-amount waterfall artifacts.
- If we need smoother partial-cell edges later, port a small version of Terraria's visible-edge cache:
  - derive visible liquid level, including waterfall extension;
  - derive left/right/top/bottom walls from neighboring visible cells;
  - smooth those walls with neighboring edge values;
  - draw source-rectangle slices offset into each tile, instead of inventing per-case diagonal waterfall caps.
- Terraria-style polish separates simulation from edge rendering. Splonks should keep this split: the grid stores amounts, while render code derives body, top, and fall visuals from that grid.
