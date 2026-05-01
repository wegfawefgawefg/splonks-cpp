# Water / Fluid Simulation Notes

## Target Implementation: Vector / Density Fluid

The old byte-grid water was useful for proving the ownership split, but the
next implementation should be cleaner:

- Store fluid amount as a normalized float in `0..1`.
- Store fluid velocity as a `Vec2` per cell.
- Store terrain separately from fluid, as we do now:
  `Stage::tiles` is terrain and `Stage::fluid_tiles` / amount / velocity are
  the fluid overlay.
- Use a global gravity vector first. A per-cell gravity grid can be added later
  without changing the amount/velocity storage shape.
- Remove binary momentum and scan-order direction hacks.
- Simulate from a snapshot and apply transfers after all proposals are known.
- Render as fixed-alpha cells with explicit surface ribbons. The attempted
  density contour renderer was removed because the simpler cell renderer looked
  better for this game, was easier to tune, and was cheaper.

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
- Draw one full water cell with global water alpha, not alpha scaled by fill amount.
- Use the render cutoff as the single "water exists visually" threshold.
- Draw `watertop` ribbons on any exposed edge where this cell is above the
  render cutoff and the neighbor is not solid/fluid above that same cutoff.
- Draw optional debug flow as a single moving pixel. Stronger velocity moves
  the pixel faster rather than making the indicator longer.
- Draw occasional render-only bubbles from the `bubble` animation inside cells
  above the topper cutoff.

Debug knobs for the vector model:

- Simulation interval.
- Global fluid gravity X/Y.
- Pressure strength.
- Velocity damping.
- Transfer cap per step.
- Temporal smoothing and response.
- Render cutoff.
- Global water alpha.
- Fluid brush mode:
  - water paint / erase;
  - permanent per-cell gravity override paint / erase;
  - temporary per-cell gravity impulse paint / erase;
  - global gravity direction picker from stage center to mouse.
- Temporary gravity decay.

Per-cell gravity:

- `Stage::fluid_gravity` stores a permanent `Vec2` override per cell.
- `Stage::fluid_gravity_strength` marks whether the permanent override is active.
  This makes `(0, 0)` a valid local gravity value instead of meaning "unset".
- `Stage::fluid_temp_gravity` stores a decaying additive impulse per cell.
- The debug brush has a separate painted gravity vector for permanent/temp gravity, so those tools do not have to reuse the global simulation gravity direction.
- Effective gravity for simulation is:
  `active_local_gravity_or_global_gravity + temporary_gravity`.
- Temporary gravity decays even when the cell is dry, so old brush/bomb impulses do not wait forever for water to arrive.
- The renderer no longer depends on gravity direction. It draws alpha cells and
  edge ribbons from neighbor occupancy, which keeps it stable under arbitrary
  fluid gravity.

## Current Implementation: Vector Amount Overlay

- Store fluid in `Stage::fluid_tiles`, a level-sized overlay grid parallel to `Stage::tiles`.
- Store per-cell fill in `Stage::fluid_amount`, as a normalized float in `0..1`.
- Store per-cell velocity in `Stage::fluid_velocity`.
- Terrain stays in `Stage::tiles`, so a cell can be `terrain = Ladder` and `fluid = WaterSwim`.
- Mark fluid-capable tile archetypes with `simulated_fluid`.
- Every N gameplay frames, run one full pass over the stage tile grid. The current validation default is every frame and is live-tunable through the debug fluid brush.
- Read terrain blockage from `stage.tiles`, read/write fluid type, amount, and velocity through the fluid overlay grids.
- Fluids move into air or transparent non-solid terrain cells, so water can occupy ladders/ropes without replacing them.
- Build transfer proposals from a snapshot, scale them by source amount and target capacity, then apply the whole transfer set at once.
- Transfers are driven by velocity projected onto neighbor directions plus pressure from amount differences.
- Amounts are normalized floats. `1.0` is a full cell, so the sim no longer needs byte-liquid droplet special cases.
- Gravity is a tunable global `Vec2` for now. Pressure is blocked from flowing opposite gravity; with zero gravity, pressure can spread in any direction.
- Gravity magnitude matters. Pressure direction gating fades in with gravity strength, and the transfer budget scales by total proposal score so micro-gravity does not spend a full-cell transfer budget immediately.
- Apply changed fluid cells after the pass, then batch lighting and acoustics updates for those changed cells.
- Render water as a late transparent pass:
  - smooth `fluid_display_amount` as an optional render cache;
  - render one full water quad per visible occupied cell, with global water alpha;
  - draw `watertop` ribbons only on edges not adjacent to solid terrain or visible fluid;
  - `watertop` uses the animated `watertop` sprite, with tile-position tick offsets so every surface tile does not animate in lockstep;
  - draw occasional render-only `bubble` sprites inside visible water cells.
- The render cutoff slider controls whether a display-cache cell counts as visible water at all. Body rendering, topper rendering, and neighbor edge checks all use this same cutoff.
- The global water alpha slider controls water transparency. It is intentionally independent of fill amount, because fill amount should control presence/shape, not opacity.
  A hidden hard threshold made low-amount water vanish while flowing and should not be reintroduced without an explicit debug setting.
- The renderer still uses a tiny epsilon when deciding whether a smoothed display-cache cell is visible at all. This prevents temporal smoothing from leaving zero-amount phantom blue cells when the public render cutoff is `0`.
- Surface checks must use wrapped neighbor lookup. A top-row water cell should not draw `watertop` if Y wrap makes the bottom-row cell directly above it contain water.
- The stage-center gravity picker maps `(mouse_world - stage_center) / 64px` into global gravity, clamped to the debug gravity slider bounds. It is intentionally not normalized, so short drags can create weak gravity and long drags can create stronger gravity.
- Optional flow indicators draw render-only moving pixels from the fluid velocity grid. They are debug visualization, not gameplay particles.

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
- `WaterTop` is drawn at render time from alpha-cell exposed-edge data.
- Entity control changes come from tile overlap effects, not from water-specific physics checks in the engine.
- Water-specific entities, like piranhas, may still use water query helpers in their own content logic.
- Fluids render as a late transparent pass over terrain/entities.

Terraria byte-liquid reference notes:

- Terraria's runtime update first tries to fill the cell below by its full remaining capacity, up to `255`, before doing horizontal equalization.
- Terraria's horizontal path averages liquid amounts over neighboring same-liquid cells and requeues changed cells.
- Splonks currently keeps the simpler full-stage pass, but uses normalized `0..1` amounts instead of Terraria's byte `0..255` liquid amount.

Source notes:

- `TerrariaDecompiled` is useful because it exposes decompiled vanilla Terraria source, including `Liquid.cs`. It is still decompiled code, not an official engine design document.
- tModLoader source/docs are a strong public reference for Terraria-compatible APIs and modern rendering internals, but not every renderer detail should be assumed to be byte-for-byte vanilla Terraria.
- For simulation parity, prefer the decompiled `Liquid.cs` flow rules. For rendering ideas, tModLoader `LiquidRenderer` / `LiquidEdgeRenderer` are useful design references.
- Terraria's `LiquidRenderer.InternalPrepareDraw` builds a `LiquidCache`, derives `VisibleLiquidLevel`, extends visible liquid downward up to `WATERFALL_LENGTH`, computes smoothed edge walls, then draws source-rectangle slices with per-cell offsets.
- tModLoader documents `WATERFALL_LENGTH` as controlling how far visual liquidfalls draw. Liquidfalls are emitted from source liquid when the tile underneath is not solid; they are not a separate liquid amount in the target cell.
- Splonks intentionally keeps the current renderer simpler than Terraria's full `VisibleLeftWall` / `VisibleRightWall` / edge-frame cache because we do not currently have a Terraria-style liquid texture atlas. The important constraint is to avoid diagonal target inference: full columns must not emit side waterfalls just because a diagonal neighbor is open.
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
- We drew pool water as textured geometry quads/trapezoids, while Terraria draws source-rect slices with offsets.
- We currently draw waterfall-classified cells as a centered strip from the same `water` tile, not from a dedicated atlas waterfall region.
- We expose some raw sim oscillation because the sim can flip cells between tiny and near-full amounts; Terraria's visible cache and atlas slicing hide more of that.

This prototype was removed from the current renderer. Keep the notes here only
as a reference if we later want a Terraria-style liquid atlas renderer.

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

## Water And Lighting

Current water rendering does not recalculate lighting. Water can optionally use
render-time lighting: sample the existing backwall brightness for each water
cell and tint the water quad/topper/bubbles by that value. The debug fluid
brush exposes a toggle plus a strength slider, where `0` is unlit water and `1`
uses the sampled stage brightness directly.

Making water affect lighting is a separate step. The pragmatic version is to
treat changed fluid cells like changed terrain for lighting/acoustics batching,
then have the light solver read a fluid optical property such as attenuation or
color tint. That should only run for cells changed by the fluid pass, not every
render frame. A live full lighting recompute every fluid tick is the wrong shape
unless the stages stay tiny forever.
