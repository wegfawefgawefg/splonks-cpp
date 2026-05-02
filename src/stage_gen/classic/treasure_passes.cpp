#include "stage_gen/classic/treasure_passes.hpp"

#include "frame_data_id.hpp"
#include "stage_gen/classic/stage_pass_helpers.hpp"
#include "stage_gen/classic/stage_passes.hpp"
#include "stage_gen/classic/tile_palette.hpp"
#include "utils.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

namespace splonks::stage_gen::classic {

void ConvertExitTilesToBasicExitSpawns(Stage& stage) {
    for (unsigned int y = 0; y < stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < stage.GetTileWidth(); ++x) {
            if (stage.GetTile(x, y) != Tile::Exit) {
                continue;
            }

            const Vec2 exit_pos =
                Vec2::New(static_cast<float>(x * kTileSize), static_cast<float>(y * kTileSize));
            if (!HasSpawnAtWorldPos(stage, exit_pos)) {
                stage.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = EntityType::BasicExit,
                    .pos = exit_pos,
                    .animation_id = frame_data_ids::Exit,
                    .exit_id = "default",
                });
            }
            stage.SetTile(IVec2::New(static_cast<int>(x), static_cast<int>(y)), Tile::Air);
        }
    }
}

bool IsValidTreasureFloorTile(const Stage& stage, int tile_x, int tile_y) {
    if (!stage.IsTileCoordInside(tile_x, tile_y) || tile_y <= 0) {
        return false;
    }
    if (stage.GetTile(static_cast<unsigned int>(tile_x), static_cast<unsigned int>(tile_y)) !=
        Tile::Air) {
        return false;
    }
    if (!IsCollidableTileAt(stage, tile_x, tile_y + 1)) {
        return false;
    }

    const Tile tile_above =
        stage.GetTile(static_cast<unsigned int>(tile_x), static_cast<unsigned int>(tile_y - 1));
    return tile_above != Tile::Spikes && tile_above != Tile::Entrance;
}

std::optional<Vec2> FindBranchExitSpawnPos(const Stage& stage) {
    const std::optional<Vec2> entrance_pos = FindEntrancePos(stage);
    const std::optional<Vec2> default_exit_pos = FindExitPos(stage);
    std::vector<Vec2> candidates;

    for (int tile_y = 1; tile_y < static_cast<int>(stage.GetTileHeight()) - 1; ++tile_y) {
        for (int tile_x = 0; tile_x < static_cast<int>(stage.GetTileWidth()); ++tile_x) {
            if (!IsValidTreasureFloorTile(stage, tile_x, tile_y)) {
                continue;
            }
            if (IsStartRoomAt(stage, tile_x, tile_y) || IsShopRoomAt(stage, tile_x, tile_y)) {
                continue;
            }

            const Vec2 pos = Vec2::New(static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                                       static_cast<float>(tile_y * static_cast<int>(kTileSize)));
            if (entrance_pos.has_value() && Length(pos - *entrance_pos) < 80.0F) {
                continue;
            }
            if (default_exit_pos.has_value() && Length(pos - *default_exit_pos) < 80.0F) {
                continue;
            }
            if (HasSpawnAtWorldPos(stage, pos)) {
                continue;
            }
            candidates.push_back(pos);
        }
    }

    if (candidates.empty()) {
        return std::nullopt;
    }
    return candidates[static_cast<std::size_t>(PickStagePassIndex(candidates.size()))];
}

void AddBranchExit(Stage& stage, const StagePassConfig& pass) {
    const std::string exit_id = GetPassString(pass, "exit_id");
    if (exit_id.empty()) {
        AddStageGenAnnotation(stage, "branch exit skipped: missing exit_id");
        return;
    }
    if (stage.FindExitId(exit_id) == kInvalidStageExitId) {
        AddStageGenAnnotation(stage, "branch exit skipped: no route target for " + exit_id);
        return;
    }
    if (HasExitSpawn(stage, exit_id)) {
        AddStageGenAnnotation(stage, "branch exit skipped: already placed " + exit_id);
        return;
    }

    const int min_level_number = pass.GetInt("min_level_number", 0);
    if (stage.quest_level_number < min_level_number) {
        AddStageGenAnnotation(stage, "branch exit skipped: level too low for " + exit_id);
        return;
    }

    const int chance_denominator = pass.GetInt("chance_denominator", 1);
    if (chance_denominator > 1 && rng::RandomIntInclusive(1, chance_denominator) != 1) {
        AddStageGenAnnotation(stage, "branch exit skipped: chance miss for " + exit_id);
        return;
    }

    const std::optional<Vec2> pos = FindBranchExitSpawnPos(stage);
    if (!pos.has_value()) {
        AddStageGenAnnotation(stage, "branch exit skipped: no candidate for " + exit_id);
        return;
    }

    stage.entity_spawns.push_back(StageEntitySpawn{
        .type_ = EntityType::BasicExit,
        .pos = *pos,
        .animation_id = frame_data_ids::Exit,
        .exit_id = exit_id,
    });
    AddStageGenAnnotation(stage, "branch exit placed: " + exit_id);
}

std::optional<Vec2> FindKeyChestSpawnPos(const Stage& stage) {
    const std::optional<Vec2> exit_pos = FindExitPos(stage);
    if (!exit_pos.has_value()) {
        return std::nullopt;
    }

    const int exit_tile_x = static_cast<int>(exit_pos->x) / static_cast<int>(kTileSize);
    const int exit_tile_y = static_cast<int>(exit_pos->y) / static_cast<int>(kTileSize);
    const std::array<IVec2, 7> candidates = {
        IVec2::New(-1, 1), IVec2::New(2, 1),  IVec2::New(0, 1), IVec2::New(-2, 1),
        IVec2::New(1, 1),  IVec2::New(-1, 0), IVec2::New(1, 0),
    };

    for (const IVec2& candidate : candidates) {
        const int tile_x = exit_tile_x + candidate.x;
        const int tile_y = exit_tile_y + candidate.y;
        if (!IsValidTreasureFloorTile(stage, tile_x, tile_y)) {
            continue;
        }

        const Vec2 spawn_pos =
            Vec2::New(static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                      static_cast<float>(tile_y * static_cast<int>(kTileSize)));
        if (!HasSpawnAtWorldPos(stage, spawn_pos)) {
            return spawn_pos;
        }
    }

    return std::nullopt;
}

bool HasSpawnAtTile(const Stage& stage, EntityType type_, int tile_x, int tile_y) {
    const Vec2 tile_pos =
        Vec2::New(static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                  static_cast<float>(tile_y * static_cast<int>(kTileSize)));
    for (const StageEntitySpawn& spawn : stage.entity_spawns) {
        if (spawn.type_ != type_) {
            continue;
        }
        if (static_cast<int>(spawn.pos.x) / static_cast<int>(kTileSize) == tile_x &&
            static_cast<int>(spawn.pos.y) / static_cast<int>(kTileSize) == tile_y) {
            return true;
        }
        if (Length(spawn.pos - tile_pos) < static_cast<float>(kTileSize)) {
            return true;
        }
    }
    return false;
}

bool HasMinesTreasureSideSupport(const Stage& stage, int tile_x, int tile_y) {
    return IsCollidableTileAt(stage, tile_x, tile_y) ||
           HasSpawnAtTile(stage, EntityType::Block, tile_x, tile_y);
}

EmbeddedTreasure MakeVisibleGoldEmbed(FrameDataId overlay_frame) {
    EmbeddedTreasure embedded_treasure;
    embedded_treasure.visibility = EmbeddedTreasureVisibility::Visible;
    embedded_treasure.overlay_frame = overlay_frame;
    embedded_treasure.break_sound = audio_asset_ids::MoneySmashed;
    embedded_treasure.drops[0] = EmbeddedTreasureDrop{
        .type_ = EntityType::GoldChunk,
        .count = 3,
    };
    return embedded_treasure;
}

EmbeddedTreasure MakeVisibleBigGoldEmbed(FrameDataId overlay_frame) {
    EmbeddedTreasure embedded_treasure = MakeVisibleGoldEmbed(overlay_frame);
    embedded_treasure.drops[1] = EmbeddedTreasureDrop{
        .type_ = EntityType::GoldNugget,
        .count = 1,
    };
    return embedded_treasure;
}

bool AddUdjatKeyChest(Stage& stage) {
    if (stage.quest_level_number < 2) {
        return false;
    }
    if (HasSpawnType(stage, EntityType::KeyChest) || HasSpawnType(stage, EntityType::ChestKey)) {
        return false;
    }

    const std::optional<Vec2> chest_pos = FindKeyChestSpawnPos(stage);
    if (!chest_pos.has_value()) {
        return false;
    }

    std::vector<std::size_t> treasure_indices;
    treasure_indices.reserve(stage.entity_spawns.size());
    for (std::size_t i = 0; i < stage.entity_spawns.size(); ++i) {
        const StageEntitySpawn& spawn = stage.entity_spawns[i];
        if (!IsTreasureSpawnType(spawn.type_)) {
            continue;
        }
        if (Length(spawn.pos - *chest_pos) < 64.0F) {
            continue;
        }
        treasure_indices.push_back(i);
    }

    if (!treasure_indices.empty()) {
        const std::size_t pick =
            treasure_indices[static_cast<std::size_t>(PickStagePassIndex(treasure_indices.size()))];
        stage.entity_spawns.push_back(StageEntitySpawn{
            .type_ = EntityType::KeyChest,
            .pos = *chest_pos,
            .exit_id = "",
        });
        stage.entity_spawns[pick].type_ = EntityType::ChestKey;
        return true;
    }

    for (int y = static_cast<int>(stage.GetTileHeight()) - 2; y >= 1; --y) {
        for (int x = 0; x < static_cast<int>(stage.GetTileWidth()); ++x) {
            if (!IsValidTreasureFloorTile(stage, x, y)) {
                continue;
            }

            const Vec2 key_pos = Vec2::New(static_cast<float>(x * static_cast<int>(kTileSize) + 8),
                                           static_cast<float>(y * static_cast<int>(kTileSize) - 4));
            if (Length(key_pos - *chest_pos) < 64.0F || HasSpawnAtWorldPos(stage, key_pos)) {
                continue;
            }

            stage.entity_spawns.push_back(StageEntitySpawn{
                .type_ = EntityType::KeyChest,
                .pos = *chest_pos,
                .exit_id = "",
            });
            stage.entity_spawns.push_back(StageEntitySpawn{
                .type_ = EntityType::ChestKey,
                .pos = key_pos,
                .exit_id = "",
            });
            return true;
        }
    }
    return false;
}

void AddMinesEmbeddedTreasure(Stage& stage, const ItemPoolDb& item_db) {
    const int stage_width = static_cast<int>(stage.GetTileWidth());
    const int stage_height = static_cast<int>(stage.GetTileHeight());

    for (int tile_y = 0; tile_y < stage_height; ++tile_y) {
        for (int tile_x = 0; tile_x < stage_width; ++tile_x) {
            const IVec2 tile_pos = IVec2::New(tile_x, tile_y);
            if (stage.GetTile(static_cast<unsigned int>(tile_x),
                              static_cast<unsigned int>(tile_y)) !=
                DirtTileForFamilyTile(stage.border.left.tile)) {
                continue;
            }

            const int visible_gold_roll = rng::RandomIntInclusive(1, 100);
            if (visible_gold_roll < 20) {
                stage.SetEmbeddedTreasure(
                    tile_pos,
                    MakeVisibleGoldEmbed(HashFrameDataIdConstexpr("embedded_gold"))
                );
                continue;
            }
            if (visible_gold_roll < 30) {
                stage.SetEmbeddedTreasure(
                    tile_pos,
                    MakeVisibleBigGoldEmbed(HashFrameDataIdConstexpr("embedded_gold_big"))
                );
                continue;
            }

            const bool interior_tile =
                tile_x > 0 && tile_x < stage_width - 1 && tile_y > 0 && tile_y < stage_height - 1;
            if (!interior_tile) {
                continue;
            }

            if (rng::RandomIntInclusive(1, 100) == 1) {
                stage.SetEmbeddedTreasure(tile_pos, EntityType::SapphireBig);
            } else if (rng::RandomIntInclusive(1, 120) == 1) {
                stage.SetEmbeddedTreasure(tile_pos, EntityType::EmeraldBig);
            } else if (rng::RandomIntInclusive(1, 140) == 1) {
                stage.SetEmbeddedTreasure(tile_pos, EntityType::RubyBig);
            } else if (rng::RandomIntInclusive(1, 1200) == 1) {
                stage.SetEmbeddedTreasure(tile_pos, PickUndergroundItemType(item_db, stage));
            }
        }
    }
}

void AddMinesTreasure(Stage& stage, int level_number) {
    const std::optional<Vec2> entrance_pos = FindEntrancePos(stage);
    const std::optional<Vec2> exit_pos = FindExitPos(stage);
    const int curr_level = std::max(1, level_number);
    const int bones_chance = 0;
    const int stage_width = static_cast<int>(stage.GetTileWidth());
    const int stage_height = static_cast<int>(stage.GetTileHeight());

    for (int tile_y = 0; tile_y < stage_height; ++tile_y) {
        for (int tile_x = 0; tile_x < stage_width; ++tile_x) {
            if (!IsCollidableTileAt(stage, tile_x, tile_y) || IsShopRoomAt(stage, tile_x, tile_y)) {
                continue;
            }

            const Vec2 tile_pos =
                Vec2::New(static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                          static_cast<float>(tile_y * static_cast<int>(kTileSize)));

            if (entrance_pos.has_value() && Length(tile_pos - *entrance_pos) < 32.0F) {
                continue;
            }
            if (exit_pos.has_value() && Length(tile_pos - *exit_pos) < 32.0F) {
                continue;
            }
            if (DistanceToNearestSpawnType(stage, EntityType::GoldIdol, tile_pos) < 64.0F) {
                continue;
            }

            if (tile_y <= 0 || IsCollidableTileAt(stage, tile_x, tile_y - 1)) {
                continue;
            }

            const Tile tile_above = stage.GetTile(static_cast<unsigned int>(tile_x),
                                                  static_cast<unsigned int>(tile_y - 1));
            if (tile_above == Tile::Spikes || tile_above == Tile::Entrance) {
                continue;
            }

            const Vec2 item_pos = tile_pos + Vec2::New(8.0F, -4.0F);
            const Vec2 stack_pos = tile_pos + Vec2::New(8.0F, -8.0F);
            const Vec2 chest_pos = tile_pos;
            const Vec2 box_pos = tile_pos + Vec2::New(2.0F, -12.0F);
            const Vec2 web_pos = tile_pos + Vec2::New(0.0F, -16.0F);
            const Vec2 bones_pos = tile_pos + Vec2::New(0.0F, -16.0F);
            const Vec2 skull_pos = tile_pos + Vec2::New(12.0F, -4.0F);
            if (HasSpawnAtWorldPos(stage, item_pos) || HasSpawnAtWorldPos(stage, stack_pos) ||
                HasSpawnAtWorldPos(stage, chest_pos) || HasSpawnAtWorldPos(stage, box_pos) ||
                HasSpawnAtWorldPos(stage, bones_pos) || HasSpawnAtWorldPos(stage, skull_pos)) {
                continue;
            }

            if (rng::RandomIntInclusive(1, 100) == 1) {
                AddAmbientSpawn(stage, EntityType::Rock, item_pos);
                continue;
            }
            if (rng::RandomIntInclusive(1, 40) == 1) {
                AddAmbientSpawn(stage, EntityType::Pot, stack_pos);
                continue;
            }

            const bool ceiling_above = tile_y >= 2 && IsCollidableTileAt(stage, tile_x, tile_y - 2);
            const bool left_support = HasMinesTreasureSideSupport(stage, tile_x - 1, tile_y - 1);
            const bool right_support = HasMinesTreasureSideSupport(stage, tile_x + 1, tile_y - 1);
            const bool side_support = left_support || right_support;
            const bool tunnel_support = left_support && right_support;

            if (ceiling_above && side_support) {
                const int web_denominator =
                    DistanceToNearestSpawnType(stage, EntityType::GiantSpiderHang, tile_pos) < 100.0F
                        ? 5
                        : 60;
                if (rng::RandomIntInclusive(1, web_denominator) == 1) {
                    AddAmbientSpawn(stage, EntityType::Cobweb, web_pos);
                } else if (rng::RandomIntInclusive(1, 10) == 1) {
                    AddAmbientSpawn(stage, EntityType::Box, box_pos);
                } else if (rng::RandomIntInclusive(1, 15) == 1) {
                    AddAmbientSpawn(stage, EntityType::Chest, chest_pos);
                } else if (!HasSpawnType(stage, EntityType::Damsel) &&
                           rng::RandomIntInclusive(1, 8) == 1) {
                    AddAmbientSpawn(stage, EntityType::Damsel, stack_pos);
                } else if (rng::RandomIntInclusive(1, std::max(1, 40 - 2 * curr_level)) <=
                           1 + bones_chance) {
                    if (rng::RandomIntInclusive(1, 8) == 1) {
                        AddAmbientSpawn(stage, EntityType::Skeleton, bones_pos);
                    } else {
                        AddAmbientSpawn(stage, EntityType::Bones, bones_pos);
                        AddAmbientSpawn(stage, EntityType::Skull, skull_pos);
                    }
                } else if (rng::RandomIntInclusive(1, 3) == 1) {
                    AddAmbientSpawn(stage, EntityType::Gold, item_pos);
                } else if (rng::RandomIntInclusive(1, 6) == 1) {
                    AddAmbientSpawn(stage, EntityType::GoldStack, stack_pos);
                } else if (rng::RandomIntInclusive(1, 6) == 1) {
                    AddAmbientSpawn(stage, EntityType::EmeraldBig, item_pos);
                } else if (rng::RandomIntInclusive(1, 8) == 1) {
                    AddAmbientSpawn(stage, EntityType::SapphireBig, item_pos);
                } else if (rng::RandomIntInclusive(1, 10) == 1) {
                    AddAmbientSpawn(stage, EntityType::RubyBig, item_pos);
                }
                continue;
            }

            if (tunnel_support) {
                const int web_denominator =
                    DistanceToNearestSpawnType(stage, EntityType::GiantSpiderHang, tile_pos) < 100.0F
                        ? 10
                        : 60;
                if (rng::RandomIntInclusive(1, web_denominator) == 1) {
                    AddAmbientSpawn(stage, EntityType::Cobweb, web_pos);
                } else if (rng::RandomIntInclusive(1, 4) == 1) {
                    AddAmbientSpawn(stage, EntityType::Gold, item_pos);
                } else if (rng::RandomIntInclusive(1, std::max(1, 80 - curr_level)) <=
                           1 + bones_chance) {
                    if (rng::RandomIntInclusive(1, 8) == 1) {
                        AddAmbientSpawn(stage, EntityType::Skeleton, bones_pos);
                    } else {
                        AddAmbientSpawn(stage, EntityType::Bones, bones_pos);
                        AddAmbientSpawn(stage, EntityType::Skull, skull_pos);
                    }
                } else if (rng::RandomIntInclusive(1, 8) == 1) {
                    AddAmbientSpawn(stage, EntityType::GoldStack, stack_pos);
                } else if (rng::RandomIntInclusive(1, 8) == 1) {
                    AddAmbientSpawn(stage, EntityType::EmeraldBig, item_pos);
                } else if (rng::RandomIntInclusive(1, 9) == 1) {
                    AddAmbientSpawn(stage, EntityType::SapphireBig, item_pos);
                } else if (rng::RandomIntInclusive(1, 10) == 1) {
                    AddAmbientSpawn(stage, EntityType::RubyBig, item_pos);
                }
                continue;
            }

            if (rng::RandomIntInclusive(1, 40) == 1) {
                AddAmbientSpawn(stage, EntityType::Gold, item_pos);
            } else if (rng::RandomIntInclusive(1, 50) == 1) {
                AddAmbientSpawn(stage, EntityType::GoldStack, stack_pos);
            } else if (rng::RandomIntInclusive(1, std::max(1, 140 - 2 * curr_level)) <=
                       1 + bones_chance) {
                if (rng::RandomIntInclusive(1, 8) == 1) {
                    AddAmbientSpawn(stage, EntityType::Skeleton, bones_pos);
                } else {
                    AddAmbientSpawn(stage, EntityType::Bones, bones_pos);
                    AddAmbientSpawn(stage, EntityType::Skull, skull_pos);
                }
            }
        }
    }
}

void ConvertBlocksToArrowTraps(Stage& stage, int chance_denominator) {
    const std::optional<Vec2> entrance_pos = FindEntrancePos(stage);
    chance_denominator = std::max(1, chance_denominator);

    const auto should_skip_candidate = [&](int tile_x, int tile_y, const Vec2& pos) {
        if (IsShopRoomAt(stage, tile_x, tile_y)) {
            return true;
        }
        if (rng::RandomIntInclusive(1, chance_denominator) != 1) {
            return true;
        }

        if (entrance_pos.has_value()) {
            const float dist = Length(pos - *entrance_pos);
            if (dist <= 48.0F) {
                return true;
            }
            if (static_cast<int>(pos.y) == static_cast<int>(entrance_pos->y) &&
                dist < 144.0F) {
                return true;
            }
        }
        return false;
    };

    const auto get_arrow_trap_facing = [&](int tile_x, int tile_y) -> std::optional<LeftOrRight> {
        const bool solid_right = IsCollidableTileAt(stage, tile_x + 1, tile_y);
        const bool left_open = !IsCollidableTileAt(stage, tile_x - 1, tile_y) &&
                               !IsCollidableTileAt(stage, tile_x - 2, tile_y);
        if (solid_right && left_open) {
            return LeftOrRight::Left;
        }

        const bool solid_left = IsCollidableTileAt(stage, tile_x - 1, tile_y);
        const bool right_open = !IsCollidableTileAt(stage, tile_x + 1, tile_y) &&
                                !IsCollidableTileAt(stage, tile_x + 2, tile_y);
        if (solid_left && right_open) {
            return LeftOrRight::Right;
        }
        return std::nullopt;
    };

    for (StageEntitySpawn& spawn : stage.entity_spawns) {
        if (spawn.type_ != EntityType::Block) {
            continue;
        }

        const int tile_x = static_cast<int>(spawn.pos.x) / static_cast<int>(kTileSize);
        const int tile_y = static_cast<int>(spawn.pos.y) / static_cast<int>(kTileSize);
        if (should_skip_candidate(tile_x, tile_y, spawn.pos)) {
            continue;
        }

        const std::optional<LeftOrRight> facing = get_arrow_trap_facing(tile_x, tile_y);
        if (facing.has_value()) {
            spawn.type_ = EntityType::ArrowTrap;
            spawn.facing = *facing;
        }
    }

    for (unsigned int y = 0; y < stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < stage.GetTileWidth(); ++x) {
            const int tile_x = static_cast<int>(x);
            const int tile_y = static_cast<int>(y);
            if (!IsBlockTile(stage.GetTile(x, y))) {
                continue;
            }

            const Vec2 pos =
                Vec2::New(static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                          static_cast<float>(tile_y * static_cast<int>(kTileSize)));
            if (should_skip_candidate(tile_x, tile_y, pos)) {
                continue;
            }

            const std::optional<LeftOrRight> facing = get_arrow_trap_facing(tile_x, tile_y);
            if (!facing.has_value()) {
                continue;
            }

            stage.SetTile(IVec2::New(tile_x, tile_y), Tile::Air);
            stage.entity_spawns.push_back(StageEntitySpawn{
                .type_ = EntityType::ArrowTrap,
                .pos = pos,
                .facing = *facing,
                .exit_id = "",
            });
        }
    }
}

} // namespace splonks::stage_gen::classic
