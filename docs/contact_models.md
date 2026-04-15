# Contact Models

This document describes the current entity/entity contact systems in `splonks-cpp`,
which side owns each interaction, and which cooldown bookkeeping each path uses.

It exists because the code currently mixes a few different notions of "contact":
blocking resolution, pair dispatch, one-sided gameplay effects, and multiple cooldown
layers.

## Current Layers

### 1. Blocking / impassable resolution

Files:
- `src/entities/common/physics.cpp`
- `src/entities/common/entity_contact.cpp`

This is mostly mover-owned. A sweeping or probing entity resolves against tiles and
impassable entities. The other side does not participate in a symmetric physics solve.

Properties:
- one-sided
- no gameplay cooldown attached by default
- determines whether movement is blocked / sweep stops

### 2. Entity/entity contact dispatch shell

File:
- `src/entities/common/entity_contact.cpp`

For one touched pair, the dispatcher gives both participants a chance to run their
contact logic:
- participant A against B
- participant B against A

This is dual-dispatch at the shell level, but most actual gameplay effects below are
still one-sided.

Same-tick dedupe:
- `entity_contact_dispatches_this_tick`
- unordered pair
- implemented in `src/contact_bookkeeping.cpp`
- only prevents duplicate pair dispatch within one tick when `context.direction == 0`

### 3. Normal hurt-on-contact body damage

File:
- `src/entities/common/contact_damage.cpp`

This is source-owned.

Flow:
- source entity with `hurt_on_contact` searches overlaps
- source tries to damage target
- on hurt/die, source applies body-contact knockback to target

Cooldown:
- `interaction_cooldowns`
- ordered
- kind = `InteractionCooldownKind::Harm`

Current body-contact knockback:
- horizontal: `+/-1`
- vertical: `-1`
- velocity replacement

### 4. Projectile-body contact

Files:
- `src/entities/common/contact_damage.cpp`
- `src/entities/common/step.cpp`
- `src/contact_bookkeeping.cpp`

This is also source-owned.

Flow:
- source entity is considered a projectile body if `projectile_contact_timer > 0`
  and speed is high enough
- source searches overlaps
- source tries to damage target
- if hurt/die, or if target is stunned/dead and allowed to absorb impact without
  taking damage, source applies projectile knockback to target

Cooldown:
- `projectile_body_impact_cooldowns`
- ordered
- `A -> B` and `B -> A` are separate entries
- currently used to suppress repeated same-direction projectile body impact spam

Current projectile-body knockback:
- horizontal: inherited from source velocity
- vertical: minimum pop-up of `-1`
- additive

Projectile lifetime:
- managed in `src/entities/common/step.cpp::DoThrownByStep`
- projectile state only counts down while grounded and nearly settled
- otherwise it refreshes back to full duration

### 5. Stomp

File:
- `src/entities/common/stomp.cpp`

This is explicitly stomper-owned.

Flow:
- player-like stomper checks downward motion and target head band
- stomper damages stomped target
- stomp may force stunned state and projectile-style knockback on target
- stomper gets bounce upward

Cooldown:
- writes an ordered reverse harm cooldown from stomped -> stomper for 1 frame
- purpose is to stop immediate retaliation that same frame

### 6. Crusher / pusher / crush

File:
- `src/entities/common/push.cpp`

This is mover-owned.

Flow:
- crusher/pusher tries to displace target by one pixel in the push direction
- if displacement fails, crusher tries crush damage on target

Cooldown:
- no special pair cooldown currently

### 7. Type-specific contact handlers

File:
- `src/entities/common/entity_contact.cpp`

Examples:
- baseball bat
- pot impact
- box impact
- collection

These are generally participant-owned. The dual-dispatch shell may call both sides,
but each handler typically only acts for one entity type.

Cooldowns:
- `contact_cooldowns`: ordered, used for type-specific raw contact throttling
- current notable example: baseball bat contact cooldown

## Current Cooldown Types

Defined in `src/contact_bookkeeping.hpp`.

### `contact_cooldowns`
- ordered
- low-level per-source/per-target contact throttling
- currently used by some type-specific contact handlers

### `interaction_cooldowns`
- ordered
- gameplay interaction throttling
- currently only `Harm`
- used by normal body harm contact and stomp retaliation suppression

### `entity_contact_dispatches_this_tick`
- unordered
- only same-tick pair dedupe for the dispatch shell
- not a gameplay cooldown

### `projectile_body_impact_cooldowns`
- ordered
- projectile body impact throttling only
- does not gate blocking resolution, push, crush, or other contact systems

## Practical Summary

The codebase currently uses:
- one-sided blocking resolution
- dual-dispatch shell for entity/entity contact
- mostly one-sided gameplay consequences
- several cooldown tables with different scopes

That means the important question is always:
- are we deduping pair dispatch,
- throttling a gameplay effect,
- or trying to model actual two-body physics?

Those are different jobs and should stay separate.

## Good Future Cleanup Targets

1. Keep `entity_contact_dispatches_this_tick` as pure dispatch dedupe only.
2. Keep projectile-body cooldown scoped only to projectile-body impact consequences.
3. If we ever want projectile-vs-projectile body smacks to feel physically correct,
   add a dedicated pairwise resolver instead of adding more heuristics to the current
   one-sided projectile path.
4. Avoid adding more cooldown tables unless the scope is genuinely different from the
   three gameplay-facing ones above.
