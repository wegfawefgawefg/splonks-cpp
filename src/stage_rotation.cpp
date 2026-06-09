#include "stage_rotation.hpp"

#include "audio.hpp"
#include "audio_emitters.hpp"
#include "ents/common/common.hpp"
#include "graphics.hpp"
#include "player_queries.hpp"
#include "render/camera.hpp"
#include "stage_acoustics.hpp"
#include "stage_lighting.hpp"
#include "state.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace splonks {

namespace {

int NormalizeQuarterTurns(int quarter_turns) {
    int normalized = quarter_turns % 4;
    if (normalized < 0) {
        normalized += 4;
    }
    return normalized;
}

float Smoothstep(float t) {
    t = std::clamp(t, 0.0F, 1.0F);
    return t * t * (3.0F - (2.0F * t));
}

sim::FxVec2 GetStagePixelDims(const Stage& stage) {
    return sim::PixelVec2(static_cast<int>(stage.GetWidth()), static_cast<int>(stage.GetHeight()));
}

sim::FxVec2 Half(sim::FxVec2 value) {
    return sim::FxVec2{value.x / 2, value.y / 2};
}

FVec2 RotatePointClockwise(const FVec2& point, const FVec2& old_dims) {
    const FVec2 old_center = old_dims / 2.0F;
    const FVec2 new_center = FVec2::New(old_dims.y, old_dims.x) / 2.0F;
    const FVec2 delta = point - old_center;
    return new_center + FVec2::New(-delta.y, delta.x);
}

FVec2 RotatePoint(const FVec2& point, FVec2 dims, int quarter_turns) {
    FVec2 rotated = point;
    for (int i = 0; i < quarter_turns; ++i) {
        rotated = RotatePointClockwise(rotated, dims);
        dims = FVec2::New(dims.y, dims.x);
    }
    return rotated;
}

sim::FxVec2 RotatePointClockwise(sim::FxVec2 point, sim::FxVec2 old_dims) {
    const sim::FxVec2 old_center = Half(old_dims);
    const sim::FxVec2 new_center = Half(sim::FxVec2{old_dims.y, old_dims.x});
    const sim::FxVec2 delta = point - old_center;
    return new_center + sim::FxVec2{-delta.y, delta.x};
}

sim::FxVec2 RotatePoint(sim::FxVec2 point, sim::FxVec2 dims, int quarter_turns) {
    sim::FxVec2 rotated = point;
    for (int i = 0; i < quarter_turns; ++i) {
        rotated = RotatePointClockwise(rotated, dims);
        dims = sim::FxVec2{dims.y, dims.x};
    }
    return rotated;
}

FVec2 RotateDirectionClockwise(const FVec2& direction) {
    return FVec2::New(-direction.y, direction.x);
}

FVec2 RotateDirection(FVec2 direction, int quarter_turns) {
    for (int i = 0; i < quarter_turns; ++i) {
        direction = RotateDirectionClockwise(direction);
    }
    return direction;
}

sim::FxVec2 RotateDirection(sim::FxVec2 direction, int quarter_turns) {
    for (int i = 0; i < quarter_turns; ++i) {
        direction = sim::FxVec2{-direction.y, direction.x};
    }
    return direction;
}

IVec2 RotateTileCoordClockwise(const IVec2& tile_pos, int old_width, int old_height) {
    (void)old_width;
    return IVec2::New(old_height - 1 - tile_pos.y, tile_pos.x);
}

IVec2 RotateTileCoord(const IVec2& tile_pos, int width, int height, int quarter_turns) {
    IVec2 rotated = tile_pos;
    int current_width = width;
    int current_height = height;
    for (int i = 0; i < quarter_turns; ++i) {
        rotated = RotateTileCoordClockwise(rotated, current_width, current_height);
        std::swap(current_width, current_height);
    }
    return rotated;
}

template <typename T>
std::vector<std::vector<T>> RotateGridClockwise(const std::vector<std::vector<T>>& grid) {
    if (grid.empty() || grid[0].empty()) {
        return {};
    }

    const std::size_t old_height = grid.size();
    const std::size_t old_width = grid[0].size();
    std::vector<std::vector<T>> rotated(old_width, std::vector<T>(old_height));
    for (std::size_t y = 0; y < old_height; ++y) {
        for (std::size_t x = 0; x < old_width; ++x) {
            rotated[x][old_height - 1 - y] = grid[y][x];
        }
    }
    return rotated;
}

template <typename T>
std::vector<std::vector<T>> RotateGrid(std::vector<std::vector<T>> grid, int quarter_turns) {
    for (int i = 0; i < quarter_turns; ++i) {
        grid = RotateGridClockwise(grid);
    }
    return grid;
}

void RotateTileRotationsInPlace(std::vector<std::vector<TileRotation>>& tile_rotations, int quarter_turns) {
    for (std::vector<TileRotation>& row : tile_rotations) {
        for (TileRotation& rotation : row) {
            rotation = RotateTileRotation(rotation, quarter_turns);
        }
    }
}

StageBorder RotateBorderClockwise(const StageBorder& border, StageRotationWrapPolicy wrap_policy) {
    StageBorder rotated{};
    rotated.left = border.bottom;
    rotated.top = border.left;
    rotated.right = border.top;
    rotated.bottom = border.right;
    if (wrap_policy == StageRotationWrapPolicy::SwapXYWrap) {
        rotated.wrap_x = border.wrap_y;
        rotated.wrap_y = border.wrap_x;
    } else {
        rotated.wrap_x = border.wrap_x;
        rotated.wrap_y = border.wrap_y;
    }
    rotated.void_death_y = std::nullopt;
    return rotated;
}

StageBorder RotateBorder(StageBorder border, int quarter_turns, StageRotationWrapPolicy wrap_policy) {
    for (int i = 0; i < quarter_turns; ++i) {
        border = RotateBorderClockwise(border, wrap_policy);
    }
    return border;
}

void RotateParticles(ParticleSystem& particles, const FVec2& old_dims, int quarter_turns) {
    for (SpriteParticle& particle : particles.sprite_particles) {
        particle.pos = RotatePoint(particle.pos, old_dims, quarter_turns);
        particle.vel = FVec2::New(0.0F, 0.0F);
        particle.acc = FVec2::New(0.0F, 0.0F);
    }
    for (ScriptedParticle& particle : particles.scripted_particles) {
        particle.pos = RotatePoint(particle.pos, old_dims, quarter_turns);
    }
    for (RibbonParticle& particle : particles.ribbon_particles) {
        for (std::size_t i = 0; i < particle.point_count; ++i) {
            particle.points[i] = RotatePoint(particle.points[i], old_dims, quarter_turns);
        }
    }
    for (SegmentedSpriteParticle& particle : particles.segmented_sprite_particles) {
        for (std::size_t i = 0; i < particle.point_count; ++i) {
            particle.points[i] = RotatePoint(particle.points[i], old_dims, quarter_turns);
        }
    }
}

void RotateFixedAudioEmitters(AudioEmitterManager& emitters, const FVec2& old_dims, int quarter_turns) {
    for (AudioEmitter& emitter : emitters.emitters) {
        if (!emitter.active || emitter.source_mode != AudioEmitterSourceMode::FixedWorldPos) {
            continue;
        }
        emitter.world_pos = RotatePoint(emitter.world_pos, old_dims, quarter_turns);
    }
}

void ApplyStageRotation(State& state, Graphics& graphics, int quarter_turns) {
    quarter_turns = NormalizeQuarterTurns(quarter_turns);
    if (quarter_turns == 0) {
        return;
    }

    Stage& stage = state.stage;
    const int old_tile_width = static_cast<int>(stage.GetTileWidth());
    const int old_tile_height = static_cast<int>(stage.GetTileHeight());
    const FVec2 old_dims = ToFVec2(GetStagePixelDims(stage));
    const sim::FxVec2 sim_old_dims = GetStagePixelDims(stage);

    stage.SyncTileInstanceMetadataGrid();
    stage.tiles = RotateGrid(stage.tiles, quarter_turns);
    stage.tile_rotations = RotateGrid(stage.tile_rotations, quarter_turns);
    RotateTileRotationsInPlace(stage.tile_rotations, quarter_turns);
    stage.fluid_tiles = RotateGrid(stage.fluid_tiles, quarter_turns);
    stage.fluid_amount = RotateGrid(stage.fluid_amount, quarter_turns);
    stage.fluid_display_amount = RotateGrid(stage.fluid_display_amount, quarter_turns);
    stage.fluid_velocity = RotateGrid(stage.fluid_velocity, quarter_turns);
    stage.fluid_gravity = RotateGrid(stage.fluid_gravity, quarter_turns);
    stage.fluid_gravity_strength = RotateGrid(stage.fluid_gravity_strength, quarter_turns);
    stage.fluid_temp_gravity = RotateGrid(stage.fluid_temp_gravity, quarter_turns);
    for (std::vector<sim::FxVec2>& row : stage.fluid_velocity) {
        for (sim::FxVec2& velocity : row) {
            velocity = RotateDirection(velocity, quarter_turns);
        }
    }
    for (std::vector<sim::FxVec2>& row : stage.fluid_gravity) {
        for (sim::FxVec2& gravity : row) {
            gravity = RotateDirection(gravity, quarter_turns);
        }
    }
    for (std::vector<sim::FxVec2>& row : stage.fluid_temp_gravity) {
        for (sim::FxVec2& gravity : row) {
            gravity = RotateDirection(gravity, quarter_turns);
        }
    }
    stage.backwall_tiles = RotateGrid(stage.backwall_tiles, quarter_turns);
    stage.embedded_treasures = RotateGrid(stage.embedded_treasures, quarter_turns);
    stage.tile_shake = RotateGrid(stage.tile_shake, quarter_turns);
    stage.backwall_tile_shake = RotateGrid(stage.backwall_tile_shake, quarter_turns);
    stage.rooms = RotateGrid(stage.rooms, quarter_turns);

    for (IVec2& path_tile : stage.path) {
        path_tile = RotateTileCoord(path_tile, old_tile_width, old_tile_height, quarter_turns);
    }
    for (StageTileTrigger& trigger : stage.tile_triggers) {
        trigger.tile_pos = RotateTileCoord(trigger.tile_pos, old_tile_width, old_tile_height, quarter_turns);
    }
    for (StageLight& light : stage.lights) {
        light.tile_pos = RotateTileCoord(light.tile_pos, old_tile_width, old_tile_height, quarter_turns);
    }
    for (BackgroundStamp& stamp : stage.background_stamps) {
        stamp.pos = RotatePoint(stamp.pos, old_dims, quarter_turns);
    }
    for (StageGenAnnotation& annotation : stage.stagegen_annotations) {
        annotation.world_pos = RotatePoint(annotation.world_pos, old_dims, quarter_turns);
    }

    stage.border = RotateBorder(stage.border, quarter_turns, state.stage_rotation.wrap_policy);
    if (quarter_turns % 2 != 0) {
        stage.camera_clamp_margin = FVec2::New(stage.camera_clamp_margin.y, stage.camera_clamp_margin.x);
        stage.wrap_core_origin_tiles =
            UVec2::New(stage.wrap_core_origin_tiles.y, stage.wrap_core_origin_tiles.x);
        stage.wrap_core_size_tiles =
            UVec2::New(stage.wrap_core_size_tiles.y, stage.wrap_core_size_tiles.x);
    }
    stage.tile_change_generation += 1;
    stage.SyncTileShakeGrid();

    for (Ent& ent : state.ents.ents) {
        if (!ent.active) {
            continue;
        }
        ent.SetCenter(RotatePoint(ent.GetCenter(), sim_old_dims, quarter_turns));
        ent.pos = sim::PixelVec2(ent.pos.x.to_pixels_round(), ent.pos.y.to_pixels_round());
        ent.vel = sim::FxVec2::zero();
        ent.acc = sim::FxVec2::zero();
        ent.grounded = false;
        ent.SetGrounded(stage);
    }

    RotateParticles(state.particles, old_dims, quarter_turns);
    RotateFixedAudioEmitters(state.audio_emitters, old_dims, quarter_turns);
    state.audio_listener_world_pos = RotatePoint(state.audio_listener_world_pos, old_dims, quarter_turns);
    if (state.gameplay_camera_anchor_world_pos.has_value()) {
        state.gameplay_camera_anchor_world_pos =
            RotatePoint(*state.gameplay_camera_anchor_world_pos, old_dims, quarter_turns);
    }
    graphics.ResetTileVariations();
    state.RebuildSid(graphics);
    InvalidateStageLighting(state);
    InvalidateStageAcoustics(state);
}

void ClearRenderRotation(Graphics& graphics) {
    graphics.world_rotation_active = false;
    graphics.world_rotation_degrees = 0.0F;
    graphics.world_rotation_pivot = FVec2::New(0.0F, 0.0F);
}

void SnapCameraAfterStageRotation(State& state, Graphics& graphics) {
    if (graphics.debug_lock_play_camera) {
        return;
    }

    FVec2 target = GetStageCameraCenter(state.stage);
    float zoom = GetDefaultFollowCameraZoom(graphics);

    const Ent* camera_target_ent = nullptr;
    if (state.controlled_ent_vid.has_value()) {
        camera_target_ent = state.ents.GetEnt(*state.controlled_ent_vid);
    }
    if ((camera_target_ent == nullptr || !camera_target_ent->active) &&
        state.mode == Mode::GameOver) {
        camera_target_ent = GetPrimaryLocalPlayer(state);
    }

    if (graphics.camera_mode == CameraMode::StageFit) {
        zoom = GetStageFitCameraZoom(state.stage, graphics);
    } else if (camera_target_ent != nullptr && camera_target_ent->active) {
        target = ToFVec2(
            ents::common::GetVisualCenterForEnt(
                *camera_target_ent,
                graphics,
                camera_target_ent->GetCenter()
            )
        );
        graphics.play_cam.pos = ClampCameraTargetToStage(state.stage, target);
        target = graphics.play_cam.pos;
    } else if (state.mode == Mode::GameOver && state.gameplay_camera_anchor_world_pos.has_value()) {
        graphics.play_cam.pos = ClampCameraTargetToStage(
            state.stage,
            *state.gameplay_camera_anchor_world_pos
        );
        target = graphics.play_cam.pos;
    }

    graphics.camera.target = target;
    graphics.camera.zoom = zoom * graphics.camera_zoom_multiplier;
}

void SyncRenderRotation(State& state, Graphics& graphics) {
    if (!state.stage_rotation.active) {
        ClearRenderRotation(graphics);
        return;
    }

    const float denom = static_cast<float>(std::max(1, state.stage_rotation.duration_frames));
    const float t = static_cast<float>(state.stage_rotation.elapsed_frames) / denom;
    graphics.world_rotation_active = true;
    graphics.world_rotation_pivot = ToFVec2(state.stage_rotation.pivot);
    graphics.world_rotation_degrees =
        static_cast<float>(state.stage_rotation.quarter_turns * 90) * Smoothstep(t);
}

} // namespace

bool IsStageRotationActive(const State& state) {
    return state.stage_rotation.active;
}

void StartStageRotation(State& state, Graphics& graphics, Audio& audio, int quarter_turns) {
    const int normalized = NormalizeQuarterTurns(quarter_turns);
    if (normalized == 0 || state.stage_rotation.active) {
        return;
    }

    state.stage_rotation.active = true;
    state.stage_rotation.elapsed_frames = 0;
    state.stage_rotation.duration_frames = kDefaultStageRotationFrames;
    state.stage_rotation.quarter_turns = normalized == 3 ? -1 : normalized;
    state.stage_rotation.pivot = Half(GetStagePixelDims(state.stage));
    audio.PlayAudioAsset(audio_asset_ids::BigMachineRotate);
    SyncRenderRotation(state, graphics);
}

void StepStageRotation(State& state, Graphics& graphics) {
    if (!state.stage_rotation.active) {
        ClearRenderRotation(graphics);
        return;
    }

    state.stage_rotation.elapsed_frames += 1;
    if (state.stage_rotation.elapsed_frames > state.stage_rotation.duration_frames) {
        const int quarter_turns = state.stage_rotation.quarter_turns;
        ApplyStageRotation(state, graphics, quarter_turns);
        state.stage_rotation.active = false;
        ClearRenderRotation(graphics);
        SnapCameraAfterStageRotation(state, graphics);
        return;
    }

    SyncRenderRotation(state, graphics);
}

} // namespace splonks
