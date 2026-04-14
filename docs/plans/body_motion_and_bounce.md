# Body Motion And Bounce

## Goal

Separate three different concepts that are easy to accidentally mash together:

1. Material response
- how bouncy / slippery a tile or entity is

2. Motion policy
- whether a thing is a grounded actor, a loose body, a projectile, etc.

3. Condition / gameplay state
- stunned, dead, normal, and so on

The main reason to split these is that the same entity can need different collision behavior depending on its gameplay condition.

Example:
- a caveman walking on dirt should not bounce
- the same caveman, once stunned and tumbling, should bounce
- same entity, same tile, different result

That means a pure material-only system is not enough.

## What Classic-HD Does

Classic-HD has two similar but separate bounce families:

1. Loose item bounce
- items use bounce + friction in `oItem/Step_0.gml`
- jars, rocks, dice, etc. live here

2. Stunned body bounce
- heavy stunnable enemies call `scrCheckCollisions()` while `status >= STUNNED`
- that script applies side damping, floor bounce, and friction
- caveman / yeti / hawkman / shopkeeper all opt into this pattern manually

So the original game clearly has a shared bounce recipe, but it is not one globally unified system.

## Recommended Splonks Shape

Use two layers.

### 1. Material data

Put these in tile and entity archetypes:

- restitution / bounciness
- friction

This gives a clean place for:
- ice vs dirt vs rubbery surfaces later
- rocks vs jars vs other loose objects

### 2. Motion policy

Add a small motion/body mode separate from condition:

```cpp
enum class BodyMotionMode : std::uint8_t {
    GroundedActor,
    LooseBody,
};
```

Then collision response can do:

- `GroundedActor`
  - land and stick
  - do not bounce vertically in the normal locomotion path
- `LooseBody`
  - use bounce + friction response

That lets the same entity swap behavior cleanly:
- normal caveman -> `GroundedActor`
- stunned caveman -> `LooseBody`
- dead but still moving caveman -> `LooseBody`
- settled corpse -> effectively at rest

## What Not To Do

Avoid these shortcuts:

- a single `bounce_on_stun` bool with no motion model behind it
- caveman-only bounce hacks
- making condition alone determine all physics behavior
- forcing pots / rocks / stunned enemies into one undifferentiated path

## Practical Plan

Not for immediate implementation. This is a later cleanup.

When we return to it:

1. Add material fields to tile/entity archetypes.
2. Add `BodyMotionMode` or equivalent to shared entity/body logic.
3. Move loose-object bounce to the generalized material + motion-policy path.
4. Let stunned/dead-moving enemies opt into `LooseBody`.
5. Keep entity-specific sprite choice local.

## Current Decision

Do not work on bounce right now.

For caveman, finish the non-bounce behavior first:
- aggro / attack run
- HP / stun wakeup behavior
- idle wall-hop
- corpse settle behavior
