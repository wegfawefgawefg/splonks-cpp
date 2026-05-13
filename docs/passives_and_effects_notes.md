# Passives And Effects Notes

## Current Shape

Persistent passives and temporary gameplay states now use the same fixed-slot effect system.

Ents store:

```cpp
BoxedEntEffects effects;
```

`BoxedEntEffects` is empty for normal ents. It allocates one fixed-slot `EntEffects` payload only when an ent actually has effects, and deep-copies that payload for replay snapshots.

`EffectInstance` is intentionally generic:

- `id`: resolves to a C++ effect spec.
- `count`: stack count, charges, or spec-specific accumulated points.
- `value`: spec-specific scalar.
- `frames_remaining`: timer for future timed effects.

Pickup ents point at an effect with `pickup_effect`. Collecting the pickup adds that effect to the collector.

## Effect Specs

Effect behavior is registered in C++ specs.

Each spec can provide:

- a debug name
- a HUD icon
- a UI kind, such as hidden or passive
- simple numeric modifiers
- an optional hook handler
- an optional expiry predicate

Common systems should not branch on concrete passive names. They should ask for an effective value:

```cpp
GetModifiedEffectValue(ent, EffectModifierTarget::GravityScale, 1.0F);
```

Special behavior belongs in the effect hook handler. Example: mitt reacts to a throw hook and applies `NoGravityUntilContact` to the thrown ent.

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

Some old passive behavior still uses direct effect ident checks because there is not yet a useful generic modifier target for it.

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

- effects are non-physical state on an ent
- tools are physical ents that can be held, equipped, bought, dropped, or thrown

This keeps the system C++-native and easy to debug while still allowing future C++ content/mod specs.
