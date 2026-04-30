# Water / Fluid Simulation Notes

## Approach A: Binary Tile Cellular Automata

This is the first implementation target.

- Store fluid as regular foreground tiles, not as special entity state.
- Mark fluid-capable tile archetypes with `simulated_fluid`.
- Every 4-8 gameplay frames, run one full pass over the stage tile grid. The initial default is 6 frames and is live-tunable through the debug fluid brush.
- Read from the current `stage.tiles` grid and write movement into a copied `next_tiles` grid.
- Fluids move only into air for now: down, then diagonal-down, then sideways.
- Apply the changed cells after the pass with normal stage tile mutation, then batch lighting and acoustics updates for those changed cells.
- Alternate left/right candidate order each fluid tick so water does not always prefer one side.

Pros:

- Very simple and easy to debug.
- No support-trigger ownership problem.
- No row/zone metadata to maintain when the level changes.
- Works with arbitrary tile destruction and stage edits.
- Keeps water behavior visible as normal tile data.

Cons:

- Binary full-tile water is chunky and not volume-conserving beyond one full tile per cell.
- A full-grid copy/pass is fine for current stage sizes, but may need an active-cell frontier later.
- Does not model partial liquid levels, pressure, waterfalls, or settling gradients.

Initial rules:

- The simulation is generic: it asks tile archetypes whether a tile is simulated fluid.
- The current visual `WaterTop` can remain derived at render time from neighboring water.
- Entity control changes come from tile overlap effects, not from water-specific physics checks in the engine.
- Water-specific entities, like piranhas, may still use water query helpers in their own content logic.

## Approach B: Amount Grid / Terraria-Style Liquid

This is the richer later option.

- Add a separate fluid grid per stage with a fluid type and amount field per tile, for example `type + uint8 amount`.
- Tiles remain terrain; fluid is an overlay/layer that renders according to amount and neighbors.
- Sim passes move portions of amount between neighboring cells using pressure/flow rules.
- Rendering can draw partial fill heights, surface waves, and different liquid types.
- Terrain breakage can wake nearby fluid cells or run a bounded active-set pass.

Pros:

- Supports partial water, filling, draining, pooling, and multiple liquid types much better.
- Avoids replacing normal terrain tiles with fluid tiles.
- Can handle more convincing future lava/acid/mud behavior.

Cons:

- More moving parts: storage, renderer, entity overlap, save/load, stagegen authoring, and debug UI.
- More ways to violate ownership if fluid behavior starts special-casing content.
- Harder to tune and validate than binary tile water.

## Current Decision

Start with Approach A. It is enough to validate piranhas, pools, drainage, and water effects without committing to a full liquid layer. If binary water becomes too limited, move to Approach B after the gameplay requirements are clearer.
