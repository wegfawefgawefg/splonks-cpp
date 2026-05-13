# Current Commit Cleanup Checklist

This tracks ownership and code-shape issues found during the current large gameplay/rendering pass.

- [x] Move global fluid simulation/render settings out of debug brush state.
  - The fluid brush should own only brush UI state: enabled, mode, radius, paint gravity, replace-solid behavior, and debug flow overlay.
  - Simulation/render constants belong to normal settings or stage/system state because gameplay and rendering use them even when the brush is not conceptually the owner.

- [x] Remove the player-lamp special case from lighting construction/debug rendering.
  - Lighting should consume live ent light fields.
  - Gameplay/controller code can decide which controlled playable ent emits the player lamp.

- [x] Move entrance ent spawning out of generic stage init.
  - Generic stage initialization should spawn authored ent spawns.
  - Stagegen/template code should decide that an entrance tile marker also creates an entrance ent.

- [x] Move treasure pickup sparkle/glint/light VFX out of common collect code.
  - Shared collection plumbing should not know treasure pres details.
  - Put treasure-specific pickup effects under content/effects or treasure ent helpers.

- [x] Split oversized render/debug files.
  - `render/tiles_and_ents.cpp` currently mixes tiles, ents, fluids, lighting helpers, and HUD-adjacent rendering.
  - `render/debug.cpp` currently mixes many unrelated debug overlays.
  - Particle rendering now lives in `render/particles.*`; light debug overlay rendering now lives in `render/debug_lighting.*`.

- [x] Move `ParticleLightingMode` out of `particle_specs.hpp`.
  - `sprite_particle.hpp` should not include spec metadata just to get a shared enum.
  - Put shared particle rendering/runtime enums in a small neutral header.
