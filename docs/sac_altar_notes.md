# Sac Altar Notes

This note captures three things:

1. the current `splonks-cpp` implementation
2. the Spelunky HD behavior we checked
3. the recommended adaptation for this project

## Current Splonks Implementation

Current code shape lives mainly in:

- `src/entities/sac_altar.cpp`
- `src/entities/sac_altar.hpp`
- `src/state.hpp`

### Favor state

- favor is run-level state, not per-altar state
- `State::sac_altar_favor` stores the accumulated favor total
- `State::sac_altar_reward_tier` stores which reward tiers have already been granted

### Current reward thresholds

- `8 favor`: accessory-style reward via `PickAccessoryReward(...)`
- `16 favor`: currently a second item reward
  - if the reward target has no back item and does not already own a jetpack, this is a `JetPack`
  - otherwise it falls back to `BombBox`
- `32 favor`: `+8` health
- after that there are currently no more rewards

### Current sacrifice values

The implementation is intentionally simplified and not yet an exact HD table.

Current behavior is approximately:

- dead generic valid victim: `1`
- stunned/living generic valid victim: `2`
- dead shopkeeper or hawk man: `3`
- stunned/living shopkeeper or hawk man: `6`
- dead damsel / hired hand / spelunker / black knight: `4`
- stunned/living damsel / hired hand / spelunker / black knight: `8`
- gold idol has a special higher value
- bombs and several non-sacrificable entities are excluded

### Current knife interaction

- the sacrifice knife can bank favor on kills
- held kills bank the full living sacrifice value
- corpse carving banks `1`
- thrown kills bank `1`
- grounded knife on a sac altar deposits the banked favor

This is a deliberate gameplay deviation already.

## Spelunky HD Behavior

These notes are based on the Spelunky HD Kali altar reference and a supporting community summary.

### HD reward thresholds

- `8 favor`: random accessory reward
- `16 favor`: `Kapala`
- `32 favor`: invigoration / `+8 HP`
- after invigoration, no further reward tiers are granted

### HD sacrifice values

The HD values we checked are:

- dead common enemy: `1`
- stunned common enemy: `2`
- dead shopkeeper or hawk man: `3`
- stunned shopkeeper or hawk man: `6`
- dead damsel / hired hand / spelunker / black knight: `4`
- stunned damsel / hired hand / spelunker / black knight: `8`

That means the broad structure of the current Splonks table is already close, but the reward content is not.

### Important HD conclusion

The common intuition that Kali should keep paying out forever is not how HD works. In HD, the reward ladder effectively ends after the `32 favor` invigoration reward.

## Recommended Adaptation

The best fit for this project is:

### Keep

- run-level favor
- run-level reward progression
- the existing favor table shape
- the `32 favor` cap on progression

Those choices are simple, legible, and already very close to HD.

### Change

- replace the current `16 favor` reward with `Kapala` once Kapala exists
- keep `8 favor` as the accessory reward tier
- keep `32 favor` as `+8 HP`
- remove the current JetPack / BombBox second-tier behavior once Kapala is implemented

### Explicit project stance

Recommended final ladder:

- `8 favor`: accessory reward
- `16 favor`: `Kapala`
- `32 favor`: `+8 HP`
- `>32 favor`: no further tier rewards

### Knife adaptation

The sacrifice knife banking mechanic is a good project-specific addition and does not need to match HD exactly.

Recommended stance:

- keep knife banking and altar cash-in
- keep corpse carving as a small favor source
- treat this as a deliberate Splonks extension, not a fidelity feature

### Why this is the right compromise

- it preserves the clean HD altar arc
- it avoids designing an infinite reward ladder just to justify extra favor
- it keeps high-favor sacrifices meaningful through favor count and punishment systems without requiring endless gifts
- it leaves room for project-specific mechanics like the sacrifice knife without muddying the core altar progression

## Suggested Follow-up

When altar work resumes, the main cleanup pass should be:

1. implement `Kapala`
2. swap the `16 favor` reward to `Kapala`
3. re-check the sacrifice value exclusions list
4. decide whether `GoldIdol` value should stay custom or match HD exactly

## References

- Spelunky HD Kali altar reference: https://spelunky.fandom.com/wiki/Kali_Altar_%28HD%29
- Supporting summary: https://gaming.stackexchange.com/questions/76277/what-are-the-altars-to-kali-for
