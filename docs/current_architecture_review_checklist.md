# Current Architecture Review Checklist

This tracks ownership and organization concerns found while reviewing the large
Classic Quest/stagegen change. Not every item is necessarily a must-fix; use
this as a triage checklist and mark items off intentionally.

## High Priority

- [x] Resolve `RoomCode` value collisions.
  - `Special8`/`Special9` currently share values with `SnakePitTop`/`SnakePitBottom`.
  - Risk: room code `8` or `9` can accidentally route through snake pit handling.
  - Reference: `src/stage_gen/classic/stagegen.cpp`

- [x] Split `src/stage_gen/classic/stagegen.cpp` by cohesive responsibility.
  - Original file owned layout graphs, room DB loading, fallback room data, glyph interpretation, shop pools, ambient spawns, branch exits, key/chest placement, and stage assembly.
  - `stagegen.cpp` now owns final generator assembly and delegates cohesive subproblems to Classic Quest modules.
  - Done: moved the Classic Quest generator under `src/stage_gen/classic/`.
  - Done: extracted room-code definitions, room-grid layout generation, layout pass dispatch, and room-code debug labels to `src/stage_gen/classic/room_layout.*`.
  - Done: extracted classic room template DB loading, weighted room selection, built-in fallback room templates, obstacle expansion, and room shop type parsing to `src/stage_gen/classic/room_templates.*`.
  - Done: extracted item-pool resolution to `src/stage_gen/classic/item_pools.*`.
  - Done: extracted stage post-passes, ambient spawn passes, branch exits, key/chest placement, and stagegen annotation helpers to `src/stage_gen/classic/stage_passes.*`.
  - Done: split stage pass implementation again into `ambient_passes.*`, `treasure_passes.*`, `stage_pass_helpers.*`, and the remaining `stage_passes.*` dispatcher.
  - Done: extracted room glyph resolution and glyph action interpretation to `src/stage_gen/classic/glyph_actions.*`.

- [x] Remove built-in room fallbacks.
  - Classic room pools no longer fall back to hardcoded templates.
  - Configured room pools must exist and contain room files or stage loading fails loudly.
  - Optional pools can stay unconfigured, but requesting a room code with no configured data now fails instead of substituting a fake room.
  - Ice Caves explicitly maps `drop` to the existing Ice side room pool until dedicated drop rooms are authored.
  - Classic item pools no longer have fallback entries; missing or exhausted requested pools now fail loudly.

## Quest And Stage Progression Ownership

- [x] Move quest route policy out of `BasicExit`.
  - `BasicExit` should own overlap/use/prompt behavior.
  - Quest flags, target resolution, `classic_win`, and legacy stage chain decisions belong in stage progression or quest routing.
  - Reference: `src/entities/basic_exit.cpp`
  - Done: `Stage` owns generated exit definitions, `BasicExit` stores only a stage-local exit id, and `stage_progression` owns exit permission checks plus transition routing.

- [x] Decouple generic stage progression from the Classic Quest generator.
  - `stage_progression.cpp` currently knows `assets/quests/classic/quest.yaml` and calls `stage_gen::classic::GenerateStage`.
  - Prefer a quest/generator registry or a smaller routing layer so progression does not directly depend on one generator.
  - Reference: `src/stage_progression.cpp`
  - Done: added `quest_stage_loader.*`; `stage_progression` applies generic transition targets and delegates quest-stage loading/Classic generator selection to that loader.

- [x] Decide whether `StageType` is still real identity or only legacy compatibility.
  - `StageType` is legacy/debug compatibility only.
  - Quest stages load through `quest_id` and `quest_stage_id`.
  - Removed the Classic Quest to `StageType` bridge and the fake `SplkMines1` debug preset.
  - Removed `theme` from stage YAML; quest stages now name concrete tile/frame data explicitly.

## Quest Data Loading

- [x] Split `src/quest.cpp`.
  - Current file contains quest parsing, stage parsing, glyph parsing, item/shop pools, and giant entity/tile name maps.
  - Likely split candidates: quest parser, stage config parser, glyph parser, pool parser, name registry/conversion.
  - Done: extracted shared YAML parse helpers to `src/quest_parse_utils.*`.
  - Done: extracted stage config parsing to `src/quest_stage_config.cpp`.
  - Done: extracted glyph parsing to `src/quest_glyphs.cpp`.
  - Done: extracted item/shop pool parsing to `src/quest_pools.cpp`.
  - Remaining giant name maps are tracked by the next checkbox.

- [x] Replace giant hardcoded entity/tile name chains with a cleaner registry.
  - Entity and tile content names should ideally come from archetype/source data tables.
  - This avoids duplicated content identity across quest parsing and archetype registration.
  - Done: added content-name resolution in `src/content_names.*`.
  - Done: quest YAML parsing now resolves entity names through registered entity archetype names.
  - Done: quest YAML parsing now resolves tile names through registered tile archetype names.
  - Done: removed `EntityTypeFromQuestName`, `TileFromQuestName`, and `QuestEntityName` from `src/quest.cpp`.

- [x] Audit `FakeBones` identity.
  - `Skeleton` is the dormant ambush skeleton identity.
  - `Bones` plus `Skull` is the inert debris identity.
  - Done: deleted `EntityType::FakeBones` and its duplicate archetype/step logic.
  - Done: Classic glyph data now uses `skeleton` for dormant ambush skeletons.

## Entity And Content Ownership

- [x] Review `UdjatEye` quest flag mutation in `chest.cpp`.
  - Current behavior directly sets Classic Quest state from item contact.
  - This is acceptable short-term, but a quest notification API would keep content rules less scattered.
  - Accepted for now: quests are not runtime-changeable yet, and Classic Quest completion is higher priority than adding a quest-event abstraction.
  - Revisit if more unrelated entity files start mutating quest flags directly, or when quests become data/script/plugin driven.

- [x] Replace the classic placeholder entity bucket with real archetype stubs.
  - Deleted `classic_placeholders.cpp/.hpp`.
  - Missing Classic entities now have explicit entity files and archetype table entries.
  - Unimplemented behavior is marked in the owning entity file instead of hidden behind a shared placeholder registry.

## Stage Data Shape

- [x] Confirm level/room metadata is stagegen-only.
  - Room metadata should not become required by gameplay systems.
  - Runtime gameplay should operate on tile/entity arrays and explicit metadata, not assume room grids.
  - Confirmed current uses are Classic stagegen helpers, debug room overlay, replay serialization, and legacy/debug stage construction.
  - No current gameplay system requires room-grid metadata.

- [x] Keep wrap/stage bounds logic tile-based.
  - Padding and wrap behavior should be expressed in tiles, not room dimensions.
  - This supports stages that are not room-grid based.
  - Confirmed toroidal wrap expansion/collapse uses tile dimensions and `padding_tiles`.
  - Remaining `Stage::kRoomShape` references are legacy/debug generation or camera margin setup, not wrap/bounds mechanics.

## Things That Look Acceptable For Now

- [x] `Stage` carrying quest metadata appears reasonable.
  - `quest_id`, `quest_stage_id`, `route_label`, exit targets, and stagegen annotations are stage metadata.
  - Risk only appears if low-level engine systems start interpreting quest-specific policy.
  - Accepted: current metadata is explicit stage context, not low-level engine policy.

- [x] Unimplemented archetype stubs are a reasonable interim strategy.
  - Missing classic content exists as explicit entity-owned archetype stubs while behavior is filled in later.
