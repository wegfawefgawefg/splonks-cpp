# Water / Fluid Simulation Notes

## Current Implementation: Amount Grid Overlay

- Store fluid in `Stage::fluid_tiles`, a level-sized overlay grid parallel to `Stage::tiles`.
- Store per-cell fill in `Stage::fluid_amount`, currently `0..255`.
- Terrain stays in `Stage::tiles`, so a cell can be `terrain = Ladder` and `fluid = WaterSwim`.
- Mark fluid-capable tile archetypes with `simulated_fluid`.
- Every 4-8 gameplay frames, run one full pass over the stage tile grid. The initial default is 6 frames and is live-tunable through the debug fluid brush.
- Read terrain blockage from `stage.tiles`, read/write fluid type and amount through `stage.fluid_tiles` and `stage.fluid_amount`.
- Fluids move into air or transparent non-solid terrain cells, so water can occupy ladders/ropes without replacing them.
- Apply the changed fluid cells after the pass, then batch lighting and acoustics updates for those changed cells.
- Alternate left/right candidate order each fluid tick so water does not always prefer one side.
- Render water as a late transparent pass:
  - body cells use amount-driven textured quads/trapezoids;
  - falling cells use center-sliced vertical strips from the same water texture;
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
- Does not yet model pressure, waterfalls, source/sink pumps, liquid mixing, or sloped surface polygons.

Initial rules:

- The simulation is generic: it asks tile archetypes whether a tile is simulated fluid.
- `WaterTop` is drawn at render time from neighboring water and amount data.
- Entity control changes come from tile overlap effects, not from water-specific physics checks in the engine.
- Water-specific entities, like piranhas, may still use water query helpers in their own content logic.
- Fluids render as a late transparent pass over terrain/entities.

Transfer rule notes:

- Terraria's runtime update first tries to fill the cell below by its full remaining capacity, up to `255`, before doing horizontal equalization.
- Terraria's horizontal path averages liquid amounts over neighboring same-liquid cells and requeues changed cells.
- Splonks currently keeps the simpler full-stage pass, but its transfer rates should still use full `0..255` capacity while we validate behavior.

Source notes:

- `TerrariaDecompiled` is useful because it exposes decompiled vanilla Terraria source, including `Liquid.cs`. It is still decompiled code, not an official engine design document.
- tModLoader source/docs are a strong public reference for Terraria-compatible APIs and modern rendering internals, but not every renderer detail should be assumed to be byte-for-byte vanilla Terraria.
- For simulation parity, prefer the decompiled `Liquid.cs` flow rules. For rendering ideas, tModLoader `LiquidRenderer` / `LiquidEdgeRenderer` are useful design references.
- References:
  - `TerrariaDecompiled/Liquid.cs`: https://infinitynichto.github.io/TerrariaDecompiled/dc/dad/Liquid_8cs_source.html
  - tModLoader `LiquidRenderer`: https://docs.tmodloader.net/docs/stable/class_liquid_renderer.html
  - tModLoader `LiquidEdgeRenderer`: https://docs.tmodloader.net/docs/preview/class_liquid_edge_renderer.html

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

- Waterfall visuals should be separate render-only effects, not collision/drowning liquid.
- Sloped surfaces should be generated from neighboring amounts, not authored as fixed-angle sprites. The first pass uses the current cell amount plus left/right neighbor amounts to make a textured trapezoid body.
- A marching-squares pass could replace the current trapezoid heuristic later if we need cleaner corners between many partial cells.
- The current SDL renderer can draw basic transitions by slicing the water texture vertically and horizontally.
- For smoother partial-cell edges, render textured quads/trapezoids with `SDL_RenderGeometry` instead of requiring a new tile asset for each slope.
- Waterfalls start as thin side-sliced vertical strips from the existing `water` texture, with width driven by source amount.
- Terraria-style polish usually separates simulation from edge rendering. Splonks should keep this split: the grid stores amounts, while render code derives body, top, slope, and fall visuals from that grid.
