# Effect Modifier Refactor Plan

## Goal

Replace scattered passive bit checks with a unified `Effect` system.

In this model, current "passives" are persistent effects. Temporary buffs/debuffs are timed or condition-expiring effects. Effect-specific behavior uses narrow hooks, not a global internal event bus.

This should improve separation between content and shared gameplay code:

- common systems should not know about specific effects like mitt, spike shoes, spring shoes, or poison.
- common systems should ask for effective values through modifier getters.
- special behavior should live in effect archetype hook handlers.
- effects should be applied in arbitrary order and may carry instance state, such as counters.
- tools remain physical entities and are not folded into this system.

This is not intended to be a scripting system. Mods are expected to add C++ archetypes and recompile.

## Current Problem

Current passives use `EntityPassiveItem` and `passive_item_flags`.

That works for a small fixed item list, but it encourages code like:

```cpp
if (HasPassiveItem(entity, EntityPassiveItem::SpringShoes)) {
    // special behavior
}
```

Those checks leak passive identity into player control, damage, physics, render, and UI paths. As more passives and temporary states are added, the shared systems become harder to reason about.

## Core Terms

`Effect`: non-physical gameplay state on an entity. Effects may be persistent, timed, or explicitly cleared by gameplay. Examples: mitt, spring shoes, meathead, parachute, burning, slow, no gravity until contact.

`Modifier`: a typed numeric operation contributed by an effect. Examples: add jump impulse, override spike damage taken, add throw boost, multiply gravity.

`Effect hook`: a narrow callback point that effects can react to. Examples: throw, jump, stomp, fall update, contact, nearby death, render UI.

`Tool`: physical entity that can be held, equipped, or used. Tools stay separate.

## Effect Instance Storage

Store effect instances on entities:

```cpp
struct EffectInstance {
    EffectId id;
    std::int32_t count = 0;
    float value = 0.0F;
    std::uint32_t frames_remaining = 0;
};

struct EntityEffects {
    EffectInstance effects[kMaxEntityEffects];
    std::uint8_t count = 0;
};

BoxedEntityEffects effects;
```

The instance fields are intentionally generic.

The boxed field keeps normal entities cheap: entities with no passives or temporary effects store only a pointer-sized empty box, while entities with effects allocate a fixed-slot payload. Replay copies deep-copy the box so snapshots do not alias live state.

- `count`: stack count, remaining charges, or accumulated points.
- `value`: charge, cooldown, intensity, progress, or archetype-specific scalar.
- `frames_remaining`: timer for effects that expire by time.

Examples:

- `Meathead`: persistent effect, tracks accumulated death points in `count`.
- `Parachute`: persistent counted effect, tracks remaining deploys in `count` and removes itself after deploy.
- `NoGravityUntilContact`: hidden temporary effect, no count needed.
- future poison/burning: timed status effect using `frames_remaining`.

## Effect Archetypes

Each effect id resolves to an archetype:

```cpp
struct EffectArchetype {
    EffectId id;
    const char* debug_name;
    FrameDataId icon;

    EffectUiKind ui_kind;
    const EffectExpiryPolicy* expiry;

    std::span<const Modifier> modifiers;

    EffectHookMask hook_mask;
    EffectHookHandler on_hook = nullptr;
};
```

`EffectUiKind` controls presentation only:

```cpp
enum class EffectUiKind {
    Hidden,
    Passive,
    Buff,
    Debuff,
};
```

The UI can sort by `EffectUiKind`, but modifier evaluation must not depend on UI order.

## Expiry Policies

Use reusable C++ expiry policy functions instead of a growing `UntilX` lifetime enum.

```cpp
using EffectShouldExpire = bool (*)(
    const EffectInstance& effect,
    const EffectHookContext& hook,
    const EffectContext& ctx
);

struct EffectExpiryPolicy {
    const char* debug_name = "Never";
    std::uint32_t default_frames = 0;
    EffectShouldExpire should_expire = nullptr;
};
```

Common reusable policies:

```cpp
extern const EffectExpiryPolicy kExpireNever;
extern const EffectExpiryPolicy kExpireOnTimer;
extern const EffectExpiryPolicy kExpireOnBlockingContact;
extern const EffectExpiryPolicy kExpireOnGrounded;
extern const EffectExpiryPolicy kExpireOnBlockingContactOrGrounded;
```

Effects can also remove themselves explicitly from `on_hook`, as parachute likely will.

Examples:

```cpp
// Persistent passive-like effect.
expiry = &kExpireNever;

// Timed poison.
expiry = &kExpireOnTimer;
frames_remaining = 300;

// Mitt-thrown no-gravity state.
expiry = &kExpireOnBlockingContactOrGrounded;
```

The debug UI should show the policy `debug_name`.

## Generic Modifiers

Modifiers should be generic typed operations, not named fields like `spike_immunity`.

```cpp
enum class ModifierTarget {
    GravityScale,
    JumpImpulse,
    StompDamage,
    StompImpulse,
    ThrowHorizontalBoost,
    ThrowVerticalBias,
    ClimbProbeCount,
    SpikeDamageTaken,
    HiddenTreasureVisibility,
};

enum class ModifierOp {
    Add,
    Multiply,
    Override,
    Min,
    Max,
};

struct Modifier {
    ModifierTarget target;
    ModifierOp op;
    float value;
};
```

Gameplay code should read modifiers through getters:

```cpp
float GetModifiedValue(
    const Entity& entity,
    ModifierTarget target,
    float base_value
);
```

The operation order must be deterministic. Proposed order:

1. start with `base_value`
2. apply `Add`
3. apply `Multiply`
4. apply `Min` / `Max` clamps
5. apply `Override` last

This keeps simple effects data-like while still allowing C++ behavior where needed.

## Effect Hooks

Special behavior should use narrow effect/content hooks rather than a long list of effect-specific hook fields.

```cpp
enum class EffectHook {
    Throw,
    Jump,
    Stomp,
    FallUpdate,
    BlockingContact,
    TileContact,
    EntityContact,
    DamageDealt,
    DamageTaken,
    DeathNearby,
    Pickup,
    RenderUi,
};

struct EffectHookContext {
    EffectHook type;
    VID actor;
    VID target;
    Vec2 pos;
    Vec2 velocity;
    DamageType damage_type;
    unsigned int damage_amount;
};
```

Each effect archetype has a hook mask. The hook runner only calls effects that opted into the hook.

```cpp
using EffectHookHandler = void (*)(
    EffectContext& ctx,
    EffectInstance& effect,
    const EffectHookContext& hook
);
```

This keeps the API less opinionated than fields like `on_throw`, `on_jump`, `on_stomp`, and `on_spike_contact`.

## Existing Effects Mapping

`Mitt`

- ui kind:
  - `Passive`
- expiry:
  - `kExpireNever`
- modifiers:
  - `ThrowHorizontalBoost Add 6.0`
- hook handler:
  - on `Throw`, apply `NoGravityUntilContact` to the thrown entity.
- note:
  - the player added a `mitt_no_grab` UI asset. This can be used later for failed auto-catch feedback or a temporary "cannot catch" status if needed.

`NoGravityUntilContact`

- ui kind:
  - `Hidden`
- expiry:
  - `kExpireOnBlockingContactOrGrounded`
- modifiers:
  - `GravityScale Override 0.0`
- hook handler:
  - none required.

`SpringShoes`

- ui kind:
  - `Passive`
- expiry:
  - `kExpireNever`
- modifiers:
  - `JumpImpulse Add <tuned spring bonus>`
  - `StompImpulse Add <tuned spring bonus>`

`SpikeShoes`

- ui kind:
  - `Passive`
- expiry:
  - `kExpireNever`
- modifiers:
  - `SpikeDamageTaken Override 0.0`
  - `StompDamage Add <tuned spike shoe bonus>`

`Gloves`

- ui kind:
  - `Passive`
- expiry:
  - `kExpireNever`
- modifiers:
  - likely `ClimbProbeCount` or a future climb/hang capability target.
- note:
  - exact mapping should be decided while migrating hang/climb code.

`Spectacles`

- ui kind:
  - `Passive`
- expiry:
  - `kExpireNever`
- modifiers:
  - `HiddenTreasureVisibility Max 1.0`

`UdjatEye`

- ui kind:
  - `Passive`
- expiry:
  - `kExpireNever`
- modifiers:
  - `HiddenTreasureVisibility Max 1.0`
- future behavior:
  - black market or special-exit guidance should be hook/UI driven, not embedded in visibility checks.

`Compass`

- ui kind:
  - `Passive`
- expiry:
  - `kExpireNever`
- hook handler:
  - on `RenderUi` or equivalent HUD/world-overlay hook, draw compass guidance.
- alternative:
  - a generic modifier target could expose compass behavior, but a hook handler is likely cleaner.

`Meathead`

- ui kind:
  - `Passive`
- expiry:
  - `kExpireNever`
- hook handler:
  - on `DeathNearby`, increment `effect.count`.
  - when count reaches threshold, spend points and grant health.

`Parachute`

- ui kind:
  - `Passive`
- expiry:
  - `kExpireNever`
- instance state:
  - `count` is deploy charges.
- hook handler:
  - on `FallUpdate`, deploy when fall conditions are met.
  - decrement `effect.count`.
  - remove effect when count reaches zero.

## Migration Rules

Be strict during migration:

- Do not add new `HasPassiveItem(...)` checks.
- Existing passive checks should be converted to either a modifier getter or a narrow effect/content hook.
- Common physics/combat/movement code should ask for effective values, not passive identity.
- Effect-specific behavior belongs in effect archetype handlers.
- Tools stay physical entities.
- Tile/area effects can later apply one-frame or persistent effects to entities, but should not be mixed into the tool system.

Examples:

```cpp
// Avoid:
if (HasPassiveItem(entity, EntityPassiveItem::SpikeShoes)) {
    damage = 0;
}

// Prefer:
damage = GetModifiedValue(entity, ModifierTarget::SpikeDamageTaken, damage);
```

```cpp
// Avoid:
if (HasPassiveItem(holder, EntityPassiveItem::Mitt)) {
    SetTemporaryEffect(thrown, EntityTemporaryEffect::NoGravityUntilContact, true);
}

// Prefer:
ApplyEffectHook(EffectHook::Throw, holder.vid, thrown.vid);
```

## Migration Order

1. [x] Add effect ids, effect instances, effect archetypes, modifier getters, expiry predicates, and hook plumbing.
2. [x] Replace passive pickup data with `pickup_effect`.
3. [x] Migrate `SpringShoes` and `SpikeShoes`.
4. [x] Migrate `Mitt` throw boost and throw hook behavior.
5. [x] Migrate `NoGravityUntilContact` from interim temp flag into an effect archetype.
6. [x] Migrate `Spectacles` and `UdjatEye` visibility checks.
7. [x] Migrate `Compass` UI behavior.
8. [x] Migrate `Meathead` and `Parachute` as stateful persistent effects.
9. [x] Remove `passive_item_flags`, `EntityPassiveItem`, and interim `EntityTemporaryEffect`.

## Open Questions

- Should modifier getters compute on read, or should modifiers be cached once per entity step?
- Should `Override` always win last, or should there be priority for multiple overrides?
- Should effect ids reuse the same runtime content id machinery planned for entities/tiles/audio?
- How should debug UI expose effect instance `count` and `value` without becoming effect-specific?
- Should stack behavior be handled by effect archetype policy or by the caller applying the effect?

## Implemented Decision

The old passive bitset and interim temporary-effect bitset have been removed.

`NoGravityUntilContact` is now a hidden effect. Mitt runs a generic throw hook, and the mitt effect hook applies that hidden effect to the thrown entity.
