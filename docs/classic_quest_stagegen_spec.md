# Classic Quest Stagegen Spec

This spec defines the first production stage generation architecture for the
Classic Quest. The goal is to reproduce the Spelunky Classic route while keeping
room templates, tables, weights, and pass knobs hotloadable.

The design deliberately does not include DLL/plugin stagegen or scripted ent
behavior. C++ owns algorithms. Data owns content and tuning.

## Goals

- Support a `classic` quest with a route graph and quest-specific runtime state.
- Use one stage definition per playable stage id.
- Reuse C++ generator functions across many stages.
- Hotload room templates, glyph maps, shop tables, spawn tables, event chances,
  and pass config.
- Provide a debug workflow: reload stage data, reroll current stage, inspect
  stagegen annotations.
- Preserve the current mines generator behavior while refactoring it into data
  and named passes.

## Non-Goals

- No DLL hotloading for stagegen in the first version.
- No custom ent behavior callbacks from data.
- No generic script VM yet.
- No attempt to express every algorithm as YAML.

## File Layout

```text
assets/quests/classic/
  quest.yaml
  stages/
    mines.yaml
    jungle.yaml
    ice_caves.yaml
    temple.yaml
    black_market.yaml
    city_of_gold.yaml
    olmec_lair.yaml
  glyphs/
    mines.yaml
    jungle.yaml
    ice_caves.yaml
    temple.yaml
  pools/
    items.yaml
    shops.yaml
    enemies.yaml
    treasure.yaml
  rooms/
    mines/
      start/
        start_00.room.yaml
      main/
        main_00.room.yaml
      side/
        side_00.room.yaml
      drop/
        drop_00.room.yaml
      exit/
        exit_00.room.yaml
      shop_left/
        shop_left_00.room.yaml
      shop_right/
        shop_right_00.room.yaml
      snake_pit_top/
        snake_pit_top_00.room.yaml
      snake_pit_bottom/
        snake_pit_bottom_00.room.yaml
    jungle/
      ...
```

## Quest Definition

`assets/quests/classic/quest.yaml`

```yaml
id: classic
title: Classic Quest
start_stage: classic_mines_1
quest_state: classic

stages:
  - id: classic_mines_1
    route_label: 1-1
    stage_file: stages/mines.yaml
    level_number: 1
    exits:
      default: classic_mines_2

  - id: classic_mines_2
    route_label: 1-2
    stage_file: stages/mines.yaml
    level_number: 2
    exits:
      default: classic_mines_3

  - id: classic_mines_3
    route_label: 1-3
    stage_file: stages/mines.yaml
    level_number: 3
    exits:
      default: classic_mines_4

  - id: classic_mines_4
    route_label: 1-4
    stage_file: stages/mines.yaml
    level_number: 4
    exits:
      default: classic_jungle_1

  - id: classic_jungle_1
    route_label: 2-1
    stage_file: stages/jungle.yaml
    level_number: 5
    exits:
      default: classic_jungle_2

  - id: classic_jungle_2
    route_label: 2-2
    stage_file: stages/jungle.yaml
    level_number: 6
    exits:
      default: classic_jungle_3
      black_market:
        target: classic_black_market
        requires:
          - flag: has_udjat_eye
          - flag_not: made_black_market
      haunted_castle:
        target: classic_haunted_castle

  - id: classic_black_market
    route_label: BM
    stage_file: stages/black_market.yaml
    level_number: 0
    exits:
      default: classic_jungle_3
```

### Exit Rules

- Each generated exit ent has an `exit_id`.
- Runtime progression uses `current_stage.exits[exit_id]`.
- Simple main exits are produced by room glyphs.
- Special branch exits can be placed by stage passes.

## Quest State

Classic quest state should be typed C++ state, not arbitrary string KV.

```cpp
struct ClassicQuestState {
    bool made_black_market = false;
    bool has_udjat_eye = false;
    bool made_moai = false;
    bool has_hedjet = false;
    bool has_sceptre = false;
    bool has_book_of_dead = false;
};

struct QuestState {
    QuestId quest_id = QuestId::None;
    ClassicQuestState classic;
};
```

YAML may reference flags by name, but names are resolved and validated at load
time. Gameplay code should use typed fields.

## Stage Definition

`assets/quests/classic/stages/mines.yaml`

```yaml
id: mines
title: Mines
theme: cave
generator: classic_room_graph
room_size: [10, 8]
layout_size: [4, 4]
glyphs: glyphs/mines.yaml
border_tile: cave_dirt
backwall_tiles: [cave_air0, cave_air1, cave_air2]

room_pools:
  start: rooms/mines/start
  main: rooms/mines/main
  side: rooms/mines/side
  drop: rooms/mines/drop
  exit: rooms/mines/exit
  shop_left: rooms/mines/shop_left
  shop_right: rooms/mines/shop_right
  snake_pit_top: rooms/mines/snake_pit_top
  snake_pit_bottom: rooms/mines/snake_pit_bottom

layout_passes:
  - name: snake_pit
    enabled: true
    properties:
      chance_denominator: 8

  - name: shop
    enabled: true
    properties:
      min_level_number: 2
      chance_uses_level_number: true

stage_passes:
  - name: convert_exit_tiles
    enabled: true

  - name: embedded_treasure
    enabled: true
    properties:
      visible_gold_roll_denominator: 100
      visible_gold_roll_max: 19
      big_gold_roll_max: 29

  - name: floor_treasure
    enabled: true

  - name: udjat_key_chest
    enabled: true
    properties:
      min_level_number: 2

  - name: arrow_trap_conversion
    enabled: true

  - name: ambient_mines_ents
    enabled: true
```

Mandatory/top-level fields are consumed by the generic stage system. Pass-specific
knobs live under `properties`.

## Room Template

`assets/quests/classic/rooms/mines/main/main_00.room.yaml`

```yaml
id: main_00
pool: main
weight: 1
size: [10, 8]

properties:
  notes: basic floor with one obstacle marker

grid: |
  6000060000
  0000000000
  0000000000
  0000000000
  0005000000
  0000000000
  0000000000
  1111111111
```

A room template describes local authored layout only. Multi-room events such as
snake pit are layout passes, not room hooks.

## Glyph Map

`assets/quests/classic/glyphs/mines.yaml`

```yaml
glyphs:
  "0":
    tile: air

  "1":
    action: random_brick_or_block

  "2":
    action: maybe_random_brick_or_block

  "4":
    spawn_chance:
      ent: block
      chance_denominator: 4

  "5":
    patch_pool: ground_obstacle

  "6":
    patch_pool: air_obstacle

  "8":
    patch_pool: doorway_obstacle

  "9":
    action: entrance_or_exit
    properties:
      exit_id: default

  "L":
    tile: ladder

  "P":
    tile: ladder_top

  "A":
    action: altar_pair

  "x":
    action: sac_altar

  "a":
    spawn: chest

  "I":
    action: gold_idol

  "B":
    action: giant_tiki_head

  "K":
    spawn: shopkeeper

  "k":
    action: shop_sign

  "i":
    action: shop_item_slot

  "S":
    spawn_random:
      - ent: snake
        weight: 4
      - ent: cobra
        weight: 1

  "M":
    tile: cave_dirt
    spawn: mattock

  "&":
    spawn: cobweb
```

Complex glyph behavior is implemented by named built-in C++ actions. Unknown
actions or glyphs should fail stage data reload loudly.

## Item And Shop Pools

`assets/quests/classic/pools/items.yaml`

```yaml
pools:
  underground_items:
    - ent: jetpack
      weight: 1
    - ent: cape
      weight: 1
    - ent: shotgun
      weight: 1
    - ent: mattock
      weight: 2
    - ent: teleporter
      weight: 1
    - ent: web_cannon
      weight: 1
    - ent: bomb_box
      weight: 2
    - ent: rope_pile
      weight: 4

  weapon_shop_items:
    unique: true
    fallback: bomb_bag
    entries:
      - ent: web_cannon
        weight: 1
      - ent: shotgun
        weight: 1
      - ent: pistol
        weight: 4
      - ent: machete
        weight: 4
      - ent: bow
        weight: 3
      - ent: bomb_bag
        weight: 4
```

`assets/quests/classic/pools/shops.yaml`

```yaml
shop_types:
  general:
    sign: sign_general
    item_pool: general_shop_items
    item_slots: 4

  bomb:
    sign: sign_bomb
    item_pool: bomb_shop_items
    item_slots: 4

  weapon:
    sign: sign_weapon
    item_pool: weapon_shop_items
    item_slots: 4

  rare:
    sign: sign_rare
    item_pool: rare_shop_items
    item_slots: 4

  clothing:
    sign: sign_clothing
    item_pool: clothing_shop_items
    item_slots: 4

  craps:
    sign: sign_craps
    item_pool: craps_shop_items
    item_slots: 2

  kissing:
    sign: sign_kissing
    item_pool: kissing_shop_items
    item_slots: 1
```

Rooms define where shop slots are. Pools define what can appear. The shop layout
pass decides whether a shop room exists and which side/orientation it uses.

Classic used tilesets, but this project deliberately uses explicit tile names in
stage and glyph data. A stage declares its border/backwall tiles directly, and
glyph maps reference concrete tile ids instead of an indirect tileset index.

## C++ Shape

```cpp
struct QuestDefinition {
    QuestId id;
    std::string title;
    StageId start_stage;
    std::vector<StageDefinition> stages;
};

struct StageDefinition {
    StageId id;
    QuestId quest_id;
    std::string route_label;
    std::string stage_config_path;
    int level_number = 0;
    std::unordered_map<ExitId, StageExitDefinition> exits;
};

struct StageGeneratorContext {
    const QuestDefinition& quest;
    const StageDefinition& stage_def;
    const StageConfig& stage_config;
    const QuestState& quest_state;
    StageGenAnnotations& annotations;
    Rng& rng;
};

using StageGeneratorFn = Stage (*)(StageGeneratorContext& context);
```

Main generator:

```cpp
Stage GenerateClassicRoomGraphStage(StageGeneratorContext& context) {
    StageLayout layout = GenerateClassicBaseLayout(context);

    RunLayoutPasses(context, layout);

    Stage stage = MakeEmptyStage(context.stage_config);

    ResolveAndStampRooms(context, layout, stage);

    RunStagePasses(context, stage);

    return stage;
}
```

Pass tables:

```cpp
struct LayoutPassDefinition {
    std::string_view name;
    void (*run)(StageGeneratorContext& context, StageLayout& layout, const StagePassConfig& config);
};

struct StagePassDefinition {
    std::string_view name;
    void (*run)(StageGeneratorContext& context, Stage& stage, const StagePassConfig& config);
};

const LayoutPassDefinition kClassicLayoutPasses[] = {
    {"snake_pit", RunSnakePitLayoutPass},
    {"shop", RunShopLayoutPass},
};

const StagePassDefinition kClassicStagePasses[] = {
    {"convert_exit_tiles", RunConvertExitTilesPass},
    {"branch_exit", RunBranchExitPass},
    {"embedded_treasure", RunEmbeddedTreasurePass},
    {"floor_treasure", RunFloorTreasurePass},
    {"udjat_key_chest", RunUdjatKeyChestPass},
    {"arrow_trap_conversion", RunArrowTrapConversionPass},
    {"ambient_mines_ents", RunAmbientMinesEntsPass},
};
```

## Generation Order

```text
1. Load quest/stage data.
2. Select StageDefinition by id.
3. Build base 4x4 room path.
4. Run layout passes.
5. Pick room templates from final room labels/pool overrides.
6. Resolve glyphs into tiles, ent spawns, and background stamps.
7. Stamp rooms into the Stage.
8. Run stage passes over final geometry.
9. Return Stage with persistent stagegen annotations.
```

This order mirrors Spelunky Classic HD's shape:

- `scrLevelGen` fills room codes.
- `scrRoomGen*` chooses and resolves room strings.
- `scrEntGen` runs final ent/treasure/trap sweeps.

## Stagegen Annotations

Annotations live on the generated `Stage`, not the per-frame debug annotation
list. They are rebuilt every reroll.

Examples:

```text
room (0,0): start/start_01.room.yaml
room (2,1): main/main_04.room.yaml
layout snake_pit: reserved column 3 rows 0-3
layout shop: candidate rejected at (1,2), occupied by snake_pit
stage pass ambient_mines_ents: spawned cobra at tile 14,20, roll 1/180
stage pass arrow_trap_conversion: converted block at tile 30,11 facing left
```

## Debug Workflow

Required debug controls:

- Select quest.
- Select stage id.
- Reload stage data.
- Reroll current stage.
- Toggle stagegen annotations.
- Optional: set RNG seed / reroll seed.
- CLI validation with `--check-classic-quest-stagegen`.

Hotloadable changes:

- Room files.
- Glyph maps.
- Item/enemy/treasure/shop pools.
- Pass enable flags.
- Pass properties.
- Event chances.

Requires rebuild:

- New C++ generator algorithm.
- New C++ layout pass.
- New C++ stage pass.
- New glyph action behavior.
- New ent behavior.

## Migration Plan

1. Keep the current `src/stage_gen/classic/` generator as the behavior reference.
2. Add `assets/quests/classic` files for Mines only.
3. Add loader structs for quest, stage, room templates, glyph maps, and pools.
4. Add `StageDefinition` table/runtime selection while preserving current `StageType` bridge if needed.
5. Move current hardcoded Mines room strings into `.room.yaml` files.
6. Move current glyph switch behavior into a glyph-action resolver.
7. Move current snake pit/shop code into named layout passes.
8. Move current treasure/enemy/trap/key passes into named stage passes.
9. Add reload/reroll/annotation debug UI.
10. Expand from Mines 1-3 to Mines 1-4.
11. Port Jungle, Ice Caves, Temple, Olmec using SpelunkyClassicHD scripts as reference.
12. For missing ents, add specs first with placeholder behavior, then refine behavior as assets arrive.

## Implementation Status

Current status:

- Implemented: `assets/quests/classic/quest.yaml` route definition for Mines, Jungle, Ice Caves, Temple, Black Market, Haunted Castle, City of Gold, and Olmec's Lair, with `classic_win` as the current end target.
- Implemented: stage config files for all listed Classic route stages under `assets/quests/classic/stages`.
- Implemented: explicit per-stage `border_tile` and `backwall_tiles`, replacing the old implicit tileset assumption for quest-driven stages.
- Implemented: glyph files for all listed Classic route stages under `assets/quests/classic/glyphs`; glyph ids now resolve to real tile/ent ids, though several of those ids still have placeholder behavior.
- Implemented: quest loader structs for quest definitions, stage configs, typed Classic quest state, exit requirements, and stage pass properties.
- Implemented: room template loader for `.room.yaml` pools with id, pool, weight, size, properties, and grid.
- Implemented: Mines room templates moved into `assets/quests/classic/rooms/mines`.
- Implemented: imported raw room strings from SpelunkyClassicHD `scrRoomGen2`, `scrRoomGen3`, `scrRoomGen4`, `scrRoomGenMarket`, and `scrRoomGen5` into theme room pools for Jungle, Ice Caves, Temple, Black Market, and Olmec's Lair.
- Implemented: partially imported stages now point normal room-pool labels at existing theme pools instead of unintentionally falling back to built-in Mines room strings.
- Implemented: table-backed layout pass dispatch for `snake_pit` and `shop`.
- Implemented: table-backed stage pass dispatch for `convert_exit_tiles`, `branch_exit`, `embedded_treasure`, `floor_treasure`, `udjat_key_chest`, `arrow_trap_conversion`, and `ambient_mines_ents`.
- Implemented: data-driven branch exits for Black Market, Haunted Castle, and City of Gold. The shared stage YAML pass no-ops on stages that do not declare the corresponding `exit_id`.
- Implemented: `StageGeneratorContext`-based call path for the Classic room graph generator.
- Implemented: glyph map loading/validation and named built-in glyph actions for the current room graph generator.
- Implemented: generated exit ents carry `exit_id`, and runtime `BasicExit` routing resolves `StageExitTarget` requirement data.
- Implemented: item/shop pool loading for underground items, high-end shop items, and shop slot selection.
- Implemented: persistent stagegen annotations on `Stage`, including room source labels, layout summary, branch-exit results, and stage-pass summaries.
- Implemented: Debug Level window can select/reroll Classic quest stages, optionally seed the quest RNG, increment the seed, and list/toggle stagegen annotations.
- Implemented: quest stage transitions through `StageLoadTargetKind::QuestStage` and `BasicExit` exit routing.
- Implemented: playback snapshots preserve quest state and quest-stage transition targets in memory.
- Implemented: `--check-classic-quest-stagegen` CLI command loads and generates every Classic quest stage without launching the game.
- Implemented: Udjat Eye pickup sets `ClassicQuestState::has_udjat_eye`; entering the Black Market sets `ClassicQuestState::made_black_market`.

Known limitations:

- Jungle, Ice Caves, Temple, Black Market, Haunted Castle, City of Gold, and Olmec's Lair still use the same generic room-graph algorithm. Their room pools and glyphs load, but theme-specific generation semantics are incomplete.
- Several Classic tile/ent ids exist only as placeholder specs or conservative tile definitions. Examples include water/lava/thin ice/trap blocks/ceiling traps/tomb lord/doors/ankh/yeti/alien ship/alien boss.
- Haunted Castle and City of Gold currently reuse Temple pools with different explicit tile mappings.
- Black Market and Olmec's Lair are structurally generateable, but they still need specialized layouts rather than the generic 4x4 room graph.
- Branch exits are placed and routed, but their final Classic/HD pres and discovery behavior are not complete.
- Layout generation still owns the base room-path algorithm in C++; YAML controls pass enablement/properties, not the algorithm itself.
- Disk recording serialization still stores the old compact stage subset; full quest/stagegen annotation persistence is not part of this pass.

Next recommended implementation order:

1. Replace `ambient_mines_ents` with theme-aware ambient passes or per-theme pass names.
2. Implement placeholder behaviors for the highest-impact stage blockers: water/lava/thin ice, trap blocks, ceiling traps, tomb lord, doors, ankh, yetis, and Olmec.
3. Add specialized layout generation for Black Market and Olmec's Lair.
4. Replace Haunted Castle and City of Gold pool aliases with dedicated pools when their source rooms/ents are ported.
5. Add more stagegen annotation points for special-room/event decisions as those systems become data-driven.
6. Decide whether playback recording should serialize full `Stage` quest metadata and stagegen annotations.
