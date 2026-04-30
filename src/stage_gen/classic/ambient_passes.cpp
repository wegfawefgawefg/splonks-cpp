#include "stage_gen/classic/ambient_passes.hpp"

#include "stage_gen/classic/stage_pass_helpers.hpp"
#include "utils.hpp"

namespace splonks::stage_gen::classic {

bool IsDarkAmbientTile(Tile tile) {
    return tile == Tile::Dark || tile == Tile::DarkFall;
}

bool StageIsDarkLevel(const Stage& stage) {
    for (unsigned int y = 0; y < stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < stage.GetTileWidth(); ++x) {
            if (IsDarkAmbientTile(stage.GetTile(x, y))) {
                return true;
            }
        }
    }
    return false;
}

bool IsVineAmbientTile(Tile tile) {
    return tile == Tile::Vine || tile == Tile::VineTop;
}

bool IsWaterAmbientTile(Tile tile) {
    return tile == Tile::WaterSwim;
}

bool IsOpenAmbientCeilingSpot(const Stage& stage, int tile_x, int tile_y) {
    return !IsCollidableTileAt(stage, tile_x, tile_y + 1) &&
           !IsCollidableTileAt(stage, tile_x, tile_y + 2) &&
           !IsWaterAmbientTile(stage.GetTileOrBorder(tile_x, tile_y + 1)) &&
           !IsWaterAmbientTile(stage.GetTileOrBorder(tile_x, tile_y + 2));
}

bool IsOpenAmbientFloorSpot(const Stage& stage, int tile_x, int tile_y) {
    if (tile_y <= 0 || IsCollidableTileAt(stage, tile_x, tile_y - 1)) {
        return false;
    }
    return stage.GetTile(static_cast<unsigned int>(tile_x),
                         static_cast<unsigned int>(tile_y - 1)) != Tile::Spikes;
}

void AddGiantSpiderHangSpawn(Stage& stage, const Vec2& pos) {
    AddAmbientSpawn(stage, EntityType::GiantSpiderHang, pos);
    AddAmbientSpawn(stage, EntityType::Cobweb, pos);
    AddAmbientSpawn(stage, EntityType::Cobweb, pos + Vec2::New(static_cast<float>(kTileSize), 0.0F));
}

void AddAmbientMinesEntities(Stage& stage) {
    const int stage_width = static_cast<int>(stage.GetTileWidth());
    const int stage_height = static_cast<int>(stage.GetTileHeight());
    const bool dark_level = StageIsDarkLevel(stage);
    const bool gen_giant_spider = rng::RandomIntInclusive(1, 6) == 1;
    bool giant_spider_spawned = false;

    for (int tile_y = 0; tile_y < stage_height; ++tile_y) {
        for (int tile_x = 0; tile_x < stage_width; ++tile_x) {
            if (!IsCollidableTileAt(stage, tile_x, tile_y) || IsShopRoomAt(stage, tile_x, tile_y) ||
                tile_y <= 1) {
                continue;
            }
            if (IsStartRoomAt(stage, tile_x, tile_y) && IsStartRoomAt(stage, tile_x, tile_y - 1)) {
                continue;
            }

            const Vec2 tile_pos =
                Vec2::New(static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                          static_cast<float>(tile_y * static_cast<int>(kTileSize)));

            if (tile_y < stage_height - 4 && IsOpenAmbientCeilingSpot(stage, tile_x, tile_y)) {
                const Vec2 ceiling_spawn_pos =
                    tile_pos + Vec2::New(0.0F, static_cast<float>(kTileSize));
                const Vec2 ceiling_spawn_pos_2 =
                    tile_pos +
                    Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize));
                const bool open_below_right = IsOpenAmbientCeilingSpot(stage, tile_x + 1, tile_y);

                if (gen_giant_spider && !giant_spider_spawned && open_below_right &&
                    !HasSpawnAtWorldPos(stage, ceiling_spawn_pos) &&
                    !HasSpawnAtWorldPos(stage, ceiling_spawn_pos_2) &&
                    rng::RandomIntInclusive(1, 40) == 1) {
                    AddGiantSpiderHangSpawn(stage, ceiling_spawn_pos);
                    giant_spider_spawned = true;
                } else if (!HasSpawnAtWorldPos(stage, ceiling_spawn_pos) &&
                           dark_level && rng::RandomIntInclusive(1, 60) == 1) {
                    AddAmbientSpawn(stage, EntityType::Lamp, ceiling_spawn_pos);
                } else if (!HasSpawnAtWorldPos(stage, ceiling_spawn_pos) &&
                           dark_level && rng::RandomIntInclusive(1, 40) == 1) {
                    AddAmbientSpawn(stage, EntityType::Scarab, ceiling_spawn_pos);
                } else if (!HasSpawnAtWorldPos(stage, ceiling_spawn_pos) &&
                           rng::RandomIntInclusive(1, 60) == 1) {
                    AddAmbientSpawn(stage, EntityType::Bat, ceiling_spawn_pos);
                } else if (!HasSpawnAtWorldPos(stage, ceiling_spawn_pos) &&
                           rng::RandomIntInclusive(1, 80) == 1) {
                    AddAmbientSpawn(stage, EntityType::SpiderHang, ceiling_spawn_pos);
                }
            }

            if (!IsOpenAmbientFloorSpot(stage, tile_x, tile_y)) {
                continue;
            }
            const Vec2 floor_spawn_pos = tile_pos - Vec2::New(0.0F, static_cast<float>(kTileSize));
            if (rng::RandomIntInclusive(1, 60) == 1) {
                AddAmbientSpawn(stage, EntityType::Snake, floor_spawn_pos);
            } else if (rng::RandomIntInclusive(1, 800) == 1) {
                AddAmbientSpawn(stage, EntityType::Caveman, floor_spawn_pos);
            }
        }
    }
}

void AddAmbientJungleEntities(Stage& stage, bool black_market) {
    const int stage_width = static_cast<int>(stage.GetTileWidth());
    const int stage_height = static_cast<int>(stage.GetTileHeight());
    const bool dark_level = StageIsDarkLevel(stage);

    for (int tile_y = 0; tile_y < stage_height; ++tile_y) {
        for (int tile_x = 0; tile_x < stage_width; ++tile_x) {
            const Tile tile =
                stage.GetTile(static_cast<unsigned int>(tile_x), static_cast<unsigned int>(tile_y));
            const bool in_shop = IsShopRoomAt(stage, tile_x, tile_y);
            const bool in_start = IsStartRoomAt(stage, tile_x, tile_y);
            const Vec2 tile_pos =
                Vec2::New(static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                          static_cast<float>(tile_y * static_cast<int>(kTileSize)));

            if (!in_shop && IsVineAmbientTile(tile) && rng::RandomIntInclusive(1, 15) == 1) {
                AddAmbientSpawn(stage, EntityType::Monkey, tile_pos);
            }

            if (!in_shop && IsWaterAmbientTile(tile) &&
                !IsCollidableTileAt(stage, tile_x, tile_y) && rng::RandomIntInclusive(1, 30) == 1) {
                AddAmbientSpawn(stage, EntityType::Piranha, tile_pos + Vec2::New(4.0F, 4.0F));
            }

            if (!IsCollidableTileAt(stage, tile_x, tile_y) || in_shop) {
                continue;
            }
            if (in_start && IsStartRoomAt(stage, tile_x, tile_y - 1)) {
                continue;
            }

            if (tile_y < stage_height - 4 && IsOpenAmbientCeilingSpot(stage, tile_x, tile_y)) {
                const Vec2 ceiling_spawn_pos =
                    tile_pos + Vec2::New(0.0F, static_cast<float>(kTileSize));
                if (dark_level && rng::RandomIntInclusive(1, 40) == 1) {
                    AddAmbientSpawn(stage, EntityType::Scarab, ceiling_spawn_pos);
                } else if (rng::RandomIntInclusive(1, 80) == 1) {
                    AddAmbientSpawn(stage, EntityType::Bat, ceiling_spawn_pos);
                }
            }

            if (!IsOpenAmbientFloorSpot(stage, tile_x, tile_y)) {
                continue;
            }
            const Vec2 floor_spawn_pos = tile_pos - Vec2::New(0.0F, static_cast<float>(kTileSize));
            const bool in_water = IsWaterAmbientTile(stage.GetTile(
                static_cast<unsigned int>(tile_x), static_cast<unsigned int>(tile_y - 1)));
            if (!in_water) {
                const bool suppress_mantrap =
                    black_market && ((tile_y * static_cast<int>(kTileSize)) % 128 == 0);
                if (!suppress_mantrap && rng::RandomIntInclusive(1, 60) == 1) {
                    AddAmbientSpawn(stage, EntityType::Mantrap, floor_spawn_pos);
                } else if (rng::RandomIntInclusive(1, 60) == 1) {
                    AddAmbientSpawn(stage, EntityType::Caveman, floor_spawn_pos);
                } else if (rng::RandomIntInclusive(1, 120) == 1) {
                    AddAmbientSpawn(stage, EntityType::FireFrog, floor_spawn_pos);
                } else if (rng::RandomIntInclusive(1, 30) == 1) {
                    AddAmbientSpawn(stage, EntityType::Frog, floor_spawn_pos);
                }
            } else if (rng::RandomIntInclusive(1, 120) == 1) {
                AddAmbientSpawn(stage, EntityType::FireFrog, floor_spawn_pos);
            } else if (rng::RandomIntInclusive(1, 30) == 1) {
                AddAmbientSpawn(stage, EntityType::Frog, floor_spawn_pos);
            }
        }
    }
}

void AddAmbientIceEntities(Stage& stage) {
    const int stage_width = static_cast<int>(stage.GetTileWidth());
    const int stage_height = static_cast<int>(stage.GetTileHeight());
    const bool dark_level = StageIsDarkLevel(stage);
    const int ufo_denominator = 30;

    for (int tile_y = 0; tile_y < stage_height; ++tile_y) {
        for (int tile_x = 0; tile_x < stage_width; ++tile_x) {
            if (IsShopRoomAt(stage, tile_x, tile_y)) {
                continue;
            }

            const Vec2 tile_pos =
                Vec2::New(static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                          static_cast<float>(tile_y * static_cast<int>(kTileSize)));

            if (IsCollidableTileAt(stage, tile_x, tile_y)) {
                if (!IsStartRoomAt(stage, tile_x, tile_y) && tile_y < stage_height - 4 &&
                    dark_level && IsOpenAmbientCeilingSpot(stage, tile_x, tile_y) &&
                    rng::RandomIntInclusive(1, 40) == 1) {
                    const Vec2 ceiling_spawn_pos =
                        tile_pos + Vec2::New(0.0F, static_cast<float>(kTileSize));
                    AddAmbientSpawn(stage, EntityType::Scarab, ceiling_spawn_pos);
                }

                if (!IsOpenAmbientFloorSpot(stage, tile_x, tile_y) ||
                    DistanceToNearestSpawnType(stage, EntityType::BasicExit, tile_pos) <= 64.0F) {
                    continue;
                }
                const Vec2 floor_spawn_pos =
                    tile_pos - Vec2::New(0.0F, static_cast<float>(kTileSize));
                if (rng::RandomIntInclusive(1, 20) == 1) {
                    AddAmbientSpawn(stage, EntityType::Yeti, floor_spawn_pos);
                }
                continue;
            }

            if (IsStartRoomAt(stage, tile_x, tile_y) || tile_y <= 0 || tile_y >= stage_height - 1) {
                continue;
            }
            if (IsCollidableTileAt(stage, tile_x, tile_y - 1) ||
                IsCollidableTileAt(stage, tile_x, tile_y + 1)) {
                continue;
            }
            if (rng::RandomIntInclusive(1, ufo_denominator) == 1) {
                AddAmbientSpawn(stage, EntityType::Ufo,
                                tile_pos - Vec2::New(0.0F, static_cast<float>(kTileSize)));
            }
        }
    }
}

void AddAmbientTempleEntities(Stage& stage) {
    const int stage_width = static_cast<int>(stage.GetTileWidth());
    const int stage_height = static_cast<int>(stage.GetTileHeight());
    const bool dark_level = StageIsDarkLevel(stage);
    bool gen_tomb_lord = stage.quest_level_number == 13 || rng::RandomIntInclusive(1, 4) == 1;
    bool tomb_lord_spawned = HasSpawnType(stage, EntityType::TombLord);

    for (int tile_y = 0; tile_y < stage_height; ++tile_y) {
        for (int tile_x = 0; tile_x < stage_width; ++tile_x) {
            if (IsShopRoomAt(stage, tile_x, tile_y) || !IsCollidableTileAt(stage, tile_x, tile_y)) {
                continue;
            }

            const Vec2 tile_pos =
                Vec2::New(static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                          static_cast<float>(tile_y * static_cast<int>(kTileSize)));

            if (tile_y < stage_height - 4 && dark_level &&
                IsOpenAmbientCeilingSpot(stage, tile_x, tile_y) &&
                rng::RandomIntInclusive(1, 40) == 1) {
                const Vec2 ceiling_spawn_pos =
                    tile_pos + Vec2::New(0.0F, static_cast<float>(kTileSize));
                AddAmbientSpawn(stage, EntityType::Scarab, ceiling_spawn_pos);
            }

            if ((IsStartRoomAt(stage, tile_x, tile_y) &&
                 IsStartRoomAt(stage, tile_x, tile_y - 1)) ||
                !IsOpenAmbientFloorSpot(stage, tile_x, tile_y)) {
                continue;
            }

            const Vec2 floor_spawn_pos = tile_pos - Vec2::New(0.0F, static_cast<float>(kTileSize));
            if (gen_tomb_lord && !tomb_lord_spawned && tile_y >= 2 &&
                !IsCollidableTileAt(stage, tile_x + 1, tile_y - 1) &&
                !IsCollidableTileAt(stage, tile_x + 2, tile_y - 1) &&
                rng::RandomIntInclusive(1, 40) == 1) {
                AddAmbientSpawn(stage, EntityType::TombLord,
                                floor_spawn_pos - Vec2::New(0.0F, static_cast<float>(kTileSize)));
                tomb_lord_spawned = true;
                gen_tomb_lord = false;
            } else if (rng::RandomIntInclusive(1, 40) == 1) {
                AddAmbientSpawn(stage, EntityType::Caveman, floor_spawn_pos);
            } else if (rng::RandomIntInclusive(1, 40) == 1) {
                AddAmbientSpawn(stage, EntityType::Hawkman, floor_spawn_pos);
            }
        }
    }
}

void AddAmbientOlmecEntities(Stage&) {
}


} // namespace splonks::stage_gen::classic
