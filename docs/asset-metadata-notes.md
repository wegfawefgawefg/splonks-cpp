# Asset Metadata Notes

Notes captured on March 11, 2026.

## What The Current JSON Was For

The current `png + json` asset pipeline is not a gameplay-box metadata system yet.

Right now the JSON exported alongside each sprite stores:

- per-frame atlas sample position
- frame width and height
- frame duration

In other words, it is animation atlas metadata for Aseprite exports.

In [src/sprite.rs](/home/vega/Coding/GameDev/splonks/src/sprite.rs), the loader currently reads:

- `frame.x`
- `frame.y`
- `frame.w`
- `frame.h`
- `duration`

and turns that into:

- `Frame { sample_position, duration }`
- `SpriteData { frames, size }`

It does **not** currently store:

- hitboxes
- hurtboxes
- arbitrary interaction boxes
- pivots / origins
- offsets
- named animation metadata

## Was `png + json` A Good Idea?

Yes.

For a solo/small-team art pipeline, `png + json` per sprite/animation is a good authoring format:

- easier iteration from Aseprite
- less brittle than hardcoded atlas offsets
- easier to update art without touching code
- clearer ownership of each asset

It is a better authoring pipeline than:

- one giant atlas
- magic coordinates in code
- hand-maintained frame timing

If runtime optimization ever mattered later, the game could still:

- keep `png + json` as the authoring format
- prepack those assets into runtime atlases
- rewrite sample positions during a build/startup step

That means the content pipeline and the runtime representation do not have to be the same thing.

## About Pivots, Hurtboxes, And State-Based Boxes

Aseprite not exporting pivot/origin/hurtbox data directly is annoying, but it is not a blocker.

Nothing prevents extending the JSON with custom metadata such as:

- pivot/origin
- hurtbox
- hitboxes
- interaction box
- per-animation tags

Example shape:

```json
{
  "frames": {
    "walk_0": {
      "frame": { "x": 0, "y": 0, "w": 16, "h": 16 },
      "duration": 100,
      "pivot": { "x": 8, "y": 14 },
      "hurtbox": { "x": 3, "y": 4, "w": 10, "h": 12 },
      "hitboxes": [],
      "interaction_box": { "x": 2, "y": 2, "w": 12, "h": 13 }
    }
  }
}
```

## Is It Normal For Boxes To Change With Animation Or State?

Yes. Very normal.

Games often change collision or interaction boxes based on:

- animation frame
- crouch / squat
- attack phase
- windup / active / recovery
- stance/state
- facing
- equipment

Common pattern:

- one stable physics/body box for world movement
- separate hurtboxes/hitboxes that vary by animation or state
- optional interaction boxes for grab/use/pickup ranges

That means a character or enemy can:

- keep a simple movement body for navigation
- still have different combat or interaction boxes when crouching, attacking, or changing shape

This is standard in games. It is not weird.

## Practical Guidance For Splonks

If this pipeline is revived later, a reasonable next step would be:

1. keep `png + json` as the source-of-truth authoring format
2. extend the JSON with optional custom metadata
3. separate:
   - movement/body box
   - hurtbox
   - hitbox
   - interaction box
4. optionally add a later atlas-packing step for runtime performance

That would preserve the good pipeline decision without going back to brittle hardcoded atlas coordinates.
