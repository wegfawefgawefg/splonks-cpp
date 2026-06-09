#include "ents/trap_block.hpp"

#include "audio.hpp"
#include "ents/block.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "sim/fxp.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace splonks::ents::trap_block {

namespace {

constexpr int kFullStageSensorTiles = 0;
constexpr int kMaxConfiguredSensorTiles = 256;
constexpr float kMoveSpeed = 5.5F;
constexpr float kWindupFrames = 30.0F;
constexpr float kAfterImpactCooldownFrames = 30.0F;
constexpr float kStartShake = 0.14F;
constexpr float kWindupShake = 0.08F;
constexpr float kImpactShake = 0.34F;
constexpr float kImpactTileShake = 0.28F;
constexpr float kImpactShakeRadiusTiles = 1.1F;
constexpr sim::Scalar kSensorHalfWidth = sim::Scalar::from_pixels(7);
constexpr sim::Scalar kOneShotMode = sim::Scalar::from_int(1);
constexpr sim::Scalar kHasFired = sim::Scalar::from_int(1);

struct DirectionInfo {
    IVec2 tile_dir;
};

const std::array<DirectionInfo, 4> kDirections{{
    {IVec2::New(-1, 0)},
    {IVec2::New(1, 0)},
    {IVec2::New(0, -1)},
    {IVec2::New(0, 1)},
}};

int GetOpenSensorCacheMarker(const Stage& stage) {
    return static_cast<int>(stage.tile_change_generation + 1U);
}

void InvalidateOpenSensorCache(Ent& block) {
    block.point_a.x = -1;
}

bool IsOneShot(const Ent& block) {
    return block.threshold_a == kOneShotMode;
}

bool HasFiredOneShot(const Ent& block) {
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

int ComputeOpenSensorDistance(const Ent& block, const State& state, const IVec2& direction) {
    const sim::Vec2 block_center = block.GetSimCenter();
    const IVec2 origin_world = IVec2::New(
        block_center.x.to_pixels_trunc(),
        block_center.y.to_pixels_trunc()
    );
    const IVec2 origin_tile = state.stage.GetTileCoordAtWc(origin_world);
    const TileStepRaycastResult ray =
        RaycastTileSteps(state.stage, origin_tile, direction, GetSensorTiles(state, direction));
    return std::clamp(
        ray.open_steps * static_cast<int>(kTileSize),
        0,
        GetMaxSensorDistance(state, direction)
    );
}

void RefreshOpenSensorDistances(Ent& block, const State& state) {
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

int GetCachedOpenSensorDistance(Ent& block, const State& state, std::size_t direction_idx) {
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

sim::Vec2 GetSensorStart(sim::Vec2 center, const DirectionInfo& direction) {
    return center + sim::PixelVec2(
        direction.tile_dir.x * static_cast<int>(kTileSize / 2),
        direction.tile_dir.y * static_cast<int>(kTileSize / 2)
    );
}

sim::AABB MakeSensorAabb(sim::Vec2 center, const DirectionInfo& direction, int open_distance) {
    const sim::Vec2 start = GetSensorStart(center, direction);
    const sim::Vec2 end = start + sim::PixelVec2(
        direction.tile_dir.x * open_distance,
        direction.tile_dir.y * open_distance
    );

    if (direction.tile_dir.x != 0) {
        return sim::AABB::from_corners(
            sim::Vec2{gfxp::min(start.x, end.x), center.y - kSensorHalfWidth},
            sim::Vec2{gfxp::max(start.x, end.x), center.y + kSensorHalfWidth}
        );
    }

    return sim::AABB::from_corners(
        sim::Vec2{center.x - kSensorHalfWidth, gfxp::min(start.y, end.y)},
        sim::Vec2{center.x + kSensorHalfWidth, gfxp::max(start.y, end.y)}
    );
}

bool IsSensorBlockingEnt(const Ent& ent) {
    return ent.active && ent.can_collide && ent.impassable && ent.crusher_pusher;
}

std::optional<sim::Scalar> GetBlockerDistance(
    sim::Vec2 sensor_start,
    sim::AABB blocker_aabb,
    const DirectionInfo& direction
) {
    if (direction.tile_dir.x > 0) {
        if (blocker_aabb.br.x < sensor_start.x) {
            return std::nullopt;
        }
        return gfxp::max(sim::Scalar::zero(), blocker_aabb.tl.x - sensor_start.x);
    }
    if (direction.tile_dir.x < 0) {
        if (blocker_aabb.tl.x > sensor_start.x) {
            return std::nullopt;
        }
        return gfxp::max(sim::Scalar::zero(), sensor_start.x - blocker_aabb.br.x);
    }
    if (direction.tile_dir.y > 0) {
        if (blocker_aabb.br.y < sensor_start.y) {
            return std::nullopt;
        }
        return gfxp::max(sim::Scalar::zero(), blocker_aabb.tl.y - sensor_start.y);
    }
    if (direction.tile_dir.y < 0) {
        if (blocker_aabb.tl.y > sensor_start.y) {
            return std::nullopt;
        }
        return gfxp::max(sim::Scalar::zero(), sensor_start.y - blocker_aabb.br.y);
    }
    return std::nullopt;
}

int GetEntBlockedOpenSensorDistance(
    Ent& block,
    const State& state,
    const Graphics& graphics,
    std::size_t direction_idx
) {
    const DirectionInfo& direction = kDirections[direction_idx];
    const sim::Vec2 center = block.GetSimCenter();
    const int open_distance = GetCachedOpenSensorDistance(block, state, direction_idx);
    const sim::AABB tile_open_sensor = MakeSensorAabb(center, direction, open_distance);
    const sim::Vec2 sensor_start = GetSensorStart(center, direction);

    sim::Scalar blocked_distance = sim::Scalar::from_pixels(open_distance);
    for (const VID& vid : QueryEntsInAabb(state, tile_open_sensor, block.vid)) {
        const Ent* const ent = state.ents.GetEnt(vid);
        if (ent == nullptr || !IsSensorBlockingEnt(*ent)) {
            continue;
        }
        const sim::AABB blocker_aabb = GetNearestWorldAabb(
            state.stage,
            center,
            common::GetContactAabbForEnt(*ent, graphics)
        );
        if (!gfxp::aabbs_intersect(tile_open_sensor, blocker_aabb)) {
            continue;
        }
        const std::optional<sim::Scalar> distance =
            GetBlockerDistance(sensor_start, blocker_aabb, direction);
        if (!distance.has_value()) {
            continue;
        }
        blocked_distance = gfxp::min(blocked_distance, *distance);
    }

    return std::clamp(blocked_distance.to_pixels_floor(), 0, open_distance);
}

sim::AABB GetSensorAabb(
    Ent& block,
    const State& state,
    const Graphics& graphics,
    std::size_t direction_idx
) {
    return MakeSensorAabb(
        block.GetSimCenter(),
        kDirections[direction_idx],
        GetEntBlockedOpenSensorDistance(block, state, graphics, direction_idx)
    );
}

void AddDebugAnnotations(Ent& block, State& state, const Graphics& graphics) {
    if (!state.debug_overlay.show_debug_annotations) {
        return;
    }

    for (std::size_t direction_idx = 0; direction_idx < kDirections.size(); ++direction_idx) {
        const sim::AABB tile_open_sensor = MakeSensorAabb(
            block.GetSimCenter(),
            kDirections[direction_idx],
            GetCachedOpenSensorDistance(block, state, direction_idx)
        );
        const sim::AABB sensor = GetSensorAabb(block, state, graphics, direction_idx);
        state.AddDebugRectAnnotation(DebugRectAnnotation{
            .area = ToFAABB(tile_open_sensor),
            .color = DebugAnnotationColor{255, 216, 0, 255},
        });
        state.AddDebugRectAnnotation(DebugRectAnnotation{
            .area = ToFAABB(sensor),
            .color = DebugAnnotationColor{255, 32, 32, 255},
        });
    }
}

bool SensorTouchesPlayer(
    Ent& block,
    const State& state,
    const Graphics& graphics,
    std::size_t direction_idx
) {
    const sim::AABB sensor = GetSensorAabb(block, state, graphics, direction_idx);
    if (sensor.br.x <= sensor.tl.x || sensor.br.y <= sensor.tl.y) {
        return false;
    }

    const std::vector<VID> hits = QueryEntsInAabb(state, sensor, block.vid);
    for (const VID& vid : hits) {
        const Ent* const ent = state.ents.GetEnt(vid);
        if (ent == nullptr || !ent->active || !IsPlayerLikeEntType(ent->type_) ||
            ent->condition == EntCondition::Dead) {
            continue;
        }
        if (!WorldAabbsIntersect(
                state.stage,
                sensor,
                common::GetContactAabbForEnt(*ent, graphics)
            )) {
            continue;
        }
        return true;
    }
    return false;
}

std::optional<std::uint32_t> FindTriggerDirection(
    Ent& block,
    const State& state,
    const Graphics& graphics
) {
    std::optional<std::uint32_t> best_direction;
    sim::Scalar best_distance = sim::Scalar::zero();
    const sim::Vec2 block_center = block.GetSimCenter();

    for (std::size_t direction_idx = 0; direction_idx < kDirections.size(); ++direction_idx) {
        if (!SensorTouchesPlayer(block, state, graphics, direction_idx)) {
            continue;
        }

        const DirectionInfo& direction = kDirections[direction_idx];
        sim::Scalar nearest_distance =
            sim::Scalar::from_pixels(GetMaxSensorDistance(state, direction.tile_dir) + 1);
        const sim::AABB sensor = GetSensorAabb(block, state, graphics, direction_idx);
        for (const VID& vid : QueryEntsInAabb(state, sensor, block.vid)) {
            const Ent* const ent = state.ents.GetEnt(vid);
            if (ent == nullptr || !ent->active || !IsPlayerLikeEntType(ent->type_)) {
                continue;
            }
            const sim::Vec2 delta =
                GetNearestWorldDelta(state.stage, block_center, ent->GetSimCenter());
            const sim::Scalar axis_distance =
                (direction.tile_dir.x != 0 ? delta.x : delta.y).abs();
            nearest_distance = gfxp::min(nearest_distance, axis_distance);
        }

        if (!best_direction.has_value() || nearest_distance < best_distance) {
            best_direction = static_cast<std::uint32_t>(direction_idx);
            best_distance = nearest_distance;
        }
    }

    return best_direction;
}

void StoreMoveDirection(Ent& block, const IVec2& direction) {
    block.point_d = direction;
    block.point_label_d = PointLabel::GoingHere;
}

IVec2 GetMoveDirection(const Ent& block) {
    if (block.point_label_d != PointLabel::GoingHere) {
        return IVec2::New(0, 0);
    }
    return block.point_d;
}

sim::Vec2 GetMoveVelocity(const IVec2& direction) {
    const sim::Scalar speed = sim::ToSimScalar(kMoveSpeed);
    return sim::Vec2{
        sim::Scalar::from_int(direction.x) * speed,
        sim::Scalar::from_int(direction.y) * speed
    };
}

bool IsMoving(const Ent& block) {
    return block.ai_state == EntAiState::Disturbed;
}

bool IsWindingUp(const Ent& block) {
    return block.ai_state == EntAiState::Pursuing;
}

bool IsCoolingDown(const Ent& block) {
    return block.ai_state == EntAiState::Returning;
}

void ShowSleepingFrame(Ent& block) {
    SetAnim(block, aframe_ids::SquisherBlock);
    block.aframe_animator.animate = false;
    block.aframe_animator.SetForcedFrame(1);
}

void ShowAwakeAnim(Ent& block) {
    if (block.aframe_animator.anim_id != aframe_ids::SquisherBlock ||
        !block.aframe_animator.animate) {
        block.aframe_animator.PlayLoop(aframe_ids::SquisherBlock);
    }
}

void StartWindup(Ent& block, std::uint32_t direction_idx, State& state) {
    const IVec2 tile_dir = kDirections[static_cast<std::size_t>(direction_idx)].tile_dir;
    StoreMoveDirection(block, tile_dir);
    block.ai_state = EntAiState::Pursuing;
    block.counter_b = sim::ToSimScalar(kWindupFrames);
    block.vel = sim::Vec2::zero();
    block.acc = sim::Vec2::zero();
    block.shake = std::max(block.shake, sim::ToSimScalar(kWindupShake));
    block.aframe_animator.PlayLoop(aframe_ids::SquisherBlock);
    (void)PlayEntCenterSoundEmitter(state, block, audio_asset_ids::BoulderLatch);
}

void StartMove(Ent& block) {
    const IVec2 tile_dir = GetMoveDirection(block);
    block.ai_state = EntAiState::Disturbed;
    block.vel = GetMoveVelocity(tile_dir);
    block.acc = sim::Vec2::zero();
    block.shake = std::max(block.shake, sim::ToSimScalar(kStartShake));
}

void StopMove(Ent& block, State& state) {
    if (IsOneShot(block)) {
        block.threshold_b = kHasFired;
        block.ai_state = EntAiState::Idle;
        block.counter_a = sim::Scalar::zero();
    } else {
        block.ai_state = EntAiState::Returning;
        block.counter_a = sim::ToSimScalar(kAfterImpactCooldownFrames);
    }
    block.vel = sim::Vec2::zero();
    block.acc = sim::Vec2::zero();
    block.shake = std::max(block.shake, sim::ToSimScalar(kImpactShake));
    InvalidateOpenSensorCache(block);
    ShowSleepingFrame(block);
    AddShake(
        state,
        block.GetRenderCenter(),
        kImpactTileShake,
        kImpactTileShake * 0.65F,
        0.0F,
        kImpactShakeRadiusTiles,
        block.vid
    );
}

} // namespace

void MakeTrapBlockOneShot(Ent& block) {
    block.threshold_a = kOneShotMode;
    block.threshold_b = sim::Scalar::zero();
    ShowSleepingFrame(block);
}

void StepEntLogicAsTrapBlock(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& block = state.ents.ents[ent_idx];
    AddDebugAnnotations(block, state, graphics);

    if (block.condition == EntCondition::Dead) {
        return;
    }

    if (IsOneShot(block) && HasFiredOneShot(block)) {
        ShowSleepingFrame(block);
        block.vel = sim::Vec2::zero();
        block.acc = sim::Vec2::zero();
        return;
    }

    if (IsCoolingDown(block)) {
        ShowSleepingFrame(block);
        block.counter_a -= sim::Scalar::from_int(1);
        if (block.counter_a <= sim::Scalar::zero()) {
            block.ai_state = EntAiState::Idle;
            InvalidateOpenSensorCache(block);
        }
        return;
    }

    if (IsWindingUp(block)) {
        ShowAwakeAnim(block);
        block.vel = sim::Vec2::zero();
        block.acc = sim::Vec2::zero();
        block.shake = std::max(block.shake, sim::ToSimScalar(kWindupShake));
        block.counter_b -= sim::Scalar::from_int(1);
        if (block.counter_b <= sim::Scalar::zero()) {
            StartMove(block);
        }
        return;
    }

    if (block.ai_state != EntAiState::Idle) {
        ShowAwakeAnim(block);
        return;
    }

    ShowSleepingFrame(block);
    block.vel = sim::Vec2::zero();
    block.acc = sim::Vec2::zero();
    const std::optional<std::uint32_t> direction_idx =
        FindTriggerDirection(block, state, graphics);
    if (direction_idx.has_value()) {
        StartWindup(block, *direction_idx, state);
    }
}

void StepEntPhysicsAsTrapBlock(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& block = state.ents.ents[ent_idx];
    if (!IsMoving(block)) {
        block.collided_last_frame = block.collided;
        block.collided = false;
        block.grounded = false;
        return;
    }

    const IVec2 move_dir = GetMoveDirection(block);
    block.vel = GetMoveVelocity(move_dir);
    block.acc = sim::Vec2::zero();

    common::PrePartialEulerStep(ent_idx, state, dt);
    block.vel = GetMoveVelocity(move_dir);
    common::DoTileAndEntCollisions(ent_idx, state, graphics, audio);
    common::PostPartialEulerStep(ent_idx, state, dt);

    const bool stopped_x = move_dir.x != 0 && block.vel.x.abs() <= sim::Scalar::zero();
    const bool stopped_y = move_dir.y != 0 && block.vel.y.abs() <= sim::Scalar::zero();
    if (block.collided && (stopped_x || stopped_y)) {
        StopMove(block, state);
        (void)PlayEntCenterSoundEmitter(state, block, audio_asset_ids::BoulderHitGround);
    }
}

extern const EntSpec kTrapBlockSpec{
    .type_ = EntType::TrapBlock,
    .size = EntSpecSize(static_cast<float>(kTileSize), static_cast<float>(kTileSize)),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_hit = true,
    .can_receive_proj_contact = true,
    .can_be_picked_up = false,
    .impassable = true,
    .hurt_on_contact = false,
    .crusher_pusher = true,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .affected_by_ground_friction = false,
    .draw_layer = DrawLayer::Middle,
    .condition = EntCondition::Normal,
    .ai_state = EntAiState::Idle,
    .damage_vuln = DamageVuln::ExplosionOnly,
    .proj_contact_damage_amount = 0,
    .on_death = block::OnDeathAsBlock,
    .step_logic = StepEntLogicAsTrapBlock,
    .step_physics = StepEntPhysicsAsTrapBlock,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::SquisherBlock),
};

} // namespace splonks::ents::trap_block
