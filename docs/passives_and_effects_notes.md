# Passives And Effects Notes

## Current Shape

Persistent passives and temporary gameplay states now use the same fixed-slot effect system.

Entities store:

```cpp
BoxedEntityEffects effects;
```

`BoxedEntityEffects` is empty for normal entities. It allocates one fixed-slot `EntityEffects` payload only when an entity actually has effects, and deep-copies that payload for replay snapshots.

`EffectInstance` is intentionally generic:

- `id`: resolves to a C++ effect archetype.
- `count`: stack count, charges, or archetype-specific accumulated points.
- `value`: archetype-specific scalar.
- `frames_remaining`: timer for future timed effects.

Pickup entities point at an effect with `pickup_effect`. Collecting the pickup adds that effect to the collector.

## Effect Archetypes

Effect behavior is registered in C++ archetypes.

Each archetype can provide:

- a debug name
- a HUD icon
- a UI kind, such as hidden or passive
- simple numeric modifiers
- an optional hook handler
- an optional expiry predicate

Common systems should not branch on concrete passive names. They should ask for an effective value:

```cpp
GetModifiedEffectValue(entity, EffectModifierTarget::GravityScale, 1.0F);
```

Special behavior belongs in the effect hook handler. Example: mitt reacts to a throw hook and applies `NoGravityUntilContact` to the thrown entity.

## Implemented Effects

- `Gloves`: persistent passive marker used by hang logic.
- `Spectacles`: reveals hidden treasure via `HiddenTreasureVisibility`.
- `Compass`: persistent passive consumed by HUD compass rendering.
- `Mitt`: adds throw boost and applies no-gravity-until-contact to thrown items.
- `SpringShoes`: persistent passive used by jump and stomp bounce logic.
- `SpikeShoes`: overrides spike damage to zero and raises stomp damage.
- `UdjatEye`: reveals hidden treasure via `HiddenTreasureVisibility`.
- `Meathead`: persistent passive with point count stored in `EffectInstance::count`.
- `Parachute`: persistent counted effect consumed on deploy.
- `NoGravityUntilContact`: hidden temporary effect cleared on grounded or blocking contact hooks.

## Remaining Cleanup

Some old passive behavior still uses direct effect identity checks because there is not yet a useful generic modifier target for it.

Examples:

- gloves side-hang eligibility
- spring shoe jump/bounce sound feedback
- compass HUD rendering
- parachute deployment visuals

Those checks are acceptable for now because they are in gameplay/content-facing code, not engine plumbing. If more effects need the same interaction, add a generic modifier target or a narrow content hook rather than adding more scattered concrete checks.

## Future Direction

Timed effects should use `frames_remaining` and a timer expiry predicate.

More hook types can be added as real use cases appear. Avoid adding opinionated hook fields like `on_jump`, `on_stomp`, and `on_spike_contact` unless the generic hook model becomes too vague in practice.

Effects and tools remain separate:

- effects are non-physical state on an entity
- tools are physical entities that can be held, equipped, bought, dropped, or thrown

This keeps the system C++-native and easy to debug while still allowing future C++ content/mod archetypes.
