# Passives And Effects Notes

## Current State

Players and other entities currently use a few separate mechanisms for persistent and temporary gameplay state:

- `passive_item_flags` stores persistent passive item ownership, such as mitt, spring shoes, spike shoes, compass, and meathead.
- `movement_flags` stores transient movement state, such as walking, running, pushing, climbing, and hanging.
- dedicated timers store specific state, such as stun, fall, projectile contact, and throw immunity.
- entity-local `counter_a` through `counter_d` store archetype-specific scratch state.
- `temporary_effect_flags` is a small interim bitset for simple temporary effects. It currently supports `NoGravityUntilContact` for mitt-thrown items.

This is workable for the current scope, but the passive bitset and one-off timers should not grow into a large set of hardcoded special cases scattered through engine and gameplay code.

## Terms

`Passive`: persistent owned capability or upgrade. It is usually shown in the UI and saved on the character. Examples: mitt, spring shoes, spike shoes, compass, meathead.

`Temporary effect`: runtime state applied to an entity for a limited duration or until a lifecycle event clears it. Examples: no gravity until contact, burning, frozen, slow, invulnerable until landing.

`Modifier`: a simple numeric or boolean adjustment derived from passives and temporary effects. Examples: gravity scale, jump impulse bonus, spike immunity, movement speed scale, damage scale.

`Gameplay event`: a fact emitted by gameplay code that passives/effects can react to. Examples: throw, jump, stomp, damage dealt, damage taken, death, tile contact, entity contact, pickup.

## Preferred Long-Term Shape

Keep passives and effects separate, but let both feed an `EntityModifiers` aggregate.

Passives should eventually move away from a raw bitset enum toward a fixed-size list of passive ids:

```cpp
struct PassiveSlot {
    PassiveId id;
};

PassiveSlot passives[kMaxPassives];
std::uint8_t passive_count;
```

Each passive id points to a C++ archetype:

```cpp
struct PassiveArchetype {
    PassiveId id;
    FrameDataId icon;
    EntityModifiers static_modifiers;
    GameplayEventMask event_mask;
    PassiveEventHandler on_event;
};
```

Temporary effects should use fixed-size slots, not heap allocations:

```cpp
struct ActiveEffect {
    EffectId id;
    std::uint32_t frames;
    float amount;
};

ActiveEffect effects[kMaxEffects];
std::uint8_t effect_count;
```

Each effect id points to a C++ archetype:

```cpp
struct EffectArchetype {
    EffectId id;
    EntityModifiers static_modifiers;
    GameplayEventMask event_mask;
    EffectEventHandler on_event;
};
```

At the start of each entity step, rebuild modifiers from passives and active effects:

```cpp
entity.modifiers = EntityModifiers::Identity();
ApplyPassiveModifiers(entity);
ApplyEffectModifiers(entity);
```

Physics, combat, and UI read the already-computed modifiers instead of repeatedly checking passives and effects directly.

## Why Event Handlers Instead Of Many Hook Fields

A passive archetype with fields like `on_throw`, `on_jump`, `on_stomp`, and `on_spike_contact` is too opinionated. It forces the passive API to grow every time a new kind of interaction matters.

A generic event handler is less opinionated:

```cpp
enum class GameplayEventType {
    Throw,
    Jump,
    Stomp,
    TileContact,
    EntityContact,
    DamageDealt,
    DamageTaken,
    Death,
    Pickup,
};

struct GameplayEvent {
    GameplayEventType type;
    VID actor;
    VID target;
    Vec2 pos;
    Vec2 velocity;
    DamageType damage_type;
    unsigned int damage_amount;
};
```

Passives/effects can opt into event masks so we do not broadcast every event to every passive.

This gives C++ mod authors real behavior hooks without embedding Lua or inventing a huge data DSL.

## Interim Rule

Until the full system exists, simple temporary mechanics can use `EntityTemporaryEffect` and `temporary_effect_flags`.

Rules for this interim bitset:

- Keep entries generic. Avoid names tied to one item, such as `MittThrow`.
- Do not put item-specific branching in common engine code.
- Use common code only for generic behavior, such as "no gravity until contact".
- If this grows beyond a handful of entries, replace it with fixed-size active effect slots and effect archetypes.

Current interim effect:

- `NoGravityUntilContact`: physics skips gravity while the flag is present. The flag clears when the entity is grounded or after a blocking collision is observed.

