# Spelunky Movement Research

## Purpose

This note collects movement constants and behavior from the sources we can
actually inspect, then compares them to current `splonks-cpp`. It is meant to
guide a future high-parity Spelunky-style controller.

The useful split:

- `ClassicHD` is local source, so those values are hard facts.
- Mossmouth `Spelunky HD` source is not available here, so exact internal
  constants remain unverified.
- `Spelunky 2` exposes useful datamined entity data through Overlunky, but its
  units are tile/world units, not pixels.

## Current Splonks

Current player constants are in `src/entities/player.hpp`:

- `kMoveAcc = 0.5`
- `kRunAcc = 0.5`
- `kClimbSpeed = 3.0`
- `kMaxWalkSpeed = 2.5`
- `kMaxRunSpeed = 4.0`
- `kMaxSpeed = 9.0`
- `kJumpImpulse = 4.5`
- `kCoyoteTimeFrames = 6`
- player nominal size: `10 x 10` px

Current stage gravity:

- `src/stage.hpp`
- `kDefaultStageGravity = 0.3`

Current behavior summary:

- Horizontal control adds acceleration, then clamps x velocity to walk/run cap.
- Vertical velocity is clamped to `+/-kMaxSpeed`.
- Player movement now reads from runtime debug tuning.
- Fall damage is timer based.

## Local ClassicHD Facts

Local source:

- `/home/vega/Coding/GameDev/Splonks/SpelunkyClassicHD`

Important files:

- `options/main/options_main.yy`
- `scripts/characterCreateEvent/characterCreateEvent.gml`
- `scripts/characterStepEvent/characterStepEvent.gml`
- `objects/oPlayer1/Create_0.gml`
- `objects/oPlayer1/Step_0.gml`

Project speed:

- `option_game_speed = 30`
- ClassicHD movement constants are authored per 30 Hz step.
- Splonks runs gameplay at 60 Hz, so direct per-step constants play twice as
  fast in real time unless converted.

Player-specific values:

- `myGrav = 0.6`
- `fallTimer = 0` initially
- player step overrides `xVelLimit = 10`
- player clamps `xVel` to `[-10, 10]`
- player clamps `yVel` to `[-yVelLimit, yVelLimit]`
- inherited `yVelLimit = 10`

Generic character defaults from `characterCreateEvent.gml`:

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
- collision bounds: `setCollisionBounds(-5, -8, 5, 8)`

ClassicHD fall damage:

- `fallTimer` increments while `yVel > 0` and not climbing.
- Parachute checks `fallTimer > 14`.
- Landing with `fallTimer > 16` causes long-drop stun/damage.
- `fallTimer > 48`: `10` damage.
- `fallTimer > 32`: `2` damage.
- otherwise over `16`: `1` damage.
- landing bounce sets `yVel = -3`.
- landing horizontal velocity is damped more strongly on normal ground than ice.

ClassicHD jump shape:

- Jump is not just one impulse.
- There is an initial impulse.
- A short held-jump window modulates gravity.
- Releasing jump early cuts that window.

This is probably the biggest difference from current Splonks.

## Mossmouth Spelunky HD

Exact source constants are not available in this repo.

Behavior targets commonly reported by players are still useful:

- normal jump is roughly `2` tiles
- Spring Shoes add roughly `1` tile
- ropes reach roughly `8` tiles
- fall damage is commonly described around:
  - about `8-17` tiles: `1` damage
  - about `18-27` tiles: `2` damage
  - about `28+` tiles: death

These are validation targets, not hard internal constants.

## Spelunky 2 Datamined Values

Source:

- Overlunky API: `https://spelunky-fyi.github.io/overlunky/`
- Overlunky entity data:
  `https://raw.githubusercontent.com/spelunky-fyi/overlunky/main/docs/game_data/entities.json`

For `ENT_TYPE_CHAR_ANA_SPELUNKY` and other normal player characters:

- `max_speed = 0.0725`
- `sprint_factor = 2.0`
- `acceleration = 0.032`
- `jump = 0.18`
- `friction = 0.015`
- `weight = 1.0`

Spelunky 2 units:

- One tile is `1.0` world unit.
- Player chars are roughly `width = 1.25`, `height = 1.25`.
- Floor/block entities are `width = 1.0`, `height = 1.0`.

Useful conversion to Splonks pixels:

- Splonks tile size is `16 px`.
- `Spelunky2 world units * 16 = Splonks px`.
- `0.0725 tiles/frame * 16 = 1.16 px/frame`.
- With `sprint_factor = 2.0`, horizontal run target is roughly
  `2.32 px/frame`.
- `0.032 tiles/frame^2 * 16 = 0.512 px/frame^2`.
- `0.18 tiles/frame * 16 = 2.88 px/frame`.

Caveat:

- Overlunky `max_speed` appears to be movement database data.
- Do not assume it is terminal fall speed without measuring in game or finding
  the actual Spelunky 2 physics code path.

## Current Diff

Splonks versus ClassicHD:

- Player gravity defaults to the current Splonks feel: stage gravity `0.3`.
- Player terminal vertical speed is close but not equal: Splonks `9`,
  ClassicHD `10`.
- Horizontal run is probably too fast versus Spelunky 2 datamined player data:
  Splonks run cap `4 px/frame`; Spelunky 2 converted run target is about
  `2.32 px/frame`.
- Horizontal walk is probably also high compared to Spelunky 2's base
  `1.16 px/frame`, though ClassicHD uses different semantics.
- Splonks acceleration `0.5 px/frame^2` lines up surprisingly closely with
  Spelunky 2 converted `0.512 px/frame^2`.
- Splonks jump impulse `4.5 px/frame` is much larger than Spelunky 2's
  converted `2.88 px/frame`, but this is not an apples-to-apples comparison
  because Spelunky uses more nuanced jump physics.
- Splonks can now enable ClassicHD-style held-jump gravity modulation through
  the runtime Player Tuning debug window.

## Fall Timer Implications

ClassicHD fall damage thresholds are frame thresholds, not direct tile-distance
thresholds. Distance depends on:

- simulation rate
- gravity
- initial vertical velocity
- terminal fall speed
- whether the timer starts immediately
- whether low upward/downward states reset or pause the timer

The local ClassicHD source uses raw thresholds `16/32/48` at 30 Hz. In a 60 Hz
simulation, those are `32/64/96` frames for equivalent real-time behavior.

Directly copying ClassicHD `gravity = 0.6` into 60 Hz makes the fall much too
fast. The 60 Hz equivalent is:

- acceleration-like constants: multiply by `(30 / 60)^2 = 0.25`
- velocity-like constants: multiply by `(30 / 60) = 0.5`
- frame-count constants: multiply by `(60 / 30) = 2`

## Runtime Player Tuning

`Debug: Player Tuning` owns the current parity pass. It exposes vertical,
horizontal, run, and climb/hang knobs against the real `Player` entity rather
than a temporary alternate player type.

Validation targets:

- short tap jump height
- held jump height
- running jump distance
- first fall damage distance
- stun bounce after fall damage
- climbing attach/detach feel
- rope/ladder top behavior
- thrown/carry movement side effects

## Open Questions

- What is the true commercial Spelunky HD terminal fall speed?
- Does Spelunky 2 use `EntityDB.max_speed` for vertical fall clamp, or only
  horizontal locomotion?
- What exact tile distance should Splonks use for first fall damage if we are
  targeting feel rather than literal ClassicHD timer values?
