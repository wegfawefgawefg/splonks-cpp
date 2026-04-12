# Spelunky Physics Research

## Purpose

This note records what the current `splonks-cpp` player physics are, what the
local `SpelunkyClassicHD` source says, and what that implies for later tuning.

The immediate conclusion is simple:

- our current physics are inherited from `splonks-rs`
- they are playable and internally consistent
- they are not a literal match for Spelunky Classic / HD
- the biggest jump-model difference is that we currently use a single jump
  impulse, while Spelunky uses a variable-jump model

This is a research note, not a migration plan. Physics can stay as-is for now.

## Current `splonks-cpp` Numbers

Current player constants:

- `src/entities/player.hpp`
- `kMoveAcc = 0.5F`
- `kRunAcc = 0.5F`
- `kClimbSpeed = 3.0F`
- `kMaxWalkSpeed = 2.5F`
- `kMaxRunSpeed = 4.0F`
- `kMaxSpeed = 9.0F`
- `kJumpImpulse = 4.5F`
- `kCoyoteTimeFrames = 6`
- `kJumpDelayFrames = 1`
- player size `10 x 10`

Current stage gravity:

- `src/stage.hpp`
- `gravity = 0.3F`

Current jump behavior:

- `src/entities/common_hang.cpp`
- grounded or coyote jump sets `entity.vel.y = -player::kJumpImpulse`
- there is no held-jump timer or variable gravity ramp

Current horizontal behavior:

- `src/controls.cpp`
- left/right directly set `acc.x` to `+/-kMoveAcc` or `+/-kRunAcc`
- `src/entities/player.cpp`
- velocity is clamped to walk or run cap
- friction is a simple multiply by `0.85F` when not actively controlled

## Provenance

These values match the current `splonks-rs` era physics that `splonks-cpp`
inherited. The current C++ build has diverged in collision/contact details, but
the basic player movement numbers are still from that line.

That means the present feel is "our Rust feel", not "measured Spelunky feel".

## Local Classic Source Findings

The local comparison source is:

- `/home/vega/Coding/GameDev/Splonks/SpelunkyClassicHD`

Important caveat:

- this is Spelunky Classic source / ClassicHD work
- it is not the commercial Spelunky HD remake source
- it is still the best hard local source we have for real Spelunky-style
  movement semantics

Relevant files:

- `/home/vega/Coding/GameDev/Splonks/SpelunkyClassicHD/scripts/characterCreateEvent/characterCreateEvent.gml`
- `/home/vega/Coding/GameDev/Splonks/SpelunkyClassicHD/scripts/characterStepEvent/characterStepEvent.gml`
- `/home/vega/Coding/GameDev/Splonks/SpelunkyClassicHD/objects/oPlayer1/Step_0.gml`

Key Classic values from `characterCreateEvent.gml`:

- `grav = 1`
- `gravNorm = 1`
- `xVelLimit = 16`
- `yVelLimit = 10`
- `xAccLimit = 9`
- `yAccLimit = 6`
- `runAcc = 3`
- `initialJumpAcc = -2`
- `jumpTimeTotal = 10`
- `climbAcc = 0.6`
- `departLadderXVel = 4`
- `departLadderYVel = -4`
- `frictionRunningX = 0.6`
- `frictionRunningFastX = 0.98`
- `frictionClimbingX = 0.6`
- `frictionClimbingY = 0.6`
- `frictionDuckingX = 0.8`
- `frictionFlyingX = 0.99`
- collision bounds are set with `setCollisionBounds(-5, -8, 5, 8)`

Important Classic movement semantics from `characterStepEvent.gml`:

- airborne movement applies `yAcc += gravityIntensity`
- stepping off a ledge immediately adds `yAcc += grav`
- grounded jump uses `yAcc += initialJumpAcc * 2`
- jump hold is variable:
  - `jumpTime` starts at `0`
  - `jumpTimeTotal = 10`
  - while jump is held, gravity is scaled by `jumpTime / jumpTimeTotal`
  - releasing jump early cuts the variable-jump window short
- run behavior is stateful, not just a second accel value:
  - sustained run can increase speed limits
  - friction changes by locomotion mode

Important Classic fall / stun behavior from `oPlayer1/Step_0.gml`:

- `fallTimer` increments while falling
- long falls trigger stun / damage on landing
- dead or stunned bodies use different bounce / friction behavior
- spikes kill only on meaningful downward impact, not merely because the player
  touched them from above at low speed

## What This Means For Jumping

Our current jump is a single-impulse jump:

- press jump
- set `vel.y = -4.5`
- normal gravity resumes immediately

Spelunky Classic is not doing that.

It does have an initial jump impulse, but jump height is not just that impulse.
The jump button also modifies gravity for a short hold window. That means the
actual shape of the arc depends on button hold duration.

So the answer to the design question is:

- no, our current single impulse is not how Spelunky works
- the closest Spelunky description is "initial impulse plus short variable jump
  sustain via gravity modulation"

That is probably the single biggest reason our jump feel is only
Spelunky-adjacent right now.

## Online HD Findings

There does not appear to be a trustworthy public table of exact commercial
Spelunky HD movement constants.

What is publicly easy to verify is behavior:

- Spelunky HD normal jump is generally described as about `2` tiles
- Spelunky HD Spring Shoes add about `1` tile to jump height
- Spelunky HD ropes reach about `8` tiles
- Spelunky HD fall damage thresholds are commonly described as:
  - `8-17` tiles: `1` damage
  - `18-27` tiles: `2` damage
  - `28+` tiles: death

These are useful for validation targets, but they do not give us the actual
internal constants.

## Practical Conclusion

If we want literal Spelunky-like movement later, the likely order is:

1. keep current physics for now
2. change jump from pure single-impulse to variable jump
3. tune horizontal friction / run behavior
4. tune fall timer and landing damage separately
5. validate against measured tile outcomes, not guessed constants

The important part is that changing only raw gravity or only `kJumpImpulse`
would not be enough. Spelunky movement is a small system, not one number.
