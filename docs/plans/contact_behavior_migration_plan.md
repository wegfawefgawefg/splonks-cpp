# Contact Behavior Migration Plan

This doc lists gameplay behavior that currently lives in step-time overlap/query code,
but may want to move into the newer collision/contact dispatch system.

The buckets are:
- `Should Move`
- `Good Candidates`
- `Not Really`

## Should Move

### `common_damage.cpp`

These are still classic step-time overlap contact systems and are the strongest
targets for migration:

- `MaybeHurtAndStunOnContact(...)`
- `MaybeHurtAndStunOnContactAsProjectile(...)`

Why:
- this is real `entity↔entity` contact behavior
- it currently depends on snapshot `SID` overlap queries
- it wants the same pixel-accurate timing as the newer swept contact path

### `common_stomp.cpp`

- `TryStompEntitiesBelow(...)`

Why:
- this is really downward contact resolution
- it currently uses a small step-time foot overlap box
- it would be cleaner as a contact rule driven by actual movement/contact

### `block.cpp`

The crush section is really collision behavior:

- `CRUSH ZONE`

Why:
- this is damage from contact/overlap under a moving block
- it does not really belong in free-form block step logic

## Good Candidates

### `common_collect.cpp`

- `TryCollectEntityFromContact(...)`

Current recommendation:
- moved to collector-driven `entity↔entity` contact dispatch

Why:
- gold already participates in the same entity overlap world
- collector-driven contact keeps the behavior explicit and local
- no special contact cooldown is needed because pickup destroys the collectible

Open design question:
- should the player/entity query and collect nearby gold
- or should the gold detect that it is being collected and add itself to the collector

Current lean:
- collector-driven
- collector contact handler calls one boring helper
- easy to extend later if more collector types want pickups

### `block.cpp`

The repel section is contact-ish, but less urgent:

- `REPEL THINGS FROM CENTER`

Why:
- it behaves more like a persistent field/zone than a sharp collision impact
- it may stay as a step-time query if that reads better

### `common_carry.cpp`

Pickup targeting is overlap/query based:

- the "trying to pick up these" search

Why:
- this is contact-adjacent
- but it is input/interaction driven, not really physical collision resolution

## Not Really

### `common_step.cpp`

These should stay in ordinary step code:

- `ApplyDeactivateConditions(...)`
- `StoreHealthToLastHealth(...)`
- `StepStunTimer(...)`
- `StepTravelSoundWalkerClimber(...)`
- `DoThrownByStep(...)`

Why:
- they are lifecycle/state/timer/audio systems
- they are not really contact resolution

### `bat.cpp`

Keep in step:
- AI
- perch/return logic
- chase logic

Only contact behavior should migrate, not bat behavior as a whole.

### `rock.cpp`

Rock step itself does not currently contain the actual hit behavior.

Why:
- its current logic is mostly control/movement setup
- when real rock impact behavior is implemented, that behavior should go in contact code
- but there is not much to migrate from rock step right now

## Suggested Order

1. `common_collect.cpp`
   - done: moved to collector-driven entity contact
2. `common_stomp.cpp`
3. `common_damage.cpp`
4. `block.cpp` crush behavior
5. reassess `block.cpp` repel and `common_carry.cpp`
