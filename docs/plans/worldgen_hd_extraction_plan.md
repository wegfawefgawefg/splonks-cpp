# Spelunky HD Worldgen Extraction Plan

## Goal

Use the Spelunky HD room-path and room-realization logic as source material for
our stage generation, but convert it into a boring intermediate format that our
C++ runtime can own cleanly.

The first target is not "port every GML script exactly."
The first target is:

- extract HD room path concepts
- extract HD room tile templates
- feed those into our own stage builder

That should let us get much closer to HD room layouts without dragging GML
control flow into runtime code.

## What We Have Now

Our current generator is simple:

- [stage.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage.cpp)
  - picks a 4x4 path
  - stores a tiny `RoomType` enum into `Stage::rooms`
  - directly calls `GenRoom(room_type, stage_type)`
- [room.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/room.cpp)
  - resolves a `RoomType` straight into tiles
- [stage_init.cpp](/home/vega/Coding/GameDev/Splonks/splonks-cpp/src/stage_init.cpp)
  - still does broad random population after stage tiles already exist

This means we do not currently have a real intermediate worldgen model.
We have:

- path type
- immediate tile realization

That is too small to express HD-style special rooms cleanly.

## What HD Actually Does

HD splits worldgen into at least three responsibilities:

1. Room-path generation

- [scrLevelGen.gml](/home/vega/Coding/GameDev/Splonks/SpelunkyClassicHD/scripts/scrLevelGen/scrLevelGen.gml)
- fills `global.roomPath[x,y]` with numeric codes
- also sets start/end and biome/special flags

2. Biome-specific room realization

- mines: [scrRoomGen.gml](/home/vega/Coding/GameDev/Splonks/SpelunkyClassicHD/scripts/scrRoomGen/scrRoomGen.gml)
- jungle: [scrRoomGen2.gml](/home/vega/Coding/GameDev/Splonks/SpelunkyClassicHD/scripts/scrRoomGen2/scrRoomGen2.gml)
- temple: [scrRoomGen4.gml](/home/vega/Coding/GameDev/Splonks/SpelunkyClassicHD/scripts/scrRoomGen4/scrRoomGen4.gml)
- these scripts use `roomPath` codes plus biome flags to choose room strings and
  obstacle chunks

3. Entity/treasure generation

- [scrEntityGen.gml](/home/vega/Coding/GameDev/Splonks/SpelunkyClassicHD/scripts/scrEntityGen/scrEntityGen.gml)
- this is mostly a later pass over the generated level, not the same thing as
  room tile realization

That split is useful and we should keep it.

## Recommended Target Model

We should split our own worldgen into these pieces:2

1. `StageLayout`

- room grid size
- start room
- end room
- room path codes per cell
- special stage flags for this generated level

2. `RoomRecipe`

- biome
- room path code
- variant id
- base 10x8 glyph string or tile template
- optional obstacle slots / modifiers

3. `StageBuildResult`

- realized tiles
- optional room metadata
- optional marker layer for later entity placement

This is the important cut:

- layout decides "what kind of room goes here"
- room recipe decides "what raw room pattern does this cell use"
- build result decides "what tiles and markers actually end up in the stage"

## What To Extract First

First pass should be Area 1 only.

Reason:

- our current gameplay is closest to cave/mines anyway
- [scrRoomGen.gml](/home/vega/Coding/GameDev/Splonks/SpelunkyClassicHD/scripts/scrRoomGen/scrRoomGen.gml)
  gives us the most immediately useful reference
- it keeps the first extraction bounded

First extracted pieces:

1. room-path codes from `scrLevelGen`

- at least the meanings of:
  - `0` side room
  - `1` horizontal/main room
  - `2` vertical-down connector
  - `3` exit room / bottom connector style
  - `4/5` shop left/right
  - `7/8/9` special pit chains where relevant

2. mines room recipes from `scrRoomGen`

- start room variants
- end room variants
- side room variants
- main room variants
- drop room variants
- shop variants can be stubbed at first if needed

3. glyph mapping

- define our own meaning for HD glyph characters
- do not bury these ad hoc in a switch inside stage generation

## What Not To Do First

We should not do these in the first slice:

- full GML script interpreter
- exact port of every obstacle randomization branch
- exact entity spawning from room markers
- all biomes at once
- all special cases like Black Market, Moai, City of Gold, etc.

That is too much surface area for the first extraction.

## Intermediate Format Recommendation

Use a boring data-first format that C++ can consume directly.

Two reasonable options:

1. C++ static tables

- fastest to implement
- easiest to debug in current codebase
- good for initial extraction

2. External data files

- JSON or similar
- nicer for iteration later
- more loader work immediately

For the first slice, I recommend C++ static tables.

Reason:

- fewer moving parts
- easy debugger visibility
- no loader/parser work before we even know the final format

If this stabilizes, we can move the tables to external data later.

## Concrete First Implementation Slice

Build this in order:

1. Add a real room-path code enum

- separate from current `RoomType`
- name it something like `RoomPathCode`
- mirror HD concepts instead of our current tiny doorway enum

2. Add a `StageLayout` struct

- owns:
  - `start_room`
  - `end_room`
  - `room_codes[4][4]`
  - generated special flags

3. Change stage generation to:

- generate `StageLayout` first
- then build each room from `StageLayout`

4. Add mines room recipe tables

- extracted from `scrRoomGen.gml`
- one static list per room-path code bucket

5. Add a glyph-to-template/tile translation layer

- convert HD room strings into our tile/template model
- for first pass, ignore unsupported glyphs safely and explicitly

6. Keep current random entity population for now

- do not block the tilegen migration on marker-based spawns

That would already let us generate levels from HD-like room data while keeping
the rest of the game stable.

## Likely File Layout

Keep it flat and boring.

- `src/worldgen_layout.hpp`
- `src/worldgen_layout.cpp`
- `src/worldgen_room_recipe.hpp`
- `src/worldgen_room_recipe.cpp`
- `src/worldgen_hd_mines.cpp`
- `src/worldgen_hd_shared.hpp`

Or under `src/stage_gen/` if we want to keep it near existing code, but still
split by responsibility:

- layout generation
- recipe tables
- recipe realization

Do not keep growing `stage.cpp` into the whole system.

## Migration Strategy

### Phase 1

Replace:

- current `Stage::New` room typing

With:

- `StageLayout` using HD-like room-path codes

But still use our current tile and spawn pipeline where possible.

### Phase 2

Replace:

- current `GenRoom(RoomType, StageType)`

With:

- room recipe lookup by biome + path code + variant

### Phase 3

Introduce a marker layer for later room-driven spawning.

This is where we start using HD-style room glyphs for:

- entrance / exit placement
- shops
- traps
- biome specials
- maybe later enemy/item hints

### Phase 4

Add more biomes:

- jungle
- ice
- temple

## Main Risks

1. HD room glyphs are not just tiles

- many characters mean obstacles, decor, spawn hints, liquids, shops, etc.

2. HD mixes room recipe choice with mutable global flags

- altar/idol/shop/special uniqueness
- we need our own explicit generated-state struct for that

3. Our current stage population is still broad random placement

- that will temporarily clash with more authored HD-style room layouts

## Recommendation

Yes, we should work on worldgen now.

The right first step is not "port all of HD generation."
The right first step is:

- add a real layout intermediate model
- extract Area 1 room-path codes
- extract Area 1 room recipes
- keep entity population mostly as-is for the first pass

That gives us a bounded migration with real payoff and keeps the runtime code
boring enough to debug.
