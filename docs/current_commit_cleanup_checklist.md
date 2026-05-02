# Current Commit Cleanup Checklist

This tracks ownership and code-shape issues found during the current large gameplay/rendering pass.

- [x] Move global fluid simulation/render settings out of debug brush state.
  - The fluid brush should own only brush UI state: enabled, mode, radius, paint gravity, replace-solid behavior, and debug flow overlay.
  - Simulation/render constants belong to normal settings or stage/system state because gameplay and rendering use them even when the brush is not conceptually the owner.

- [x] Remove the player-lamp special case from lighting construction/debug rendering.
  - Lighting should consume live entity light fields.
  - Gameplay/controller code can decide which controlled playable entity emits the player lamp.

- [x] Move entrance entity spawning out of generic stage init.
  - Generic stage initialization should spawn authored entity spawns.
  - Stagegen/template code should decide that an entrance tile marker also creates an entrance entity.

- [x] Move treasure pickup sparkle/glint/light VFX out of common collect code.
  - Shared collection plumbing should not know treasure presentation details.
  - Put treasure-specific pickup effects under content/effects or treasure entity helpers.

- [x] Split oversized render/debug files.
  - `render/tiles_and_entities.cpp` currently mixes tiles, entities, fluids, lighting helpers, and HUD-adjacent rendering.
  - `render/debug.cpp` currently mixes many unrelated debug overlays.
  - Particle rendering now lives in `render/particles.*`; light debug overlay rendering now lives in `render/debug_lighting.*`.

- [x] Move `ParticleLightingMode` out of `particle_archetypes.hpp`.
  - `sprite_particle.hpp` should not include archetype metadata just to get a shared enum.
  - Put shared particle rendering/runtime enums in a small neutral header.
