#include "entities/trap_block.hpp"

#include "audio.hpp"
#include "entities/block.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace splonks::entities::trap_block {

namespace {

constexpr int kFullStageSensorTiles = 0;
constexpr int kMaxConfiguredSensorTiles = 256;
constexpr float kSensorHalfWidth = 7.0F;
constexpr float kMoveSpeed = 5.5F;
constexpr float kWindupFrames = 30.0F;
constexpr float kAfterImpactCooldownFrames = 30.0F;
constexpr float kStartShake = 0.14F;
constexpr float kWindupShake = 0.08F;
constexpr float kImpactShake = 0.34F;
constexpr float kImpactTileShake = 0.28F;
constexpr float kImpactShakeRadiusTiles = 1.1F;
constexpr float kOneShotMode = 1.0F;
constexpr float kHasFired = 1.0F;

struct DirectionInfo {
    IVec2 tile_dir;
    Vec2 world_dir;
};

const std::array<DirectionInfo, 4> kDirections{{
    {IVec2::New(-1, 0), Vec2::New(-1.0F, 0.0F)},
    {IVec2::New(1, 0), Vec2::New(1.0F, 0.0F)},
    {IVec2::New(0, -1), Vec2::New(0.0F, -1.0F)},
    {IVec2::New(0, 1), Vec2::New(0.0F, 1.0F)},
}};

int GetOpenSensorCacheMarker(const Stage& stage) {
    return static_cast<int>(stage.tile_change_generation + 1U);
}

void InvalidateOpenSensorCache(Entity& block) {
    block.point_a.x = -1;
}

bool IsOneShot(const Entity& block) {
    return block.threshold_a == kOneShotMode;
}

bool HasFiredOneShot(const Entity& block) {
    return block.threshold_b == kHasFired;
}

int GetFullStageSensorTiles(const Stage& stage, const IVec2& direction) {
    const unsigned int tile_count = direction.x != 0 ? stage.GetTileWidth() : stage.GetTileHeight();
    return std::max(1, static_cast<int>(tile_count));
}

int GetSensorTiles(const State& state, const IVec2& direction) {
    int configured_tiles = kFullStageSensorTiles;
    if (state.debug_level.kind == DebugLevelKind::CrusherTrapTest) {
        configured_tiles = std::clamp(
            state.debug_level.crusher_trap_test.squisher_sensor_tiles,
            kFullStageSensorTiles,
            kMaxConfiguredSensorTiles
        );
    }

    if (configured_tiles == kFullStageSensorTiles) {
        return GetFullStageSensorTiles(state.stage, direction);
    }
    return configured_tiles;
}

int GetMaxSensorDistance(const State& state, const IVec2& direction) {
    return GetSensorTiles(state, direction) * static_cast<int>(kTileSize);
}

int ComputeOpenSensorDistance(const Entity& block, const State& state, const IVec2& direction) {
    const IVec2 origin_tile = state.stage.GetTileCoordAtWc(ToIVec2(block.GetCenter()));
    const TileStepRaycastResult ray =
        RaycastTileSteps(state.stage, origin_tile, direction, GetSensorTiles(state, direction));
    return std::clamp(
        ray.open_steps * static_cast<int>(kTileSize),
        0,
        GetMaxSensorDistance(state, direction)
    );
}

void RefreshOpenSensorDistances(Entity& block, const State& state) {
    const int cache_marker = GetOpenSensorCacheMarker(state.stage);
    if (block.point_a.x == cache_marker) {
        return;
    }

    block.point_a.x = cache_marker;
    block.point_a.y = ComputeOpenSensorDistance(block, state, kDirections[0].tile_dir);
    block.point_b.x = ComputeOpenSensorDistance(block, state, kDirections[1].tile_dir);
    block.point_b.y = ComputeOpenSensorDistance(block, state, kDirections[2].tile_dir);
    block.point_c.x = ComputeOpenSensorDistance(block, state, kDirections[3].tile_dir);
}

int GetCachedOpenSensorDistance(Entity& block, const State& state, std::size_t direction_idx) {
    RefreshOpenSensorDistances(block, state);
    switch (direction_idx) {
    case 0:
        return block.point_a.y;
    case 1:
        return block.point_b.x;
    case 2:
        return block.point_b.y;
    case 3:
        return block.point_c.x;
    default:
        return 0;
    }
}

AABB MakeSensorAabb(const Vec2& center, const DirectionInfo& direction, int open_distance) {
    const Vec2 start = center + direction.world_dir * (static_cast<float>(kTileSize) * 0.5F);
    const Vec2 end = start + direction.world_dir * static_cast<float>(open_distance);

    if (direction.tile_dir.x != 0) {
        return AABB::New(
            Vec2::New(std::min(start.x, end.x), center.y - kSensorHalfWidth),
            Vec2::New(std::max(start.x, end.x), center.y + kSensorHalfWidth)
        );
    }

    return AABB::New(
        Vec2::New(center.x - kSensorHalfWidth, std::min(start.y, end.y)),
        Vec2::New(center.x + kSensorHalfWidth, std::max(start.y, end.y))
    );
}

bool IsSensorBlockingEntity(const Entity& entity) {
    return entity.active && entity.can_collide && entity.impassable && entity.crusher_pusher;
}

std::optional<float> GetBlockerDistance(
    const Vec2& sensor_start,
    const AABB& blocker_aabb,
    const DirectionInfo& direction
) {
    if (direction.tile_dir.x > 0) {
        if (blocker_aabb.br.x < sensor_start.x) {
            return std::nullopt;
        }
        return std::max(0.0F, blocker_aabb.tl.x - sensor_start.x);
    }
    if (direction.tile_dir.x < 0) {
        if (blocker_aabb.tl.x > sensor_start.x) {
            return std::nullopt;
        }
        return std::max(0.0F, sensor_start.x - blocker_aabb.br.x);
    }
    if (direction.tile_dir.y > 0) {
        if (blocker_aabb.br.y < sensor_start.y) {
            return std::nullopt;
        }
        return std::max(0.0F, blocker_aabb.tl.y - sensor_start.y);
    }
    if (direction.tile_dir.y < 0) {
        if (blocker_aabb.tl.y > sensor_start.y) {
            return std::nullopt;
        }
        return std::max(0.0F, sensor_start.y - blocker_aabb.br.y);
    }
    return std::nullopt;
}

int GetEntityBlockedOpenSensorDistance(
    Entity& block,
    const State& state,
    const Graphics& graphics,
    std::size_t direction_idx
) {
    const DirectionInfo& direction = kDirections[direction_idx];
    const Vec2 center = block.GetCenter();
    const int open_distance = GetCachedOpenSensorDistance(block, state, direction_idx);
    const AABB tile_open_sensor = MakeSensorAabb(center, direction, open_distance);
    const Vec2 sensor_start = center + direction.world_dir * (static_cast<float>(kTileSize) * 0.5F);

    float blocked_distance = static_cast<float>(open_distance);
    for (const VID& vid : QueryEntitiesInAabb(state, tile_open_sensor, block.vid)) {
        const Entity* const entity = state.entity_manager.GetEntity(vid);
        if (entity == nullptr || !IsSensorBlockingEntity(*entity)) {
            continue;
        }
        const AABB blocker_aabb = GetNearestWorldAabb(
            state.stage,
            center,
            common::GetContactAabbForEntity(*entity, graphics)
        );
        if (!AabbsIntersect(tile_open_sensor, blocker_aabb)) {
            continue;
        }
        const std::optional<float> distance =
            GetBlockerDistance(sensor_start, blocker_aabb, direction);
        if (!distance.has_value()) {
            continue;
        }
        blocked_distance = std::min(blocked_distance, *distance);
    }

    return std::clamp(static_cast<int>(std::floor(blocked_distance)), 0, open_distance);
}

AABB GetSensorAabb(
    Entity& block,
    const State& state,
    const Graphics& graphics,
    std::size_t direction_idx
) {
    return MakeSensorAabb(
        block.GetCenter(),
        kDirections[direction_idx],
        GetEntityBlockedOpenSensorDistance(block, state, graphics, direction_idx)
    );
}

void AddDebugAnnotations(Entity& block, State& state, const Graphics& graphics) {
    if (!state.debug_overlay.show_debug_annotations) {
        return;
    }

    for (std::size_t direction_idx = 0; direction_idx < kDirections.size(); ++direction_idx) {
        const AABB tile_open_sensor = MakeSensorAabb(
            block.GetCenter(),
            kDirections[direction_idx],
            GetCachedOpenSensorDistance(block, state, direction_idx)
        );
        const AABB sensor = GetSensorAabb(block, state, graphics, direction_idx);
        state.AddDebugRectAnnotation(DebugRectAnnotation{
            .area = tile_open_sensor,
            .color = DebugAnnotationColor{255, 216, 0, 255},
        });
        state.AddDebugRectAnnotation(DebugRectAnnotation{
            .area = sensor,
            .color = DebugAnnotationColor{255, 32, 32, 255},
        });
    }
}

bool SensorTouchesPlayer(Entity& block, const State& state, const Graphics& graphics, std::size_t direction_idx) {
    const AABB sensor = GetSensorAabb(block, state, graphics, direction_idx);
    if (sensor.br.x <= sensor.tl.x || sensor.br.y <= sensor.tl.y) {
        return false;
    }

    const std::vector<VID> hits = QueryEntitiesInAabb(state, sensor, block.vid);
    for (const VID& vid : hits) {
        const Entity* const entity = state.entity_manager.GetEntity(vid);
        if (entity == nullptr || !entity->active || !IsPlayerLikeEntityType(entity->type_) ||
            entity->condition == EntityCondition::Dead) {
            continue;
        }
        if (!WorldAabbsIntersect(
                state.stage,
                sensor,
                common::GetContactAabbForEntity(*entity, graphics)
            )) {
            continue;
        }
        return true;
    }
    return false;
}

std::optional<std::size_t> FindTriggerDirection(
    Entity& block,
    const State& state,
    const Graphics& graphics
) {
    std::optional<std::size_t> best_direction;
    float best_distance = 0.0F;
    const Vec2 block_center = block.GetCenter();

    for (std::size_t direction_idx = 0; direction_idx < kDirections.size(); ++direction_idx) {
        if (!SensorTouchesPlayer(block, state, graphics, direction_idx)) {
            continue;
        }

        const DirectionInfo& direction = kDirections[direction_idx];
        float nearest_distance =
            static_cast<float>(GetMaxSensorDistance(state, direction.tile_dir) + 1);
        const AABB sensor = GetSensorAabb(block, state, graphics, direction_idx);
        for (const VID& vid : QueryEntitiesInAabb(state, sensor, block.vid)) {
            const Entity* const entity = state.entity_manager.GetEntity(vid);
            if (entity == nullptr || !entity->active || !IsPlayerLikeEntityType(entity->type_)) {
                continue;
            }
            const Vec2 delta = GetNearestWorldDelta(state.stage, block_center, entity->GetCenter());
            const float axis_distance = std::abs(
                direction.tile_dir.x != 0 ? delta.x : delta.y
            );
            nearest_distance = std::min(nearest_distance, axis_distance);
        }

        if (!best_direction.has_value() || nearest_distance < best_distance) {
            best_direction = direction_idx;
            best_distance = nearest_distance;
        }
    }

    return best_direction;
}

void StoreMoveDirection(Entity& block, const IVec2& direction) {
    block.point_d = direction;
    block.point_label_d = PointLabel::GoingHere;
}

IVec2 GetMoveDirection(const Entity& block) {
    if (block.point_label_d != PointLabel::GoingHere) {
        return IVec2::New(0, 0);
    }
    return block.point_d;
}

bool IsMoving(const Entity& block) {
    return block.ai_state == EntityAiState::Disturbed;
}

bool IsWindingUp(const Entity& block) {
    return block.ai_state == EntityAiState::Pursuing;
}

bool IsCoolingDown(const Entity& block) {
    return block.ai_state == EntityAiState::Returning;
}

void ShowSleepingFrame(Entity& block) {
    SetAnimation(block, frame_data_ids::SquisherBlock);
    block.frame_data_animator.animate = false;
    block.frame_data_animator.SetForcedFrame(1);
}

void ShowAwakeAnimation(Entity& block) {
    if (block.frame_data_animator.animation_id != frame_data_ids::SquisherBlock ||
        !block.frame_data_animator.animate) {
        block.frame_data_animator.PlayLoop(frame_data_ids::SquisherBlock);
    }
}

void StartWindup(Entity& block, std::size_t direction_idx, State& state) {
    const IVec2 tile_dir = kDirections[direction_idx].tile_dir;
    StoreMoveDirection(block, tile_dir);
    block.ai_state = EntityAiState::Pursuing;
    block.counter_b = kWindupFrames;
    block.vel = Vec2::New(0.0F, 0.0F);
    block.acc = Vec2::New(0.0F, 0.0F);
    block.shake = std::max(block.shake, kWindupShake);
    block.frame_data_animator.PlayLoop(frame_data_ids::SquisherBlock);
    (void)PlayEntityCenterSoundEmitter(state, block, audio_asset_ids::BoulderLatch);
}

void StartMove(Entity& block) {
    const IVec2 tile_dir = GetMoveDirection(block);
    block.ai_state = EntityAiState::Disturbed;
    block.vel = ToVec2(tile_dir) * kMoveSpeed;
    block.acc = Vec2::New(0.0F, 0.0F);
    block.shake = std::max(block.shake, kStartShake);
}

void StopMove(Entity& block, State& state) {
    if (IsOneShot(block)) {
        block.threshold_b = kHasFired;
        block.ai_state = EntityAiState::Idle;
        block.counter_a = 0.0F;
    } else {
        block.ai_state = EntityAiState::Returning;
        block.counter_a = kAfterImpactCooldownFrames;
    }
    block.vel = Vec2::New(0.0F, 0.0F);
    block.acc = Vec2::New(0.0F, 0.0F);
    block.shake = std::max(block.shake, kImpactShake);
    InvalidateOpenSensorCache(block);
    ShowSleepingFrame(block);
    AddShake(
        state,
        block.GetCenter(),
        kImpactTileShake,
        kImpactTileShake * 0.65F,
        0.0F,
        kImpactShakeRadiusTiles,
        block.vid
    );
}

} // namespace

void MakeTrapBlockOneShot(Entity& block) {
    block.threshold_a = kOneShotMode;
    block.threshold_b = 0.0F;
    ShowSleepingFrame(block);
}

void StepEntityLogicAsTrapBlock(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& block = state.entity_manager.entities[entity_idx];
    AddDebugAnnotations(block, state, graphics);

    if (block.condition == EntityCondition::Dead) {
        return;
    }

    if (IsOneShot(block) && HasFiredOneShot(block)) {
        ShowSleepingFrame(block);
        block.vel = Vec2::New(0.0F, 0.0F);
        block.acc = Vec2::New(0.0F, 0.0F);
        return;
    }

    if (IsCoolingDown(block)) {
        ShowSleepingFrame(block);
        block.counter_a -= 1.0F;
        if (block.counter_a <= 0.0F) {
            block.ai_state = EntityAiState::Idle;
            InvalidateOpenSensorCache(block);
        }
        return;
    }

    if (IsWindingUp(block)) {
        ShowAwakeAnimation(block);
        block.vel = Vec2::New(0.0F, 0.0F);
        block.acc = Vec2::New(0.0F, 0.0F);
        block.shake = std::max(block.shake, kWindupShake);
        block.counter_b -= 1.0F;
        if (block.counter_b <= 0.0F) {
            StartMove(block);
        }
        return;
    }

    if (block.ai_state != EntityAiState::Idle) {
        ShowAwakeAnimation(block);
        return;
    }

    ShowSleepingFrame(block);
    block.vel = Vec2::New(0.0F, 0.0F);
    block.acc = Vec2::New(0.0F, 0.0F);
    const std::optional<std::size_t> direction_idx = FindTriggerDirection(block, state, graphics);
    if (direction_idx.has_value()) {
        StartWindup(block, *direction_idx, state);
    }
}

void StepEntityPhysicsAsTrapBlock(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& block = state.entity_manager.entities[entity_idx];
    if (!IsMoving(block)) {
        block.collided_last_frame = block.collided;
        block.collided = false;
        block.grounded = false;
        return;
    }

    const IVec2 move_dir = GetMoveDirection(block);
    block.vel = ToVec2(move_dir) * kMoveSpeed;
    block.acc = Vec2::New(0.0F, 0.0F);

    common::PrePartialEulerStep(entity_idx, state, dt);
    block.vel = ToVec2(move_dir) * kMoveSpeed;
    common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    common::PostPartialEulerStep(entity_idx, state, dt);

    const bool stopped_x = move_dir.x != 0 && std::abs(block.vel.x) <= 0.0F;
    const bool stopped_y = move_dir.y != 0 && std::abs(block.vel.y) <= 0.0F;
    if (block.collided && (stopped_x || stopped_y)) {
        StopMove(block, state);
        (void)PlayEntityCenterSoundEmitter(state, block, audio_asset_ids::BoulderHitGround);
    }
}

extern const EntityArchetype kTrapBlockArchetype{
    .type_ = EntityType::TrapBlock,
    .size = Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize)),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_hit = true,
    .can_receive_projectile_contact = true,
    .can_be_picked_up = false,
    .impassable = true,
    .hurt_on_contact = false,
    .crusher_pusher = true,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .affected_by_ground_friction = false,
    .draw_layer = DrawLayer::Middle,
    .condition = EntityCondition::Normal,
    .ai_state = EntityAiState::Idle,
    .damage_vulnerability = DamageVulnerability::ExplosionOnly,
    .projectile_contact_damage_amount = 0,
    .on_death = block::OnDeathAsBlock,
    .step_logic = StepEntityLogicAsTrapBlock,
    .step_physics = StepEntityPhysicsAsTrapBlock,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::SquisherBlock),
};

} // namespace splonks::entities::trap_block
