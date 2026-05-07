#include "entities/arrow_trap.hpp"

#include "audio.hpp"
#include "effects.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "gameplay_authority.hpp"
#include "gameplay_events.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>

namespace splonks::entities::arrow_trap {

namespace {

constexpr int kArrowTrapMaxSensorDistance = 96;
constexpr int kArrowTrapMaxSensorTileSteps = kArrowTrapMaxSensorDistance / static_cast<int>(kTileSize);
constexpr float kArrowTrapSensorHalfHeight = 3.0F;
constexpr float kArrowTrapMovingEntitySpeed = 0.05F;
constexpr float kArrowTrapArrowSpeed = 8.0F;
constexpr float kArrowGravity = 0.10F;
constexpr float kArrowRotationVelocityEpsilon = 0.01F;
constexpr float kArrowImpactVelocityScale = 0.18F;
constexpr unsigned int kArrowDamage = 2;

bool HasFired(const Entity& trap) {
    return trap.counter_a > 0.0F;
}

int DirectionForTrap(const Entity& trap) {
    return trap.facing == LeftOrRight::Left ? -1 : 1;
}

Vec2 GetSensorStart(const Entity& trap) {
    const int direction = DirectionForTrap(trap);
    return trap.GetCenter() + Vec2::New(static_cast<float>(direction) * 9.0F, 0.0F);
}

bool ShouldTriggerOnEntity(const Entity& entity) {
    return Length(entity.vel) > kArrowTrapMovingEntitySpeed;
}

void SnapArrowPositionToPixels(Entity& arrow) {
    arrow.pos = Vec2::New(std::round(arrow.pos.x), std::round(arrow.pos.y));
}

Vec2 ToStoredArrowOffset(const Vec2& offset) {
    return Vec2::New(std::round(offset.x), std::round(offset.y));
}

IVec2 ToStoredArrowOffsetPoint(const Vec2& offset) {
    const Vec2 rounded = ToStoredArrowOffset(offset);
    return IVec2::New(static_cast<int>(rounded.x), static_cast<int>(rounded.y));
}

Vec2 FromStoredArrowOffsetPoint(const IVec2& point) {
    return Vec2::New(static_cast<float>(point.x), static_cast<float>(point.y));
}

int GetOpenSensorCacheMarker(const Stage& stage) {
    return static_cast<int>(stage.tile_change_generation + 1U);
}

int ComputeOpenSensorDistance(const Entity& trap, const State& state) {
    const int direction = DirectionForTrap(trap);
    const Vec2 start = GetSensorStart(trap);
    const IVec2 origin_tile = state.stage.GetTileCoordAtWc(ToIVec2(start));
    const TileStepRaycastResult ray = RaycastTileSteps(
        state.stage,
        origin_tile,
        IVec2::New(direction, 0),
        kArrowTrapMaxSensorTileSteps
    );
    return std::clamp(
        ray.open_steps * static_cast<int>(kTileSize),
        0,
        kArrowTrapMaxSensorDistance
    );
}

int GetCachedOpenSensorDistance(Entity& trap, const State& state) {
    const int cache_marker = GetOpenSensorCacheMarker(state.stage);
    if (trap.point_a.x != cache_marker) {
        trap.point_a.x = cache_marker;
        trap.point_a.y = ComputeOpenSensorDistance(trap, state);
    }
    return trap.point_a.y;
}

AABB GetOpenSensorAabb(Entity& trap, const State& state) {
    const int direction = DirectionForTrap(trap);
    const Vec2 start = GetSensorStart(trap);
    const int open_distance = GetCachedOpenSensorDistance(trap, state);
    const float end_x = start.x + static_cast<float>(direction * open_distance);
    return AABB::New(
        Vec2::New(std::min(start.x, end_x), start.y - kArrowTrapSensorHalfHeight),
        Vec2::New(std::max(start.x, end_x), start.y + kArrowTrapSensorHalfHeight)
    );
}

void AddArrowTrapDebugAnnotations(Entity& trap, State& state) {
    if (!state.debug_overlay.show_debug_annotations) {
        return;
    }

    const AABB sensor_aabb = GetOpenSensorAabb(trap, state);
    state.AddDebugRectAnnotation(DebugRectAnnotation{
        .area = sensor_aabb,
        .color = DebugAnnotationColor{255, 192, 0, 255},
    });
    state.AddDebugLabelAnnotation(DebugLabelAnnotation{
        .world_pos = sensor_aabb.tl + Vec2::New(2.0F, -6.0F),
        .text = "arrow sensor",
        .color = DebugAnnotationColor{255, 192, 0, 255},
    });
}

bool SensorTouchesMovingEntity(
    Entity& trap,
    const State& state,
    const Graphics& graphics
) {
    const AABB sensor_aabb = GetOpenSensorAabb(trap, state);
    if (sensor_aabb.br.x <= sensor_aabb.tl.x) {
        return false;
    }

    const std::vector<VID> hits = QueryEntitiesInAabb(state, sensor_aabb, trap.vid);
    for (const VID& vid : hits) {
        const Entity* const entity = state.entity_manager.GetEntity(vid);
        if (entity == nullptr || !entity->active || !entity->can_be_hit) {
            continue;
        }
        if (!ShouldTriggerOnEntity(*entity)) {
            continue;
        }
        if (!WorldAabbsIntersect(
                state.stage,
                sensor_aabb,
                entities::common::GetContactAabbForEntity(*entity, graphics)
            )) {
            continue;
        }
        return true;
    }

    return false;
}

Entity* SpawnArrow(State& state, const Vec2& center, int direction, const VID& trap_vid) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return nullptr;
    }

    Entity* const arrow = state.entity_manager.GetEntityMut(*vid);
    if (arrow == nullptr) {
        return nullptr;
    }

    SetEntityAs(*arrow, EntityType::Arrow);
    arrow->SetCenter(center);
    arrow->vel = Vec2::New(static_cast<float>(direction) * kArrowTrapArrowSpeed, 0.0F);
    arrow->acc = Vec2::New(0.0F, 0.0F);
    arrow->facing = direction < 0 ? LeftOrRight::Left : LeftOrRight::Right;
    arrow->rotation = 0.0F;
    arrow->thrown_by = trap_vid;
    arrow->thrown_immunity_timer = entities::common::kThrownByImmunityDuration;
    arrow->projectile_contact_damage_type = DamageType::Attack;
    arrow->projectile_contact_damage_amount = kArrowDamage;
    arrow->projectile_contact_timer = entities::common::kProjectileContactDuration;
    arrow->can_apply_projectile_contact = false;
    (void)AddEffect(*arrow, EffectId::NoGravityUntilContact);
    return arrow;
}

Entity* SpawnLooseArrow(State& state, const Vec2& center) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return nullptr;
    }

    Entity* const arrow = state.entity_manager.GetEntityMut(*vid);
    if (arrow == nullptr) {
        return nullptr;
    }

    SetEntityAs(*arrow, EntityType::Arrow);
    arrow->SetCenter(center);
    SnapArrowPositionToPixels(*arrow);
    arrow->vel = Vec2::New(0.0F, 0.0F);
    arrow->acc = Vec2::New(0.0F, 0.0F);
    arrow->projectile_contact_timer = 0;
    arrow->projectile_contact_damage_amount = kArrowDamage;
    arrow->can_apply_projectile_contact = false;
    arrow->thrown_by.reset();
    return arrow;
}

void FireTrap(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;
    Entity& trap = state.entity_manager.entities[entity_idx];
    if (HasFired(trap)) {
        return;
    }
    if (!HasLocalGameplayAuthorityForEntity(state, trap.vid)) {
        return;
    }

    const int direction = DirectionForTrap(trap);
    const Vec2 arrow_center = trap.GetCenter() + Vec2::New(
        static_cast<float>(direction) * 10.0F,
        -4.0F
    );
    Entity* const arrow = SpawnArrow(state, arrow_center, direction, trap.vid);
    if (arrow == nullptr) {
        return;
    }
    EmitEntitySpawnedGameplayEvent(state, *arrow);

    trap.counter_a = 1.0F;
    (void)PlayWorldSoundEmitter(state, arrow_center, audio_asset_ids::Throw);
}

void StepEntityLogicAsArrowTrap(
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

    Entity& trap = state.entity_manager.entities[entity_idx];
    if (!trap.active || HasFired(trap)) {
        return;
    }

    AddArrowTrapDebugAnnotations(trap, state);

    if (!SensorTouchesMovingEntity(trap, state, graphics)) {
        return;
    }

    FireTrap(entity_idx, state, audio);
}

void StepEntityLogicAsArrow(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& arrow = state.entity_manager.entities[entity_idx];
    if (arrow.held_by_vid.has_value()) {
        arrow.entity_a.reset();
        arrow.rotation = 0.0F;
        SnapArrowPositionToPixels(arrow);
        return;
    }

    if (arrow.entity_a.has_value()) {
        const Entity* const stuck_to = state.entity_manager.GetEntity(*arrow.entity_a);
        if (stuck_to == nullptr || !stuck_to->active) {
            arrow.entity_a.reset();
            arrow.has_physics = true;
            arrow.can_collide = true;
            arrow.vel = Vec2::New(0.0F, 0.0F);
            arrow.acc = Vec2::New(0.0F, kArrowGravity);
            arrow.projectile_contact_timer = 0;
            arrow.can_apply_projectile_contact = false;
            return;
        }

        arrow.has_physics = false;
        arrow.can_collide = false;
        arrow.vel = Vec2::New(0.0F, 0.0F);
        arrow.acc = Vec2::New(0.0F, 0.0F);
        arrow.SetCenter(stuck_to->GetCenter() + FromStoredArrowOffsetPoint(arrow.point_a));
        SnapArrowPositionToPixels(arrow);
        return;
    }

    if (!arrow.has_physics) {
        SnapArrowPositionToPixels(arrow);
        return;
    }

    if (Length(arrow.vel) > kArrowRotationVelocityEpsilon) {
        if (arrow.projectile_contact_timer > 0) {
            arrow.projectile_contact_damage_amount = kArrowDamage;
        }
        if (std::abs(arrow.vel.x) > kArrowRotationVelocityEpsilon) {
            arrow.facing = arrow.vel.x < 0.0F ? LeftOrRight::Left : LeftOrRight::Right;
        }
        const float horizontal_speed = std::max(std::abs(arrow.vel.x), kArrowRotationVelocityEpsilon);
        const float relative_rotation =
            std::atan2(arrow.vel.y, horizontal_speed) * (180.0F / 3.14159265F);
        arrow.rotation = arrow.facing == LeftOrRight::Left ? -relative_rotation : relative_rotation;
    }
    const float gravity_scale =
        GetModifiedEffectValue(arrow, EffectModifierTarget::GravityScale, 1.0F);
    arrow.acc.y += kArrowGravity * gravity_scale;
}

bool CanArrowHitEntity(const Entity& arrow, const Entity& other) {
    if (!arrow.active || !other.active || arrow.projectile_contact_timer == 0) {
        return false;
    }
    if (other.type_ == EntityType::Arrow) {
        return false;
    }
    if (arrow.held_by_vid.has_value()) {
        return false;
    }
    if (!other.can_be_hit || !other.can_receive_projectile_contact || !other.can_collide) {
        return false;
    }
    if (arrow.thrown_by.has_value() && other.vid == *arrow.thrown_by) {
        return false;
    }
    return true;
}

Entity* GetHeldBow(Entity& collector, State& state) {
    if (!collector.holding_vid.has_value()) {
        return nullptr;
    }

    Entity* const held = state.entity_manager.GetEntityMut(*collector.holding_vid);
    if (held == nullptr || !held->active || held->type_ != EntityType::Bow) {
        return nullptr;
    }
    return held;
}

bool TryCollectLooseArrowIntoHeldBow(
    std::size_t arrow_idx,
    std::size_t collector_idx,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    if (graphics == nullptr || audio == nullptr ||
        arrow_idx >= state.entity_manager.entities.size() ||
        collector_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& arrow = state.entity_manager.entities[arrow_idx];
    Entity& collector = state.entity_manager.entities[collector_idx];
    if (!arrow.active || arrow.held_by_vid.has_value() || arrow.projectile_contact_timer > 0 ||
        arrow.buyable.active || !collector.can_collect_pickups) {
        return false;
    }

    Entity* const bow = GetHeldBow(collector, state);
    if (bow == nullptr) {
        return false;
    }

    bow->counter_b += 1.0F;
    if (!bow->entity_a.has_value()) {
        SetAnimation(
            *bow,
            bow->counter_b > 0.0F ? frame_data_ids::BowLooseLoaded
                                  : frame_data_ids::BowLooseEmpty
        );
    }
    (void)PlayEntityCenterSoundEmitter(state, *bow, audio_asset_ids::Equip);
    common::DeactivateCollectedPickup(arrow_idx, state, *graphics);
    return true;
}

void StickArrowToEntity(Entity& arrow, Entity& other, State& state) {
    const Vec2 other_center = other.GetCenter();
    const Vec2 arrow_center = GetNearestWorldPoint(state.stage, other_center, arrow.GetCenter());
    const Vec2 stored_offset = ToStoredArrowOffset(arrow_center - other_center);

    arrow.SetCenter(other_center + stored_offset);
    SnapArrowPositionToPixels(arrow);
    arrow.entity_a = other.vid;
    arrow.point_a = ToStoredArrowOffsetPoint(stored_offset);
    arrow.has_physics = false;
    arrow.can_collide = false;
    arrow.projectile_contact_timer = 0;
    arrow.can_apply_projectile_contact = false;
    arrow.thrown_by.reset();
    arrow.thrown_immunity_timer = 0;
    arrow.vel = Vec2::New(0.0F, 0.0F);
    arrow.acc = Vec2::New(0.0F, 0.0F);
    EmitEntityStatePatchedGameplayEvent(state, arrow, arrow);
}

entities::common::ContactResolution OnEntityContactAsArrow(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const entities::common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)graphics;
    (void)audio;
    if (TryCollectLooseArrowIntoHeldBow(entity_idx, other_entity_idx, state, graphics, audio)) {
        return {};
    }

    if (entity_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return {};
    }

    Entity& arrow = state.entity_manager.entities[entity_idx];
    Entity& other_entity = state.entity_manager.entities[other_entity_idx];

    const bool swept_contact = context.phase == entities::common::ContactPhase::SweptEntered;
    const bool blocked_impassable_contact =
        context.phase == entities::common::ContactPhase::AttemptedBlocked &&
        context.has_impact &&
        other_entity.impassable;
    if ((!swept_contact && !blocked_impassable_contact) || audio == nullptr) {
        return {};
    }

    if (!CanArrowHitEntity(arrow, other_entity)) {
        return {};
    }
    if (!HasLocalGameplayAuthorityForEntity(state, arrow.vid)) {
        return {};
    }

    const Vec2 impact_velocity = arrow.vel;
    if (arrow.collide_sound.has_value()) {
        (void)PlayWorldSoundEmitter(state, arrow.GetCenter(), *arrow.collide_sound);
    }
    const entities::common::DamageResult damage_result = entities::common::TryHitEntity(
        other_entity_idx,
        state,
        *audio,
        arrow.projectile_contact_damage_type,
        arrow.projectile_contact_damage_amount,
        entities::common::HitOptions{
            .source_vid = arrow.vid,
            .knockback = entities::common::KnockbackSpec{
                .velocity = arrow.vel * kArrowImpactVelocityScale,
                .clear_velocity = false,
                .clear_acceleration = true,
                .thrown_by = arrow.thrown_by,
                .thrown_immunity_timer = entities::common::kThrownByImmunityDuration,
                .projectile_contact_damage_type = DamageType::Attack,
                .projectile_contact_damage_amount = 1,
                .projectile_contact_duration = entities::common::kProjectileContactDuration,
            },
            .allow_remote_player_target = true,
        }
    );
    (void)damage_result;
    arrow.vel = impact_velocity;
    Entity& updated_other_entity = state.entity_manager.entities[other_entity_idx];
    if (updated_other_entity.active) {
        StickArrowToEntity(arrow, updated_other_entity, state);
    } else {
        EmitEntityDeactivatedGameplayEvent(state, arrow);
        state.entity_manager.SetInactive(entity_idx);
    }

    return entities::common::ContactResolution{.stop_sweep = true};
}

entities::common::ContactResolution OnTileContactAsArrow(
    std::size_t entity_idx,
    const entities::common::ContactContext& context,
    State& state
) {
    if (context.phase != entities::common::ContactPhase::AttemptedBlocked ||
        entity_idx >= state.entity_manager.entities.size()) {
        return {};
    }

    Entity& arrow = state.entity_manager.entities[entity_idx];
    if (!arrow.active) {
        return {};
    }

    arrow.vel = Vec2::New(0.0F, 0.0F);
    arrow.acc = Vec2::New(0.0F, 0.0F);
    arrow.projectile_contact_timer = 0;
    arrow.projectile_contact_damage_amount = 0;
    arrow.can_apply_projectile_contact = false;
    arrow.thrown_by.reset();
    arrow.has_physics = false;
    SnapArrowPositionToPixels(arrow);
    EmitEntityStatePatchedGameplayEvent(state, arrow, arrow);
    return entities::common::ContactResolution{.stop_sweep = true};
}

EntityDamageEffectResult OnDamageAsArrow(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
) {
    (void)audio;
    (void)amount;
    (void)damage_applied;
    if (damage_type != DamageType::Explosion ||
        entity_idx >= state.entity_manager.entities.size()) {
        return EntityDamageEffectResult::None;
    }

    Entity& arrow = state.entity_manager.entities[entity_idx];
    arrow.has_physics = true;
    arrow.can_collide = true;
    arrow.can_apply_projectile_contact = false;
    arrow.projectile_contact_damage_type = DamageType::Attack;
    arrow.projectile_contact_damage_amount = kArrowDamage;
    return EntityDamageEffectResult::None;
}

void OnDeathAsArrowTrap(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    const Entity& trap = state.entity_manager.entities[entity_idx];
    if (HasFired(trap)) {
        return;
    }
    if (!HasLocalGameplayAuthorityForEntity(state, trap.vid)) {
        return;
    }
    Entity* const arrow = SpawnLooseArrow(state, trap.GetCenter());
    if (arrow != nullptr) {
        EmitEntitySpawnedGameplayEvent(state, *arrow);
    }
}

} // namespace

extern const EntityArchetype kArrowTrapArchetype{
    .type_ = EntityType::ArrowTrap,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = true,
    .can_be_hung_on = true,
    .hurt_on_contact = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Middle,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::ExplosionOnly,
    .on_death = OnDeathAsArrowTrap,
    .step_logic = StepEntityLogicAsArrowTrap,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::ArrowTrap),
};

extern const EntityArchetype kArrowArchetype{
    .type_ = EntityType::Arrow,
    .size = Vec2::New(8.0F, 8.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .affected_by_ground_friction = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingOnly,
    .projectile_contact_damage_amount = kArrowDamage,
    .can_apply_projectile_contact = false,
    .collide_sound = audio_asset_ids::Thud,
    .on_damage = OnDamageAsArrow,
    .step_logic = StepEntityLogicAsArrow,
    .on_entity_contact = OnEntityContactAsArrow,
    .on_tile_contact = OnTileContactAsArrow,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Arrow),
};

} // namespace splonks::entities::arrow_trap
