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

The old step-time crush/repel logic has been moved onto contact-driven block shove:

Why:
- moving blocks now shove contacted entities one pixel at a time
- if the shove fails, they apply crush damage
- this is the reusable shape future crushers / shields / pushers want

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

Block shove/crush is now contact-driven, but the general reusable pusher/crusher
surface still wants follow-up:

Why:
- future directional pushers should be able to reuse the same 1-pixel shove path
- the current generic helper is small, but the behavior surface is still block-first

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
3. `block.cpp` shove/crush
   - done: moved to contact-driven 1-pixel shove + crush on failed shove
4. `common_damage.cpp`
5. reassess future generic pushers and `common_carry.cpp`
