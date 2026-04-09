# Lighting Plan

This document captures the lighting directions discussed for `splonks-cpp`,
which ideas looked promising, which ones did not, and the order we currently
plan to implement them.

## Goals

- Add depth and atmosphere to the world.
- Preserve gameplay readability.
- Do not hide the path forward.
- Prefer solutions that are cheap, explicit, and easy to tune.

## Lighting Ideas Discussed

### 1. Gameplay-safe ambient + local accent lights

Keep a fairly bright global floor so all tiles remain readable, then add local
lights from player, torches, gold, exits, explosions, and similar emitters.

Notes:
- Good for atmosphere without making the game unreadable.
- Brighter areas are emphasized, but darkness does not truly hide tiles.

### 2. Per-tile light propagation

Each tile stores a light level. Emitters flood outward with falloff, and the
renderer uses that light map to tint tiles and entities.

Notes:
- Stable and cheap compared to full dynamic shadows.
- A stronger “real lighting” foundation than pure tint tricks.
- More valuable if the game has multiple meaningful light sources.

### 3. Tile ambient occlusion / seam darkening

Darken corners, interior seams, undersides, wall-floor joints, and tile
recesses.

Notes:
- Not really “lighting transport,” but gives a lot of depth.
- Very good value for cost.
- Keeps the world readable.

### 4. Height / face shading

Brighten top faces and darken undersides and side faces so terrain reads as
shaped volume instead of flat texture.

Notes:
- Especially good for platform readability.
- Fits Spelunky-like terrain well.
- Pairs naturally with seam darkening.

### 5. Soft spotlight centered around player

Subtly bias attention toward the player region while keeping the wider world
visible.

Notes:
- Could help with focus.
- Needs to stay broad and gentle.
- Could pair well with zoom tuning.

### 6. Colored accent lights only

Add color influence from torches, exits, explosions, gems, cursed objects, and
other world elements, without making darkness the core of the system.

Notes:
- Low gameplay risk.
- Good atmosphere.
- Works well on top of other shading.

### 7. Screen-space vignette from world lighting

Darken the screen edges in a way influenced by nearby world lighting.

Notes:
- More of a post-process feel than a world-lighting model.
- Easy to overdo.

## User Response Summary

The user’s rough preferences were:

- `1`: sounds good
- `2`: maybe okay, but concern that the game does not yet have many interesting
  light sources
- `3`: very interested
- `4`: very interested
- `5`: maybe worth trying
- `7`: not very interested

The user also referenced:

- `Octopath Traveler` as a game with great lighting
- `Vagante` as another good pixel-game reference

There was some openness to a broad player-centric reveal/mask if kept wide and
gameplay-safe, but not strong interest in aggressive obscuring darkness.

## Current Conclusion

The best first work is not “real darkness.”

The current best path is:

1. Height / face shading
2. Tile ambient occlusion / seam darkening
3. Later, consider gameplay-safe ambient + accent lights
4. Later, consider per-tile light propagation if the game gains enough useful
   light sources to justify it

This order was chosen because:

- `3` and `4` add depth immediately
- they do not require a full light system
- they preserve readability
- they are likely cheaper and easier to debug than propagated lighting

## Implementation Notes

### Phase 1: Height / Face Shading

Initial idea:

- brighten top-facing surfaces slightly
- darken undersides slightly
- darken vertical side faces a bit

Desired result:

- platforms feel more solid
- walls feel less flat
- traversal surfaces remain clear

### Phase 2: Tile AO / Seam Darkening

Initial idea:

- darken inside corners
- darken seams between clustered solids
- darken wall-floor joints
- darken tile recesses and overhang lips

Desired result:

- more depth
- better grounding
- stronger cave structure without reducing visibility

### Phase 3+: Optional Lighting Expansion

If needed later:

- add safe ambient + local accent lights
- add colored light sources
- possibly add per-tile propagation if the content starts needing it

Potential later experiment:

- a very broad, gameplay-safe player-focus light or reveal
- only if it improves mood without harming route visibility

## Current Plan

We plan to do lighting in this order:

1. Height / face shading
2. Tile ambient occlusion / seam darkening
3. Re-evaluate after seeing the result in-game
4. Only then decide whether to add accent lights or a propagated light system

