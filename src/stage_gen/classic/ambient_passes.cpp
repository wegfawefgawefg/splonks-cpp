#include "stage_gen/classic/ambient_passes.hpp"

#include "stage_gen/classic/stage_pass_helpers.hpp"
#include "water.hpp"
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
    return IsWaterTile(tile);
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

void AddGiantSpiderHangSpawn(Stage& stage, const FVec2& pos) {
    AddAmbientSpawn(stage, EntType::GiantSpiderHang, pos);
    AddAmbientSpawn(stage, EntType::Cobweb, pos);
    AddAmbientSpawn(stage, EntType::Cobweb, pos + FVec2::New(static_cast<float>(kTileSize), 0.0F));
}

void AddAmbientMinesEnts(Stage& stage, DetRng& det_rng) {
    const int stage_width = static_cast<int>(stage.GetTileWidth());
    const int stage_height = static_cast<int>(stage.GetTileHeight());
    const bool dark_level = StageIsDarkLevel(stage);
    const bool gen_giant_spider = det_rng.RandomIntInclusive(1, 6) == 1;
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

            const FVec2 tile_pos =
                FVec2::New(static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                          static_cast<float>(tile_y * static_cast<int>(kTileSize)));

            if (tile_y < stage_height - 4 && IsOpenAmbientCeilingSpot(stage, tile_x, tile_y)) {
                const FVec2 ceiling_spawn_pos =
                    tile_pos + FVec2::New(0.0F, static_cast<float>(kTileSize));
                const FVec2 ceiling_spawn_pos_2 =
                    tile_pos +
                    FVec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize));
                const bool open_below_right = IsOpenAmbientCeilingSpot(stage, tile_x + 1, tile_y);

                if (gen_giant_spider && !giant_spider_spawned && open_below_right &&
                    !HasSpawnAtWorldPos(stage, ceiling_spawn_pos) &&
                    !HasSpawnAtWorldPos(stage, ceiling_spawn_pos_2) &&
                    det_rng.RandomIntInclusive(1, 40) == 1) {
                    AddGiantSpiderHangSpawn(stage, ceiling_spawn_pos);
                    giant_spider_spawned = true;
                } else if (!HasSpawnAtWorldPos(stage, ceiling_spawn_pos) &&
                           dark_level && det_rng.RandomIntInclusive(1, 60) == 1) {
                    AddAmbientSpawn(stage, EntType::Lamp, ceiling_spawn_pos);
                } else if (!HasSpawnAtWorldPos(stage, ceiling_spawn_pos) &&
                           dark_level && det_rng.RandomIntInclusive(1, 40) == 1) {
                    AddAmbientSpawn(stage, EntType::Scarab, ceiling_spawn_pos);
                } else if (!HasSpawnAtWorldPos(stage, ceiling_spawn_pos) &&
                           det_rng.RandomIntInclusive(1, 60) == 1) {
                    AddAmbientSpawn(stage, EntType::Bat, ceiling_spawn_pos);
                } else if (!HasSpawnAtWorldPos(stage, ceiling_spawn_pos) &&
                           det_rng.RandomIntInclusive(1, 80) == 1) {
                    AddAmbientSpawn(stage, EntType::SpiderHang, ceiling_spawn_pos);
                }
            }

            if (!IsOpenAmbientFloorSpot(stage, tile_x, tile_y)) {
                continue;
            }
            const FVec2 floor_spawn_pos = tile_pos - FVec2::New(0.0F, static_cast<float>(kTileSize));
            if (det_rng.RandomIntInclusive(1, 60) == 1) {
                AddAmbientSpawn(stage, EntType::Snake, floor_spawn_pos);
            } else if (det_rng.RandomIntInclusive(1, 800) == 1) {
                AddAmbientSpawn(stage, EntType::Caveman, floor_spawn_pos);
            }
        }
    }
}

void AddAmbientJungleEnts(Stage& stage, bool black_market, DetRng& det_rng) {
    const int stage_width = static_cast<int>(stage.GetTileWidth());
    const int stage_height = static_cast<int>(stage.GetTileHeight());
    const bool dark_level = StageIsDarkLevel(stage);

    for (int tile_y = 0; tile_y < stage_height; ++tile_y) {
        for (int tile_x = 0; tile_x < stage_width; ++tile_x) {
            const Tile tile =
                stage.GetTile(static_cast<unsigned int>(tile_x), static_cast<unsigned int>(tile_y));
            const bool in_shop = IsShopRoomAt(stage, tile_x, tile_y);
            const bool in_start = IsStartRoomAt(stage, tile_x, tile_y);
            const FVec2 tile_pos =
                FVec2::New(static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                          static_cast<float>(tile_y * static_cast<int>(kTileSize)));

            if (!in_shop && IsVineAmbientTile(tile) && det_rng.RandomIntInclusive(1, 15) == 1) {
                AddAmbientSpawn(stage, EntType::Monkey, tile_pos);
            }

            if (!in_shop && IsWaterAmbientTile(tile) &&
                !IsCollidableTileAt(stage, tile_x, tile_y) && det_rng.RandomIntInclusive(1, 30) == 1) {
                AddAmbientSpawn(stage, EntType::Piranha, tile_pos + FVec2::New(4.0F, 4.0F));
            }

            if (!IsCollidableTileAt(stage, tile_x, tile_y) || in_shop) {
                continue;
            }
            if (in_start && IsStartRoomAt(stage, tile_x, tile_y - 1)) {
                continue;
            }

            if (tile_y < stage_height - 4 && IsOpenAmbientCeilingSpot(stage, tile_x, tile_y)) {
                const FVec2 ceiling_spawn_pos =
                    tile_pos + FVec2::New(0.0F, static_cast<float>(kTileSize));
                if (dark_level && det_rng.RandomIntInclusive(1, 40) == 1) {
                    AddAmbientSpawn(stage, EntType::Scarab, ceiling_spawn_pos);
                } else if (det_rng.RandomIntInclusive(1, 80) == 1) {
                    AddAmbientSpawn(stage, EntType::Bat, ceiling_spawn_pos);
                }
            }

            if (!IsOpenAmbientFloorSpot(stage, tile_x, tile_y)) {
                continue;
            }
            const FVec2 floor_spawn_pos = tile_pos - FVec2::New(0.0F, static_cast<float>(kTileSize));
            const bool in_water = IsWaterAmbientTile(stage.GetTile(
                static_cast<unsigned int>(tile_x), static_cast<unsigned int>(tile_y - 1)));
            if (!in_water) {
                const bool suppress_mantrap =
                    black_market && ((tile_y * static_cast<int>(kTileSize)) % 128 == 0);
                if (!suppress_mantrap && det_rng.RandomIntInclusive(1, 60) == 1) {
                    AddAmbientSpawn(stage, EntType::Mantrap, floor_spawn_pos);
                } else if (det_rng.RandomIntInclusive(1, 60) == 1) {
                    AddAmbientSpawn(stage, EntType::Caveman, floor_spawn_pos);
                } else if (det_rng.RandomIntInclusive(1, 120) == 1) {
                    AddAmbientSpawn(stage, EntType::FireFrog, floor_spawn_pos);
                } else if (det_rng.RandomIntInclusive(1, 30) == 1) {
                    AddAmbientSpawn(stage, EntType::Frog, floor_spawn_pos);
                }
            } else if (det_rng.RandomIntInclusive(1, 120) == 1) {
                AddAmbientSpawn(stage, EntType::FireFrog, floor_spawn_pos);
            } else if (det_rng.RandomIntInclusive(1, 30) == 1) {
                AddAmbientSpawn(stage, EntType::Frog, floor_spawn_pos);
            }
        }
    }
}

void AddAmbientIceEnts(Stage& stage, DetRng& det_rng) {
    const int stage_width = static_cast<int>(stage.GetTileWidth());
    const int stage_height = static_cast<int>(stage.GetTileHeight());
    const bool dark_level = StageIsDarkLevel(stage);
    const int ufo_denominator = 30;

    for (int tile_y = 0; tile_y < stage_height; ++tile_y) {
        for (int tile_x = 0; tile_x < stage_width; ++tile_x) {
            if (IsShopRoomAt(stage, tile_x, tile_y)) {
                continue;
            }

            const FVec2 tile_pos =
                FVec2::New(static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                          static_cast<float>(tile_y * static_cast<int>(kTileSize)));

            if (IsCollidableTileAt(stage, tile_x, tile_y)) {
                if (!IsStartRoomAt(stage, tile_x, tile_y) && tile_y < stage_height - 4 &&
                    dark_level && IsOpenAmbientCeilingSpot(stage, tile_x, tile_y) &&
                    det_rng.RandomIntInclusive(1, 40) == 1) {
                    const FVec2 ceiling_spawn_pos =
                        tile_pos + FVec2::New(0.0F, static_cast<float>(kTileSize));
                    AddAmbientSpawn(stage, EntType::Scarab, ceiling_spawn_pos);
                }

                if (!IsOpenAmbientFloorSpot(stage, tile_x, tile_y) ||
                    DistanceSqToNearestSpawnType(stage, EntType::BasicExit, tile_pos) <=
                        64.0F * 64.0F) {
                    continue;
                }
                const FVec2 floor_spawn_pos =
                    tile_pos - FVec2::New(0.0F, static_cast<float>(kTileSize));
                if (det_rng.RandomIntInclusive(1, 20) == 1) {
                    AddAmbientSpawn(stage, EntType::Yeti, floor_spawn_pos);
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
            if (det_rng.RandomIntInclusive(1, ufo_denominator) == 1) {
                AddAmbientSpawn(stage, EntType::Ufo,
                                tile_pos - FVec2::New(0.0F, static_cast<float>(kTileSize)));
            }
        }
    }
}

void AddAmbientTempleEnts(Stage& stage, DetRng& det_rng) {
    const int stage_width = static_cast<int>(stage.GetTileWidth());
    const int stage_height = static_cast<int>(stage.GetTileHeight());
    const bool dark_level = StageIsDarkLevel(stage);
    bool gen_tomb_lord = stage.quest_level_number == 13 || det_rng.RandomIntInclusive(1, 4) == 1;
    bool tomb_lord_spawned = HasSpawnType(stage, EntType::TombLord);

    for (int tile_y = 0; tile_y < stage_height; ++tile_y) {
        for (int tile_x = 0; tile_x < stage_width; ++tile_x) {
            if (IsShopRoomAt(stage, tile_x, tile_y) || !IsCollidableTileAt(stage, tile_x, tile_y)) {
                continue;
            }

            const FVec2 tile_pos =
                FVec2::New(static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                          static_cast<float>(tile_y * static_cast<int>(kTileSize)));

            if (tile_y < stage_height - 4 && dark_level &&
                IsOpenAmbientCeilingSpot(stage, tile_x, tile_y) &&
                det_rng.RandomIntInclusive(1, 40) == 1) {
                const FVec2 ceiling_spawn_pos =
                    tile_pos + FVec2::New(0.0F, static_cast<float>(kTileSize));
                AddAmbientSpawn(stage, EntType::Scarab, ceiling_spawn_pos);
            }

            if ((IsStartRoomAt(stage, tile_x, tile_y) &&
                 IsStartRoomAt(stage, tile_x, tile_y - 1)) ||
                !IsOpenAmbientFloorSpot(stage, tile_x, tile_y)) {
                continue;
            }

            const FVec2 floor_spawn_pos = tile_pos - FVec2::New(0.0F, static_cast<float>(kTileSize));
            if (gen_tomb_lord && !tomb_lord_spawned && tile_y >= 2 &&
                !IsCollidableTileAt(stage, tile_x + 1, tile_y - 1) &&
                !IsCollidableTileAt(stage, tile_x + 2, tile_y - 1) &&
                det_rng.RandomIntInclusive(1, 40) == 1) {
                AddAmbientSpawn(stage, EntType::TombLord,
                                floor_spawn_pos - FVec2::New(0.0F, static_cast<float>(kTileSize)));
                tomb_lord_spawned = true;
                gen_tomb_lord = false;
            } else if (det_rng.RandomIntInclusive(1, 40) == 1) {
                AddAmbientSpawn(stage, EntType::Caveman, floor_spawn_pos);
            } else if (det_rng.RandomIntInclusive(1, 40) == 1) {
                AddAmbientSpawn(stage, EntType::Hawkman, floor_spawn_pos);
            }
        }
    }
}

void AddAmbientOlmecEnts(Stage&) {
}


} // namespace splonks::stage_gen::classic
