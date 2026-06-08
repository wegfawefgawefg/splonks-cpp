#include "stage_wrap.hpp"

#include "ent.hpp"
#include "graphics.hpp"
#include "sim/fxp.hpp"
#include "stage_lighting.hpp"
#include "stage_acoustics.hpp"
#include "state.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace splonks {

namespace {

Vec2 GetCoreOriginWc(const Stage& stage) {
    return ToVec2(stage.wrap_core_origin_tiles * kTileSize);
}

Vec2 GetCoreSizeWc(const Stage& stage) {
    return ToVec2(stage.wrap_core_size_tiles * kTileSize);
}

void ShiftActiveEnts(State& state, const Vec2& delta) {
    const sim::Vec2 sim_delta = sim::ToSimVec2(delta);
    for (Ent& ent : state.ents.ents) {
        if (!ent.active) {
            continue;
        }
        ent.pos += sim_delta;
    }
}

void ShiftStageSpawnsAndStamps(Stage& stage, const Vec2& delta) {
    for (EntSpawn& spawn : stage.ent_spawns) {
        spawn.pos += delta;
    }
    for (BackgroundStamp& stamp : stage.background_stamps) {
        stamp.pos += delta;
    }
    const IVec2 tile_delta = ToIVec2(delta / static_cast<float>(kTileSize));
    for (StageLight& light : stage.lights) {
        light.tile_pos = light.tile_pos + tile_delta;
    }
    for (StageTileTrigger& trigger : stage.tile_triggers) {
        trigger.tile_pos = trigger.tile_pos + tile_delta;
    }
}

void WrapPosIntoCore(const Stage& stage, Vec2& pos) {
    const Vec2 core_origin = GetCoreOriginWc(stage);
    const Vec2 core_size = GetCoreSizeWc(stage);

    if (stage.border.wrap_x && core_size.x > 0.0F) {
        while (pos.x < core_origin.x) {
            pos.x += core_size.x;
        }
        while (pos.x >= core_origin.x + core_size.x) {
            pos.x -= core_size.x;
        }
    }

    if (stage.border.wrap_y && core_size.y > 0.0F) {
        while (pos.y < core_origin.y) {
            pos.y += core_size.y;
        }
        while (pos.y >= core_origin.y + core_size.y) {
            pos.y -= core_size.y;
        }
    }
}

void WrapPosIntoCore(const Stage& stage, sim::Vec2& pos) {
    const sim::Vec2 core_origin = sim::ToSimVec2(GetCoreOriginWc(stage));
    const sim::Vec2 core_size = sim::ToSimVec2(GetCoreSizeWc(stage));

    if (stage.border.wrap_x && core_size.x > sim::Scalar::zero()) {
        while (pos.x < core_origin.x) {
            pos.x += core_size.x;
        }
        while (pos.x >= core_origin.x + core_size.x) {
            pos.x -= core_size.x;
        }
    }

    if (stage.border.wrap_y && core_size.y > sim::Scalar::zero()) {
        while (pos.y < core_origin.y) {
            pos.y += core_size.y;
        }
        while (pos.y >= core_origin.y + core_size.y) {
            pos.y -= core_size.y;
        }
    }
}

void CropEntsAndShiftBack(State& state, const Vec2& delta_wc) {
    const sim::Vec2 sim_delta_wc = sim::ToSimVec2(delta_wc);
    for (Ent& ent : state.ents.ents) {
        if (!ent.active) {
            continue;
        }
        WrapPosIntoCore(state.stage, ent.pos);
        ent.pos -= sim_delta_wc;
    }
}

void CropStageSpawnsAndStampsAndShiftBack(Stage& stage, const Vec2& delta_wc) {
    for (EntSpawn& spawn : stage.ent_spawns) {
        WrapPosIntoCore(stage, spawn.pos);
        spawn.pos = spawn.pos - delta_wc;
    }
    for (BackgroundStamp& stamp : stage.background_stamps) {
        WrapPosIntoCore(stage, stamp.pos);
        stamp.pos = stamp.pos - delta_wc;
    }
    const IVec2 core_origin = ToIVec2(stage.wrap_core_origin_tiles);
    const IVec2 core_size = ToIVec2(stage.wrap_core_size_tiles);
    for (StageLight& light : stage.lights) {
        if (stage.border.wrap_x && core_size.x > 0) {
            while (light.tile_pos.x < core_origin.x) {
                light.tile_pos.x += core_size.x;
            }
            while (light.tile_pos.x >= core_origin.x + core_size.x) {
                light.tile_pos.x -= core_size.x;
            }
        }
        if (stage.border.wrap_y && core_size.y > 0) {
            while (light.tile_pos.y < core_origin.y) {
                light.tile_pos.y += core_size.y;
            }
            while (light.tile_pos.y >= core_origin.y + core_size.y) {
                light.tile_pos.y -= core_size.y;
            }
        }
        light.tile_pos = light.tile_pos - core_origin;
    }
    for (StageTileTrigger& trigger : stage.tile_triggers) {
        if (stage.border.wrap_x && core_size.x > 0) {
            while (trigger.tile_pos.x < core_origin.x) {
                trigger.tile_pos.x += core_size.x;
            }
            while (trigger.tile_pos.x >= core_origin.x + core_size.x) {
                trigger.tile_pos.x -= core_size.x;
            }
        }
        if (stage.border.wrap_y && core_size.y > 0) {
            while (trigger.tile_pos.y < core_origin.y) {
                trigger.tile_pos.y += core_size.y;
            }
            while (trigger.tile_pos.y >= core_origin.y + core_size.y) {
                trigger.tile_pos.y -= core_size.y;
            }
        }
        trigger.tile_pos = trigger.tile_pos - core_origin;
    }
}

void ExpandStageForWrap(
    State& state,
    Graphics& graphics,
    bool wrap_x,
    bool wrap_y,
    std::uint32_t padding_tiles
) {
    Stage& stage = state.stage;
    stage.SyncTileShakeGrid();
    stage.SyncTileInstanceMetadataGrid();
    const UVec2 old_tile_dims = UVec2::New(stage.GetTileWidth(), stage.GetTileHeight());
    const UVec2 padding_tile_dims = UVec2::New(
        wrap_x ? padding_tiles : 0U,
        wrap_y ? padding_tiles : 0U
    );
    if (padding_tile_dims.x == 0U && padding_tile_dims.y == 0U) {
        stage.wrap_transform_active = false;
        stage.wrap_padding_tiles = 0;
        stage.wrap_core_origin_tiles = UVec2::New(0, 0);
        stage.wrap_core_size_tiles = UVec2::New(0, 0);
        return;
    }

    const UVec2 new_tile_dims = UVec2::New(
        old_tile_dims.x + padding_tile_dims.x * 2U,
        old_tile_dims.y + padding_tile_dims.y * 2U
    );
    std::vector<std::vector<Tile>> tiles(
        static_cast<std::size_t>(new_tile_dims.y),
        std::vector<Tile>(static_cast<std::size_t>(new_tile_dims.x), Tile::Air)
    );
    std::vector<std::vector<Tile>> backwall_tiles(
        static_cast<std::size_t>(new_tile_dims.y),
        std::vector<Tile>(static_cast<std::size_t>(new_tile_dims.x), Tile::Air)
    );
    std::vector<std::vector<Tile>> fluid_tiles(
        static_cast<std::size_t>(new_tile_dims.y),
        std::vector<Tile>(static_cast<std::size_t>(new_tile_dims.x), Tile::Air)
    );
    std::vector<std::vector<sim::Scalar>> fluid_amount(
        static_cast<std::size_t>(new_tile_dims.y),
        std::vector<sim::Scalar>(static_cast<std::size_t>(new_tile_dims.x), sim::Scalar::zero())
    );
    std::vector<std::vector<sim::Scalar>> fluid_display_amount(
        static_cast<std::size_t>(new_tile_dims.y),
        std::vector<sim::Scalar>(static_cast<std::size_t>(new_tile_dims.x), sim::Scalar::zero())
    );
    std::vector<std::vector<sim::Vec2>> fluid_velocity(
        static_cast<std::size_t>(new_tile_dims.y),
        std::vector<sim::Vec2>(
            static_cast<std::size_t>(new_tile_dims.x),
            sim::Vec2::zero()
        )
    );
    std::vector<std::vector<sim::Vec2>> fluid_gravity(
        static_cast<std::size_t>(new_tile_dims.y),
        std::vector<sim::Vec2>(
            static_cast<std::size_t>(new_tile_dims.x),
            sim::Vec2::zero()
        )
    );
    std::vector<std::vector<std::uint8_t>> fluid_gravity_strength(
        static_cast<std::size_t>(new_tile_dims.y),
        std::vector<std::uint8_t>(static_cast<std::size_t>(new_tile_dims.x), 0)
    );
    std::vector<std::vector<sim::Vec2>> fluid_temp_gravity(
        static_cast<std::size_t>(new_tile_dims.y),
        std::vector<sim::Vec2>(
            static_cast<std::size_t>(new_tile_dims.x),
            sim::Vec2::zero()
        )
    );
    std::vector<std::vector<sim::Scalar>> tile_shake(
        static_cast<std::size_t>(new_tile_dims.y),
        std::vector<sim::Scalar>(static_cast<std::size_t>(new_tile_dims.x), sim::Scalar::zero())
    );
    std::vector<std::vector<sim::Scalar>> backwall_tile_shake(
        static_cast<std::size_t>(new_tile_dims.y),
        std::vector<sim::Scalar>(static_cast<std::size_t>(new_tile_dims.x), sim::Scalar::zero())
    );
    std::vector<std::vector<EmbeddedTreasure>> embedded_treasures(
        static_cast<std::size_t>(new_tile_dims.y),
        std::vector<EmbeddedTreasure>(static_cast<std::size_t>(new_tile_dims.x))
    );

    for (unsigned int y = 0; y < old_tile_dims.y; ++y) {
        for (unsigned int x = 0; x < old_tile_dims.x; ++x) {
            tiles[static_cast<std::size_t>(y + padding_tile_dims.y)]
                 [static_cast<std::size_t>(x + padding_tile_dims.x)] =
                     stage.tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            backwall_tiles[static_cast<std::size_t>(y + padding_tile_dims.y)]
                          [static_cast<std::size_t>(x + padding_tile_dims.x)] =
                              stage.backwall_tiles[static_cast<std::size_t>(y)]
                                                  [static_cast<std::size_t>(x)];
            fluid_tiles[static_cast<std::size_t>(y + padding_tile_dims.y)]
                       [static_cast<std::size_t>(x + padding_tile_dims.x)] =
                           stage.fluid_tiles[static_cast<std::size_t>(y)]
                                            [static_cast<std::size_t>(x)];
            fluid_amount[static_cast<std::size_t>(y + padding_tile_dims.y)]
                        [static_cast<std::size_t>(x + padding_tile_dims.x)] =
                            stage.fluid_amount[static_cast<std::size_t>(y)]
                                              [static_cast<std::size_t>(x)];
            fluid_display_amount[static_cast<std::size_t>(y + padding_tile_dims.y)]
                                [static_cast<std::size_t>(x + padding_tile_dims.x)] =
                                    stage.fluid_display_amount[static_cast<std::size_t>(y)]
                                                              [static_cast<std::size_t>(x)];
            fluid_velocity[static_cast<std::size_t>(y + padding_tile_dims.y)]
                          [static_cast<std::size_t>(x + padding_tile_dims.x)] =
                              stage.fluid_velocity[static_cast<std::size_t>(y)]
                                                  [static_cast<std::size_t>(x)];
            fluid_gravity[static_cast<std::size_t>(y + padding_tile_dims.y)]
                         [static_cast<std::size_t>(x + padding_tile_dims.x)] =
                             stage.fluid_gravity[static_cast<std::size_t>(y)]
                                                [static_cast<std::size_t>(x)];
            fluid_gravity_strength[static_cast<std::size_t>(y + padding_tile_dims.y)]
                                  [static_cast<std::size_t>(x + padding_tile_dims.x)] =
                                      stage.fluid_gravity_strength[static_cast<std::size_t>(y)]
                                                                  [static_cast<std::size_t>(x)];
            fluid_temp_gravity[static_cast<std::size_t>(y + padding_tile_dims.y)]
                              [static_cast<std::size_t>(x + padding_tile_dims.x)] =
                                  stage.fluid_temp_gravity[static_cast<std::size_t>(y)]
                                                          [static_cast<std::size_t>(x)];
            tile_shake[static_cast<std::size_t>(y + padding_tile_dims.y)]
                        [static_cast<std::size_t>(x + padding_tile_dims.x)] =
                            stage.tile_shake[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            backwall_tile_shake[static_cast<std::size_t>(y + padding_tile_dims.y)]
                                 [static_cast<std::size_t>(x + padding_tile_dims.x)] =
                                     stage.backwall_tile_shake[static_cast<std::size_t>(y)]
                                                               [static_cast<std::size_t>(x)];
            embedded_treasures[static_cast<std::size_t>(y + padding_tile_dims.y)]
                              [static_cast<std::size_t>(x + padding_tile_dims.x)] =
                                  stage.embedded_treasures[static_cast<std::size_t>(y)]
                                                          [static_cast<std::size_t>(x)];
        }
    }

    stage.tiles = std::move(tiles);
    stage.backwall_tiles = std::move(backwall_tiles);
    stage.fluid_tiles = std::move(fluid_tiles);
    stage.fluid_amount = std::move(fluid_amount);
    stage.fluid_display_amount = std::move(fluid_display_amount);
    stage.fluid_velocity = std::move(fluid_velocity);
    stage.fluid_gravity = std::move(fluid_gravity);
    stage.fluid_gravity_strength = std::move(fluid_gravity_strength);
    stage.fluid_temp_gravity = std::move(fluid_temp_gravity);
    stage.tile_shake = std::move(tile_shake);
    stage.backwall_tile_shake = std::move(backwall_tile_shake);
    stage.embedded_treasures = std::move(embedded_treasures);
    stage.wrap_transform_active = true;
    stage.wrap_padding_tiles = padding_tiles;
    stage.wrap_core_origin_tiles = padding_tile_dims;
    stage.wrap_core_size_tiles = old_tile_dims;

    const Vec2 delta_wc = ToVec2(ToIVec2(padding_tile_dims * kTileSize));
    ShiftActiveEnts(state, delta_wc);
    ShiftStageSpawnsAndStamps(stage, delta_wc);
    graphics.play_cam.pos += delta_wc;
}

void CollapseWrappedStage(State& state, Graphics& graphics) {
    Stage& stage = state.stage;
    stage.SyncTileShakeGrid();
    stage.SyncTileInstanceMetadataGrid();
    if (!stage.wrap_transform_active) {
        return;
    }

    const UVec2 core_origin = stage.wrap_core_origin_tiles;
    const UVec2 core_size = stage.wrap_core_size_tiles;
    std::vector<std::vector<Tile>> tiles(
        static_cast<std::size_t>(core_size.y),
        std::vector<Tile>(static_cast<std::size_t>(core_size.x), Tile::Air)
    );
    std::vector<std::vector<Tile>> backwall_tiles(
        static_cast<std::size_t>(core_size.y),
        std::vector<Tile>(static_cast<std::size_t>(core_size.x), Tile::Air)
    );
    std::vector<std::vector<Tile>> fluid_tiles(
        static_cast<std::size_t>(core_size.y),
        std::vector<Tile>(static_cast<std::size_t>(core_size.x), Tile::Air)
    );
    std::vector<std::vector<sim::Scalar>> fluid_amount(
        static_cast<std::size_t>(core_size.y),
        std::vector<sim::Scalar>(static_cast<std::size_t>(core_size.x), sim::Scalar::zero())
    );
    std::vector<std::vector<sim::Scalar>> fluid_display_amount(
        static_cast<std::size_t>(core_size.y),
        std::vector<sim::Scalar>(static_cast<std::size_t>(core_size.x), sim::Scalar::zero())
    );
    std::vector<std::vector<sim::Vec2>> fluid_velocity(
        static_cast<std::size_t>(core_size.y),
        std::vector<sim::Vec2>(static_cast<std::size_t>(core_size.x), sim::Vec2::zero())
    );
    std::vector<std::vector<sim::Vec2>> fluid_gravity(
        static_cast<std::size_t>(core_size.y),
        std::vector<sim::Vec2>(static_cast<std::size_t>(core_size.x), sim::Vec2::zero())
    );
    std::vector<std::vector<std::uint8_t>> fluid_gravity_strength(
        static_cast<std::size_t>(core_size.y),
        std::vector<std::uint8_t>(static_cast<std::size_t>(core_size.x), 0)
    );
    std::vector<std::vector<sim::Vec2>> fluid_temp_gravity(
        static_cast<std::size_t>(core_size.y),
        std::vector<sim::Vec2>(static_cast<std::size_t>(core_size.x), sim::Vec2::zero())
    );
    std::vector<std::vector<sim::Scalar>> tile_shake(
        static_cast<std::size_t>(core_size.y),
        std::vector<sim::Scalar>(static_cast<std::size_t>(core_size.x), sim::Scalar::zero())
    );
    std::vector<std::vector<sim::Scalar>> backwall_tile_shake(
        static_cast<std::size_t>(core_size.y),
        std::vector<sim::Scalar>(static_cast<std::size_t>(core_size.x), sim::Scalar::zero())
    );
    std::vector<std::vector<EmbeddedTreasure>> embedded_treasures(
        static_cast<std::size_t>(core_size.y),
        std::vector<EmbeddedTreasure>(static_cast<std::size_t>(core_size.x))
    );
    for (unsigned int y = 0; y < core_size.y; ++y) {
        for (unsigned int x = 0; x < core_size.x; ++x) {
            tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                stage.tiles[static_cast<std::size_t>(y + core_origin.y)]
                          [static_cast<std::size_t>(x + core_origin.x)];
            backwall_tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                stage.backwall_tiles[static_cast<std::size_t>(y + core_origin.y)]
                                    [static_cast<std::size_t>(x + core_origin.x)];
            fluid_tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                stage.fluid_tiles[static_cast<std::size_t>(y + core_origin.y)]
                                 [static_cast<std::size_t>(x + core_origin.x)];
            fluid_amount[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                stage.fluid_amount[static_cast<std::size_t>(y + core_origin.y)]
                                  [static_cast<std::size_t>(x + core_origin.x)];
            fluid_display_amount[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                stage.fluid_display_amount[static_cast<std::size_t>(y + core_origin.y)]
                                          [static_cast<std::size_t>(x + core_origin.x)];
            fluid_velocity[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                stage.fluid_velocity[static_cast<std::size_t>(y + core_origin.y)]
                                    [static_cast<std::size_t>(x + core_origin.x)];
            fluid_gravity[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                stage.fluid_gravity[static_cast<std::size_t>(y + core_origin.y)]
                                   [static_cast<std::size_t>(x + core_origin.x)];
            fluid_gravity_strength[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                stage.fluid_gravity_strength[static_cast<std::size_t>(y + core_origin.y)]
                                            [static_cast<std::size_t>(x + core_origin.x)];
            fluid_temp_gravity[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                stage.fluid_temp_gravity[static_cast<std::size_t>(y + core_origin.y)]
                                        [static_cast<std::size_t>(x + core_origin.x)];
            tile_shake[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                stage.tile_shake[static_cast<std::size_t>(y + core_origin.y)]
                                  [static_cast<std::size_t>(x + core_origin.x)];
            backwall_tile_shake[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                stage.backwall_tile_shake[static_cast<std::size_t>(y + core_origin.y)]
                                           [static_cast<std::size_t>(x + core_origin.x)];
            embedded_treasures[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                stage.embedded_treasures[static_cast<std::size_t>(y + core_origin.y)]
                                        [static_cast<std::size_t>(x + core_origin.x)];
        }
    }

    const Vec2 delta_wc = ToVec2(ToIVec2(core_origin * kTileSize));
    CropEntsAndShiftBack(state, delta_wc);
    CropStageSpawnsAndStampsAndShiftBack(stage, delta_wc);
    graphics.play_cam.pos = graphics.play_cam.pos - delta_wc;

    stage.tiles = std::move(tiles);
    stage.backwall_tiles = std::move(backwall_tiles);
    stage.fluid_tiles = std::move(fluid_tiles);
    stage.fluid_amount = std::move(fluid_amount);
    stage.fluid_display_amount = std::move(fluid_display_amount);
    stage.fluid_velocity = std::move(fluid_velocity);
    stage.fluid_gravity = std::move(fluid_gravity);
    stage.fluid_gravity_strength = std::move(fluid_gravity_strength);
    stage.fluid_temp_gravity = std::move(fluid_temp_gravity);
    stage.tile_shake = std::move(tile_shake);
    stage.backwall_tile_shake = std::move(backwall_tile_shake);
    stage.embedded_treasures = std::move(embedded_treasures);
    stage.wrap_transform_active = false;
    stage.wrap_padding_tiles = 0;
    stage.wrap_core_origin_tiles = UVec2::New(0, 0);
    stage.wrap_core_size_tiles = UVec2::New(0, 0);
}

} // namespace

void ApplyToroidalWrapSettings(
    State& state,
    Graphics& graphics,
    bool wrap_x,
    bool wrap_y,
    std::uint32_t padding_tiles,
    bool camera_clamp_enabled
) {
    const bool current_wrap_x = state.stage.border.wrap_x;
    const bool current_wrap_y = state.stage.border.wrap_y;
    const std::uint32_t desired_padding = (wrap_x || wrap_y) ? padding_tiles : 0U;
    const std::uint32_t current_padding = state.stage.wrap_padding_tiles;
    const bool wrap_config_changed = current_wrap_x != wrap_x ||
                                     current_wrap_y != wrap_y ||
                                     current_padding != desired_padding;

    if (wrap_config_changed) {
        if (state.stage.wrap_transform_active) {
            CollapseWrappedStage(state, graphics);
        }

        state.stage.border.wrap_x = false;
        state.stage.border.wrap_y = false;

        if (wrap_x || wrap_y) {
            ExpandStageForWrap(state, graphics, wrap_x, wrap_y, padding_tiles);
        }
    }

    state.stage.border.wrap_x = wrap_x;
    state.stage.border.wrap_y = wrap_y;
    state.stage.wrap_padding_tiles = desired_padding;
    state.stage.camera_clamp_enabled = camera_clamp_enabled;

    if (wrap_config_changed) {
        graphics.ResetTileVariations();
        InvalidateStageLighting(state);
        InvalidateStageAcoustics(state);
        state.RebuildSid(graphics);
    }
}

} // namespace splonks
