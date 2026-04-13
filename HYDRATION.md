# Hydration

## Current State

Repo: `/home/vega/Coding/GameDev/Splonks`
Main game: `/home/vega/Coding/GameDev/Splonks/splonks-cpp`

Recent caveman work already landed:

- Added caveman frame ids in `splonks-cpp/src/frame_data_id.hpp`:
  - `Caveman`
  - `CavemanWalk`
  - `CavemanStunned`
  - `CavemanDead`
- Added display-state mapping in `splonks-cpp/src/entity/display_states.cpp`:
  - Neutral -> `caveman`
  - Walk -> `caveman_walk`
  - Stunned -> `caveman_stunned`
  - Dead -> `caveman_dead`
- Synced `caveman_dead` into `assets/graphics/annotations.yaml` from the span workspace entry.
- Turned `splonks-cpp/src/entities/caveman.cpp` from a pure archetype stub into a real minimal walker:
  - idle
  - patrol
  - turns at walls
  - turns at ledges
  - hurts on contact
  - uses shared ground-walker queries, so it respects wrapping
- Build passed with:
  - `cmake --build build -j4`

## Caveman Now Vs Classic-HD

Current Splonks caveman is still only a minimal patrol enemy.

Classic-HD reference inspected locally:

- `SpelunkyClassicHD/objects/oCaveman/Create_0.gml`
- `SpelunkyClassicHD/objects/oCaveman/Step_0.gml`
- `SpelunkyClassicHD/objects/oEnemySight/Collision_oCharacter.gml`

Important behavior slices from classic-HD:

1. Attack / aggro run
- Caveman has an `ATTACK` state.
- Walk speed is about `1.5`.
- Attack speed is about `3.0`.
- It enters attack via a forward sight probe created every 5 frames.

2. Higher HP and real wakeup lifecycle
- Caveman has `hp = 3` in classic-HD.
- Stunned caveman wakes back up if still alive after stun timer expires.
- Dead caveman uses a resting dead sprite only when body motion settles.
- While still moving, dead body behaves like a tumbling stunned body.

3. Idle wall-hop
- When grounded in idle and pushed near a wall, caveman does a little hop:
  - `yVel = -6`
  - small horizontal shove
- This makes them feel less like plain patrol walkers.

4. Stomp / contact semantics
- Head stomp stuns them and bounces player.
- Side/body contact hurts player.

5. More corpse visuals
- Classic has extra held / tumbling visuals we do not have art for yet.
- We should not invent extra state just for missing art.

## Recommended Next Implementation Order

Do these next, in this order:

1. Add forward sight scan and attack run.
- This is the biggest feel upgrade.
- Do not overbuild it.
- A simple forward ray / probe using current wrapped world query layer is enough.
- If player detected ahead, set `ai_state = Pursuing` or a renamed attack/aggro AI state and run faster.

2. Raise caveman HP to 3 and make stun wakeup more like classic.
- Keep using current condition system.
- Do not reintroduce old broad state machinery.
- Let stunned caveman wake back to normal after timer if alive.

3. Add the idle wall-hop.
- Only when grounded and in idle.
- Small vertical impulse and slight horizontal push.
- This should be local caveman logic, not a generic helper yet.

4. Improve corpse behavior.
- Moving dead caveman should look like a thrown / tumbling body if possible with existing assets.
- Once settled, dead sprite should return to `caveman_dead`.
- Do not add fake animations we do not have art for.

## Constraints / Style Notes

- Keep it C-like and explicit.
- Avoid over-abstracting caveman into a general enemy brain right now.
- Use existing wrapped query APIs so behavior works across toroidal seams.
- Keep files boring and local.
- `apply_patch` has been unreliable in this workspace, so shell writes were used.

## Useful File References

- `splonks-cpp/src/entities/caveman.cpp`
- `splonks-cpp/src/entities/caveman.hpp`
- `splonks-cpp/src/entities/snake.cpp`
- `splonks-cpp/src/entities/common/ground_walker.cpp`
- `splonks-cpp/src/world_query.cpp`
- `splonks-cpp/src/entity/display_states.cpp`
- `splonks-cpp/src/frame_data_id.hpp`
- `SpelunkyClassicHD/objects/oCaveman/Create_0.gml`
- `SpelunkyClassicHD/objects/oCaveman/Step_0.gml`
- `SpelunkyClassicHD/objects/oEnemySight/Collision_oCharacter.gml`

## Resume Prompt

A good resume prompt would be:

"Read `HYDRATION.md`, then implement the next caveman behavior slice: forward sight aggro + attack run, using the wrapped world query layer and keeping the implementation local to `entities/caveman.cpp`. Rebuild after changes."
