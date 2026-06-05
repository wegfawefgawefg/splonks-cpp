# FXP Quest

## Why This Exists

Splonks multiplayer is now using deterministic lockstep with rollback/resync. The
game sends inputs and topology updates, then expects every machine to simulate
the same world. That makes simulation determinism a gameplay requirement, not a
nice-to-have.

During real cross-machine playtests, a Linux/x86_64 host and macOS/arm64 peer
could produce desyncs even when the input stream and room state looked correct.
The current replay/desync capture work showed one important case:

- Kyle's Mac was the peer.
- The matching Linux host desync replay for that session was not available.
- The peer replay reproduced the host/captured-remote hash on Linux.
- Kyle's captured local hash differed in `ents`.
- The first visible differing entity was a `GoldStack`.
- `GoldStack` uses ordinary entity physics/collision rather than special
  networking or lobby behavior.

That points at cross-machine simulation drift in gameplay state, not at Gubsy
room discovery, Realnet NAT/relay negotiation, player topology, or input packet
ordering.

Splonks currently hashes exact float bits for important networked entity fields
such as position, velocity, acceleration, size, and rotation. Gameplay physics
also uses floats for integration, friction, gravity, collision movement, and
branch decisions. That combination is fragile in deterministic lockstep: tiny
floating-point differences can accumulate, cross branch thresholds, then become
real state divergence.

This quest exists to investigate and build a fixed-point path for deterministic
gameplay math.

## What We Learned

- This is not about rendering. Rendering, camera smoothing, UI, audio, particles,
  and visual-only interpolation can stay float.
- This is about gameplay state that affects simulation, branches, collision,
  entity spawning, damage, pickups, hazards, and lockstep hashes.
- Most authored gameplay decimal constants in source are low precision:
  one or two decimal places dominate, three decimal places appear, and only a
  small number of gameplay-ish constants go beyond that.
- A binary fixed-point scale such as `1 pixel = 1024 subpixels` gives about
  `0.00098` pixel precision, which is enough for the values we normally author.
- The main cost of fixed-point is migration and API discipline, not raw CPU time.
- We still need actual benchmarks before committing Splonks physics to it.

## Initial Direction

Create a small fixed-point companion library named `gfxp`.

Use the existing G&K/Gubsy companion library style as reference:

- `ginput`
- `glayout`
- `gmenu`
- `gcore`
- `gaudio`
- `gnetcode`
- `gparticles`
- `ganim`

The `glayout` repo should be cloned locally and used as the formatting/build
style reference:

```sh
cd /home/vega/Coding/GameDev
git clone https://github.com/wegfawefgawefg/glayout
```

The `gfxp` library is for investigation and reusable development first. Once it
is proven, Splonks can vendor/copy the necessary code directly. We should not
make Splonks permanently depend on a sibling checkout just to ship the game.

## Goals

- Provide a tiny, deterministic fixed-point number type for game simulation.
- Keep the API small enough to audit.
- Make hashing and serialization obvious.
- Make rounding behavior explicit.
- Preserve enough precision for Splonks authored movement/physics values.
- Benchmark against float operations and a representative physics loop.
- Keep distribution simple by allowing the proven implementation to be copied
  into Splonks or Gubsy later.

## Non-Goals

- Do not replace render/camera/UI/audio float math.
- Do not create a broad numeric framework.
- Do not integrate into Splonks physics before the isolated library is tested.
- Do not add a permanent sibling-repo dependency to Splonks for release builds.
- Do not optimize prematurely around synthetic microbenchmarks only.

## Proposed API Shape

Start minimal:

```cpp
namespace gfxp {

struct Fixed {
    int32_t raw = 0;

    static constexpr int frac_bits = 10;
    static constexpr int scale = 1 << frac_bits;
};

struct Vec2 {
    Fixed x;
    Fixed y;
};

} // namespace gfxp
```

Likely operations:

- construction from integer pixels
- deterministic construction from decimal strings
- explicit construction from float for debug/render interop only
- addition/subtraction
- multiplication with `int64_t` intermediates
- division with explicit rounding
- comparison
- floor/ceil/round/trunc to integer pixels
- clamp/min/max/abs/sign
- serialization to raw integer
- stable hash support
- vector add/sub/scale

The first candidate scale is:

```cpp
1 pixel = 1024 subpixels
```

That is binary fixed point with 10 fractional bits. It is precise enough for
roughly three decimal places while still allowing cheap shifts and enough world
range with `int32_t` raw storage. Multiplication should use `int64_t`
intermediates.

## Header-Only Question

We should consider whether `gfxp` should be header-only.

Reasons header-only may be good:

- Easy to vendor/copy into Splonks.
- Most operators are tiny and inline-friendly.
- No link/distribution burden.
- Future games can copy one include tree.

Reasons to avoid committing to header-only immediately:

- Benchmarks, tests, and parsing helpers may be cleaner with `.cpp` files.
- We may want a non-header implementation while experimenting.
- A generated single-header release can be made later if useful.

Plan: write normal clean library code first. If the core becomes naturally
header-only, keep it that way. If not, make a vendorable header-only package
or copied source package after the API settles.

## Benchmark Plan

Benchmarks must include both primitive operations and Splonks-like loops.

Primitive comparisons:

- float add/sub/mul/div
- fixed add/sub/mul/div
- double add/sub/mul/div
- optional comparison against `fpm`

Representative loops:

- `vel += acc`
- friction/drag style damping
- `pos += vel`
- floor/trunc to pixel
- per-axis movement step
- simple tile collision branch
- stable hashing of state

Benchmarks should report:

- throughput
- per-entity update cost
- compiler flags
- platform/CPU
- integer scale used
- whether the test is branch-heavy or arithmetic-heavy

Target platforms to test eventually:

- Linux x86_64 desktop
- macOS arm64
- Windows x86_64

## Determinism Tests

The first tests should prove exact behavior, not just speed.

- Arithmetic identities that should hold exactly.
- Rounding mode tests for positive and negative values.
- Decimal parsing tests for common authored constants.
- Serialization round-trip tests.
- Hash stability tests.
- Movement integration test with fixed input over thousands of ticks.
- Collision stepping test with thresholds near tile boundaries.

## Repository Setup Task List

- [x] Clone `glayout` into `/home/vega/Coding/GameDev`.
- [x] Inspect `glayout` build scripts, formatting rules, README style, license,
      and repository layout.
- [x] Create sibling repo `/home/vega/Coding/GameDev/gfxp`.
- [x] Set up the same compile style and formatting rules as `glayout`.
- [x] Add a small, dry README matching the tone of the other `g*` libraries.
- [x] Add a similar simple SVG/logo.
- [x] Add initial docs/spec describing the library goals, non-goals, scale,
      rounding rules, serialization, hashing, and Splonks usage path.
- [x] Add initial fixed-point type and vector type.
- [x] Add deterministic tests.
- [x] Add benchmark executable(s).
- [x] Run local build/tests/benchmarks.
- [x] Create the GitHub repo with `gh`.
- [x] Push the initial repository to GitHub.

## Initial gfxp Result

Created and pushed:

```text
https://github.com/wegfawefgawefg/gfxp
```

Local checkout:

```text
/home/vega/Coding/GameDev/gfxp
```

Initial commit:

```text
1fef62b Initial gfxp fixed-point library
```

Validation:

```sh
./scripts/build.sh
./scripts/bench.sh
```

Linux release benchmark from the first pass:

```text
entities: 20000, ticks: 2000
fixed scale: 1024 subpixels/pixel
float: 0.0349619s, 0.874048 ns/entity-tick
double: 0.0318365s, 0.795913 ns/entity-tick
fixed: 0.0447168s, 1.11792 ns/entity-tick

primitive ops: 67108864
float add: 0.34168 ns/op
double add: 0.345594 ns/op
fixed add: 0.0467201 ns/op
float mul: 0.508866 ns/op
double mul: 0.508267 ns/op
fixed mul: 0.874222 ns/op
float div: 1.72492 ns/op
double div: 2.23933 ns/op
fixed div: 1.30987 ns/op
```

Interpretation:

- Fixed add is extremely cheap.
- Fixed multiply is slower than float/double multiply in this benchmark because
  it uses an `int64_t` intermediate and scale divide.
- Fixed division is faster than float/double division in this benchmark.
- The representative movement loop is close enough to justify deeper testing.
  Collision/map queries are likely to dominate more than scalar arithmetic in
  Splonks.

## Splonks Integration Task List

This happens only after the isolated library is proven.

- [ ] Decide whether Splonks vendors `gfxp` directly or receives a copied subset.
- [ ] Decide whether Gubsy should also vendor it for future game/tooling use.
- [ ] Identify first Splonks gameplay fields to migrate: likely `pos`, `vel`,
      `acc`, and collision movement helpers.
- [ ] Keep render conversion explicit at the render boundary.
- [ ] Keep network hashes over raw fixed-point integers.
- [ ] Update SDRP/desync captures to include enough final local state to compare
      exact entity field differences.
- [ ] Migrate one narrow physics path first and validate local behavior.
- [ ] Validate Linux host/macOS peer determinism with the same recorded input
      session.

## Open Questions

- Is `1/1024 px` the final scale, or should we test `1/256` and `1/4096` too?
- Should decimal constants be parsed from strings for exact authoring behavior,
  or is controlled compile-time conversion enough for hardcoded constants?
- Should `Fixed` allow implicit construction from integers, or should all
  conversions be explicit?
- Should division support multiple named rounding modes?
- Should overflow checks be always-on in debug builds?
- Should the public namespace be `gfxp` or a longer `gk::fxp` style namespace?
