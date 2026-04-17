# Gameplay Systems Backlog

This document lists major missing gameplay systems and feature gaps that are now
large enough to matter for Spelunky-like progression and feel.

The point is not to lock implementation yet. It is a concrete backlog with short
notes about what each feature will probably need.

## Core Missing Systems

### Shop
- Needs an authored shop zone, not just shop-themed tiles and a shopkeeper entity.
- Needs unpaid item ownership, shop crime detection, and a clear shop state.
- Should be the owner of future shopkeeper hostility, forgiveness, and debt rules.

### Shop item ownership system
- Items inside the shop need an ownership bit or owner reference.
- The system should know when an item becomes stolen: pickup, carry out, damage,
  or terrain break around it.
- This should be shared state, not hardcoded separately per item type.

### Shopkeeper AI
- Needs idle/shop behavior, aggro behavior, pursuit, firing, and recovery rules.
- Should read shop state instead of each crime directly hardcoding shopkeeper reactions.
- Later this also wants group hostility and persistence across stages.

### Idol disturbance and boulder trap
- Picking up the idol or otherwise disturbing it should fire a stage trigger.
- That trigger should be able to spawn or activate the boulder and any related
  terrain or entity changes.
- This wants a reusable stage trigger/callback mechanism, not a one-off idol hack.

### Idol gold reward
- The idol should pay out when it reaches the exit.
- It should also pay out when it is moved into a shop.
- This is not just an idol rule; it touches stage transitions and shop ownership.

### Kali sacrifice
- Altars need a sacrifice zone and a body consumption rule.
- Sacrifice should award favor, and favor should later drive rewards/punishments.
- This wants a lightweight favor system, not just altar-local side effects.

### Hold onto moving things
- Right now attachments are mostly "held item follows holder", not "rider inherits mover motion".
- Needed for hanging onto things that move, riding moving carriers, and more faithful
  moving-object behavior.
- This is physics-invasive and should be treated as a real attachment/motion-transfer system.

### Exit should require an input
- The exit should not auto-trigger on touch.
- Player should need to be on the exit and press the exit/use/up input.
- This touches exit entity/tile behavior and stage transition triggering.

## Worldgen / Setpiece Gaps

### Snake pit
- Needs authored room/setpiece support in map generation.
- Needs room contents, terrain interpretation, and the correct spawn behavior.
- Likely also wants the mattock and treasure relationship wired at the same time.

## Item / Tool Gaps

### Mattock
- Needs real use behavior, durability/consumption rules, and terrain breaking behavior.
- Likely wants integration with tile break hooks rather than custom mattock-only tile code.

### More items
- Teleporter and shotgun are still notable missing gameplay items.
- These are not just pickups; they need full use behavior and game-feel tuning.
- Later item work should probably continue through the tool/use split already in progress.

## Notes

Likely implementation order if we want the minimum clean path:
1. Add a reusable stage/world trigger mechanism.
2. Use that for idol disturbance, exit interaction, and altar/shop events.
3. Add shop ownership/state.
4. Add shopkeeper AI on top of shop state.
5. Add snake pit / mattock / more item behavior.
6. Do moving-attachment motion transfer last, because it touches physics most deeply.
