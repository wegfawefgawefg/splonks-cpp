# Stage Remake Scope

This is the target level set for the Spelunky-style engine demo. The goal is to
finish a coherent Classic/HD-inspired route before inventing new worlds.

## Source Notes

- The Classic wiki lists four main areas: Cave/Mines, Lush/Jungle, Ice, and
  Temple, plus Black Market and City of Gold bonus levels.
- Classic uses levels 1-4 for Cave/Mines, 5-8 for Lush/Jungle, 9-12 for Ice,
  13-15 for Temple, and level 16 for Olmec's Lair.
- HD adds a larger branch graph with Worm, Haunted Castle, Mothership, Hell,
  and Yama. We are not prioritizing Worm or Yama.

## Main Route

These are the levels we want as the primary remake path.

| Route | Stage Id | Area | Notes |
| --- | --- | --- | --- |
| 1-1 | mines_1 | Mines | Current `splk_mines1` work maps here. |
| 1-2 | mines_2 | Mines | Key/chest can appear here. |
| 1-3 | mines_3 | Mines | Key/chest can appear here. |
| 1-4 | mines_4 | Mines | Final mines level before Jungle. |
| 2-1 | jungle_1 | Jungle | Main jungle start. |
| 2-2 | jungle_2 | Jungle | Black Market / Haunted Castle branch candidates in HD-style flow. |
| 2-3 | jungle_3 | Jungle | Black Market / Haunted Castle branch candidates in HD-style flow. |
| 2-4 | jungle_4 | Jungle | Final jungle level. |
| 3-1 | ice_caves_1 | Ice Caves | Open abyss-style generation. |
| 3-2 | ice_caves_2 | Ice Caves | Ice enemies/events. |
| 3-3 | ice_caves_3 | Ice Caves | Ice enemies/events. |
| 3-4 | ice_caves_4 | Ice Caves | Moai/Mothership branch area if we choose to support it. |
| 4-1 | temple_1 | Temple | Anubis/Sceptre logic belongs here. |
| 4-2 | temple_2 | Temple | City of Gold entrance candidate. |
| 4-3 | temple_3 | Temple | Late temple. |
| 4-4 | olmec_lair | Olmec's Lair | Normal ending boss/demo capstone. |

## Bonus / Branch Levels

These are useful for a complete Classic/HD-inspired demo, but should not block
finishing the main route.

| Stage Id | Area | Priority | Notes |
| --- | --- | --- | --- |
| black_market | Black Market | High | Important because it is a concrete shop/system stress test. Classic and HD both have it. |
| city_of_gold | City of Gold | Medium | Special Temple variant; good test for alternate tile/theme generation. |
| haunted_castle | Haunted Castle | Medium | HD branch. Useful if we want undead/castle content, but not Classic core. |
| mothership | Mothership | Low | HD branch. Can wait until Ice Caves and UFO/alien systems are real. |
| worm | Worm | Skip for now | User explicitly does not care much about Worm. |
| hell | Hell | Low | HD true-ending route. Not needed for engine-demo capstone. |
| yama_throne | Yama's Throne | Skip for now | User explicitly does not care about Yama. |

## Generator Direction

The Classic Quest generator under `src/stage_gen/classic/` should be treated as
the working reference for the first generator. A pre-refactor copy was saved at:

`docs/stage_gen_reference/splk_mines_2026_04_23_reference.cpp`

Near-term refactor target:

1. Keep stage algorithms in C++.
2. Add a `StageDefinition` table keyed by stable stage id/name.
3. Move room templates, glyph maps, spawn tables, shop tables, event odds, and
   pass knobs into hotloaded data files.
4. Add reload-data and reroll-current-stage debug buttons.
5. Add persistent stagegen annotations so generated rooms explain themselves.

## First Implementation Priority

1. Preserve and refactor the current Mines generator instead of replacing it.
2. Expand Mines from the current Classic Quest Mines work to Mines 1-4.
3. Build Jungle 1-4 using the same definition/pass architecture.
4. Build Ice Caves 1-4.
5. Build Temple 1-3.
6. Build Olmec's Lair.
7. Add Black Market once shops are stable enough.
8. Add City of Gold after Temple is stable.

## Cut / Deprioritize

The old generic `cave` and `test` stage generators are not strategic content.
Debug stages can stay as hand-authored test fixtures, but production generation
should focus on the stage list above.
