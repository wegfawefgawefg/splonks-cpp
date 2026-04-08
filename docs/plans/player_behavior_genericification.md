# Player Behavior Genericification Plan

## Goal

Pull reusable gameplay behaviors out of `entities/player.cpp` one at a time so:

- non-player entities can use them
- possessed entities can use them
- behavior stays explicit and easy to debug
- entities that do not opt in do not pay for the behavior

This is not a blind file-splitting exercise. The goal is to extract reusable
behavior with clear ownership and clear opt-in points.

## Constraints

- Preserve current player behavior unless the specific extraction changes it on
  purpose.
- Do not introduce broad always-on per-entity scans if only a few entities use
  the behavior.
- Prefer explicit opt-in flags, labels, or helper calls over magical generic
  systems.
- Test each extraction with at least:
  - player
  - one non-player test entity that uses the behavior
  - one dedicated test room/level setup

## Design Rule: No Cost Unless Used

Shared behavior should be structured so entities only pay for it when they use
it.

Examples:

- contact pickup:
  - only run for entities marked as collectors
- stomp logic:
  - only run for entities marked as stompers
- push-block logic:
  - only run for entities marked as block pushers
- carry/use logic:
  - only run for entities marked as carriers/users

This can be done with:

- explicit capability flags on `Entity`
- explicit labels/relationships already present on `Entity`
- shared helpers called only from entities that need them

Avoid “every entity checks every behavior every frame.”

## Candidate Behaviors To Pull Out Of Player

### 1. Carry / Pickup / Throw / Drop

Currently in player:

- pick up nearby carryable entity
- maintain `holding_vid`
- throw held entity
- drop held entity
- set `held_by_vid`
- set thrown immunity / throw source

Why generic:

- enemies can pick up actors/items
- enemies can throw actors/items
- possessed entities may want this too

Good first shared API shape:

- `TryPickUpEntity(...)`
- `DropHeldEntity(...)`
- `ThrowHeldEntity(...)`
- `UpdateHeldEntityPlacement(...)`

Likely entity opt-in:

- `can_pick_up`
- `can_throw`
- `can_carry`

Test entity idea:

- a simple humanoid dummy that can pick up and throw rocks/pots/player

### 2. Equip To Back / Unequip

Currently in player:

- move held item to `back_vid`
- take back item off
- maintain back-item placement
- special display-state offsets for climbing/hanging

Why generic:

- backpack-like and weapon-back behavior should not be player-only
- future entities may wear items

Likely shared API:

- `TryEquipHeldItemToBack(...)`
- `TakeBackItemOff(...)`
- `UpdateBackItemPlacement(...)`

Likely entity opt-in:

- `can_equip_back_items`

Test entity idea:

- a dummy that can equip/unequip a jetpack or weapon

### 3. Block Pushing

Currently in player:

- detects nearby blocks
- applies push acceleration to blocks
- updates player `state = Pushing`

Why generic:

- possessed entities could push blocks
- enemy types may push blocks or heavy objects

Likely shared API:

- `TryPushNearbyBlocks(...)`

Likely entity opt-in:

- `can_push_blocks`
- maybe a configurable `push_strength`

Test entity idea:

- a simple enemy that walks into blocks and shoves them

### 4. Stomp / Jump-On

Currently in player:

- foot-zone query
- enemy detection
- damage as `JumpOn`
- bounce player upward
- apply thrown-by stun ownership

Why generic:

- this is a general “stomp attacker” behavior
- not inherently player-only

Likely shared API:

- `TryStompEntitiesBelow(...)`

Likely entity opt-in:

- `can_stomp`
- maybe `stomp_damage`
- maybe `stomp_bounce_velocity`

Test entity idea:

- a special enemy or test dummy that can bounce-kill bats/mice/etc.

### 5. Collector Contact / Money Pickup

Currently in player:

- contact query over player `cbox`
- collect `Gold` / `GoldStack`
- add money
- destroy pickup

Why generic:

- some entities may collect treasure/items
- possession experiments may want collectors other than player

Important note:

- this should not live in money physics itself
- better as shared collector-side behavior or a shared contact helper

Likely shared API:

- `CollectTouchingPickups(...)`

Likely entity opt-in:

- `can_collect_money`
- later maybe more general `collector_mask`

Test entity idea:

- a dummy collector that vacuums up gold on contact

### 6. Thrown Utility Spawn: Bomb / Rope

Currently in player:

- cooldown bookkeeping
- entity spawn
- throw vector assembly from control intent
- thrown source/immunity setup

Why generic:

- bomb and rope are both “spawn and throw utility entity” behaviors
- future actors may use one or both

Likely shared API:

- `TryThrowUtilityEntity(...)`
- with parameters for:
  - entity type/setup function
  - inventory count reference
  - cooldown reference
  - immunity duration

Likely entity opt-in:

- `can_throw_bombs`
- `can_throw_ropes`

Test entity idea:

- a dummy that can throw ropes or bombs on command

### 7. Weapon Use / Attack Spawn

Currently in player:

- bat attack cooldown
- spawn baseball bat entity
- attach to holder

Why generic:

- weapon use should not stay player-only
- future enemies may swing melee weapons

Likely shared API:

- `TrySpawnMeleeAttackEntity(...)`

Likely entity opt-in:

- `can_use_weapons`
- maybe weapon-specific inventory/reference later

Test entity idea:

- a dummy that swings a bat

## Suggested Order

Extract in this order:

1. Collector contact / money pickup
2. Block pushing
3. Stomp / jump-on
4. Carry / pickup / throw / drop
5. Equip to back / unequip
6. Weapon use / attack spawn
7. Bomb / rope utility throw

Reason:

- the first three are smaller and more self-contained
- carry/equip have more state coupling
- weapon and utility throw have more spawn/cooldown coupling

## Test Strategy

For each extracted behavior:

1. Add one dedicated test room/preset.
2. Add one non-player test entity that uses the behavior.
3. Verify:
   - player still behaves the same
   - the test entity can use the shared behavior
   - unrelated entities do not run that code path

## First Candidate

Best first target:

- `CollectTouchingPickups(...)`

Reason:

- low state coupling
- easy to gate behind a collector capability
- easy to test with a simple collector dummy
- useful for both player and future entities
