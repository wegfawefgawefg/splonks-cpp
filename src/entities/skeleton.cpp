#include "entities/skeleton.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "entities/common/ground_walker.hpp"
#include "frame_data_id.hpp"
#include "particles/sprite_particle.hpp"
#include "on_damage_effects.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <cmath>
#include <memory>

namespace splonks::entities::skeleton {

namespace {

const Vec2 kSkullSize = Vec2::New(7.0F, 4.0F);
const Vec2 kSkeletonSize = Vec2::New(16.0F, 16.0F);
constexpr float kWakeHorizontalDistance = 48.0F;
constexpr float kWakeVerticalAbove = 8.0F;
constexpr float kWakeVerticalBelow = 32.0F;
constexpr float kReturnHorizontalDistance = 96.0F;
constexpr float kReturnVerticalDistance = 64.0F;
constexpr float kSkeletonWalkSpeed = 1.0F;
constexpr float kSkeletonWalkAcceleration = 0.2F;
constexpr float kSkullBreakImpactSpeed = 2.25F;

std::optional<Vec2> GetNearestPlayerDelta(const Entity& entity, const State& state) {
    if (!state.player_vid.has_value()) {
        return std::nullopt;
    }

    const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr || !player->active || player->condition == EntityCondition::Dead) {
        return std::nullopt;
    }

    const Vec2 entity_center = entity.GetCenter();
    const Vec2 player_center = GetNearestWorldPoint(state.stage, entity_center, player->GetCenter());
    return player_center - entity_center;
}

void ResizeEntityPreservingBottomCenter(Entity& entity, const Vec2& new_size) {
    const Vec2 bottom_center = entity.pos + Vec2::New(entity.size.x * 0.5F, entity.size.y);
    entity.size = new_size;
    entity.pos = bottom_center - Vec2::New(new_size.x * 0.5F, new_size.y);
}

void EnterDormantState(Entity& entity) {
    ResizeEntityPreservingBottomCenter(entity, kSkullSize);
    entity.ai_state = EntityAiState::Idle;
    entity.hurt_on_contact = false;
    entity.can_be_stomped = false;
    entity.vel = Vec2::New(0.0F, 0.0F);
    entity.acc = Vec2::New(0.0F, 0.0F);
    entity.frame_data_animator.loop = true;
    TrySetAnimation(entity, EntityDisplayState::Neutral);
}

void EnterGettingUpState(Entity& entity) {
    ResizeEntityPreservingBottomCenter(entity, kSkeletonSize);
    entity.ai_state = EntityAiState::Disturbed;
    entity.hurt_on_contact = false;
    entity.can_be_stomped = false;
    common::DecelerateHorizontallyToStop(entity, kSkeletonWalkAcceleration);
    entity.frame_data_animator.loop = false;
    SetAnimation(entity, frame_data_ids::SkeletonGettingUp);
}

void EnterWalkingState(Entity& entity) {
    ResizeEntityPreservingBottomCenter(entity, kSkeletonSize);
    entity.ai_state = EntityAiState::Patrolling;
    entity.hurt_on_contact = true;
    entity.can_be_stomped = true;
    entity.frame_data_animator.loop = true;
    TrySetAnimation(entity, EntityDisplayState::Walk);
    common::AccelerateHorizontallyTowardSpeed(
        entity,
        entity.facing == LeftOrRight::Left ? -kSkeletonWalkSpeed : kSkeletonWalkSpeed,
        kSkeletonWalkAcceleration
    );
}

bool IsPlayerInWakeRange(const Entity& entity, const State& state) {
    const std::optional<Vec2> player_delta = GetNearestPlayerDelta(entity, state);
    if (!player_delta.has_value()) {
        return false;
    }

    return std::abs(player_delta->x) <= kWakeHorizontalDistance &&
           player_delta->y >= -kWakeVerticalAbove &&
           player_delta->y <= kWakeVerticalBelow;
}

bool IsPlayerOutsideReturnRange(const Entity& entity, const State& state) {
    const std::optional<Vec2> player_delta = GetNearestPlayerDelta(entity, state);
    if (!player_delta.has_value()) {
        return true;
    }

    return std::abs(player_delta->x) > kReturnHorizontalDistance ||
           std::abs(player_delta->y) > kReturnVerticalDistance;
}

void FaceNearestPlayerIfAny(Entity& entity, const State& state) {
    const std::optional<Vec2> player_delta = GetNearestPlayerDelta(entity, state);
    if (!player_delta.has_value()) {
        return;
    }

    if (player_delta->x < 0.0F) {
        entity.facing = LeftOrRight::Left;
    } else if (player_delta->x > 0.0F) {
        entity.facing = LeftOrRight::Right;
    }
}

bool IsGroundedInNarrowPit(const Entity& entity, const State& state, const Graphics& graphics) {
    if (!entity.grounded) {
        return false;
    }

    return common::HasWallAheadForGroundWalker(entity, state, graphics, -1) &&
           common::HasWallAheadForGroundWalker(entity, state, graphics, 1);
}

std::optional<VID> SpawnEntityAtCenter(EntityType type_, const Vec2& center, State& state) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return std::nullopt;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return std::nullopt;
    }

    SetEntityAs(*entity, type_);
    entity->SetCenter(center);
    entity->vel = Vec2::New(0.0F, 0.0F);
    entity->acc = Vec2::New(0.0F, 0.0F);
    return vid;
}

void SpawnSkullBreakEffects(const Vec2& center, State& state) {
    SpawnBreakawayContainerShards(center, state);

    for (int i = 0; i < 2; ++i) {
        SpriteParticle smoke{};
        smoke.frame_data_animator = FrameDataAnimator::New(frame_data_ids::LittleSmoke);
        smoke.draw_layer = DrawLayer::Foreground;
        smoke.counter = 14;
        smoke.pos = center;
        smoke.size = Vec2::New(6.0F, 6.0F);
        smoke.alpha = 0.75F;
        smoke.vel = Vec2::New(
            rng::RandomFloat(-0.6F, 0.6F),
            rng::RandomFloat(-1.2F, -0.3F)
        );
        smoke.svel = Vec2::New(0.2F, 0.2F);
        smoke.alpha_vel = -0.05F;
        state.particles.Add(std::move(smoke));
    }
}

void SpawnSkeletonDeathEffects(const Vec2& center, State& state) {
    SpawnBreakawayContainerShards(center, state);

    for (int i = 0; i < 3; ++i) {
        SpriteParticle smoke{};
        smoke.frame_data_animator = FrameDataAnimator::New(frame_data_ids::LittleSmoke);
        smoke.draw_layer = DrawLayer::Foreground;
        smoke.counter = 12;
        smoke.pos = center;
        smoke.size = Vec2::New(5.0F, 5.0F);
        smoke.alpha = 0.7F;
        smoke.vel = Vec2::New(
            rng::RandomFloat(-1.0F, 1.0F),
            rng::RandomFloat(-1.5F, -0.5F)
        );
        smoke.svel = Vec2::New(0.15F, 0.15F);
        smoke.alpha_vel = -0.06F;
        state.particles.Add(std::move(smoke));
    }
}

void DropLooseSkull(const Vec2& center, State& state) {
    const std::optional<VID> skull_vid = SpawnEntityAtCenter(EntityType::Skull, center, state);
    if (!skull_vid.has_value()) {
        return;
    }

    Entity* const skull = state.entity_manager.GetEntityMut(*skull_vid);
    if (skull == nullptr) {
        return;
    }

    skull->vel = Vec2::New(
        rng::RandomFloat(-1.0F, 1.0F),
        rng::RandomFloat(-1.8F, -0.8F)
    );
}

bool BreakSkull(std::size_t entity_idx, State& state) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& skull = state.entity_manager.entities[entity_idx];
    if (!skull.active || skull.type_ != EntityType::Skull || skull.condition == EntityCondition::Dead) {
        return false;
    }

    skull.health = 0;
    return true;
}

} // namespace

void StepEntityLogicAsSkeleton(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    Entity& skeleton = state.entity_manager.entities[entity_idx];
    if (skeleton.condition != EntityCondition::Normal) {
        skeleton.hurt_on_contact = false;
        skeleton.can_be_stomped = false;
        return;
    }

    switch (skeleton.ai_state) {
    case EntityAiState::Idle:
        skeleton.hurt_on_contact = false;
        common::DecelerateHorizontallyToStop(skeleton, kSkeletonWalkAcceleration);
        if (IsPlayerInWakeRange(skeleton, state)) {
            FaceNearestPlayerIfAny(skeleton, state);
            EnterGettingUpState(skeleton);
        }
        return;
    case EntityAiState::Disturbed:
        skeleton.hurt_on_contact = false;
        common::DecelerateHorizontallyToStop(skeleton, kSkeletonWalkAcceleration);
        if (skeleton.frame_data_animator.animation_id != frame_data_ids::SkeletonGettingUp) {
            EnterGettingUpState(skeleton);
            return;
        }
        if (skeleton.frame_data_animator.IsFinished()) {
            EnterWalkingState(skeleton);
        }
        return;
    case EntityAiState::Patrolling:
    case EntityAiState::Returning:
    case EntityAiState::Pursuing:
        break;
    }

    if ((skeleton.grounded && IsPlayerOutsideReturnRange(skeleton, state)) ||
        IsGroundedInNarrowPit(skeleton, state, graphics)) {
        EnterDormantState(skeleton);
        return;
    }

    int direction = skeleton.facing == LeftOrRight::Left ? -1 : 1;
    if (common::HasWallAheadForGroundWalker(skeleton, state, graphics, direction)) {
        skeleton.facing = skeleton.facing == LeftOrRight::Left ? LeftOrRight::Right : LeftOrRight::Left;
        direction = -direction;
    }

    skeleton.hurt_on_contact = true;
    common::AccelerateHorizontallyTowardSpeed(
        skeleton,
        static_cast<float>(direction) * kSkeletonWalkSpeed,
        kSkeletonWalkAcceleration
    );
    SetMovementFlag(skeleton, EntityMovementFlag::Walking, true);
    TrySetAnimation(skeleton, EntityDisplayState::Walk);
}

void OnDeathAsSkull(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    const Entity& skull = state.entity_manager.entities[entity_idx];
    SpawnSkullBreakEffects(skull.GetCenter(), state);
}

void OnDeathAsSkeleton(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    const Entity& skeleton = state.entity_manager.entities[entity_idx];
    const Vec2 center = skeleton.GetCenter();
    SpawnSkeletonDeathEffects(center, state);
    DropLooseSkull(center, state);
    state.entity_manager.SetInactive(entity_idx);
}

bool TryApplySkullTileImpact(
    std::size_t entity_idx,
    const common::ContactContext& context,
    State& state
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    const Entity& skull = state.entity_manager.entities[entity_idx];
    if (skull.type_ != EntityType::Skull || skull.condition == EntityCondition::Dead) {
        return false;
    }
    if (context.phase != common::ContactPhase::AttemptedBlocked || !context.has_impact) {
        return false;
    }
    if (std::abs(context.impact_velocity) < kSkullBreakImpactSpeed) {
        return false;
    }

    return BreakSkull(entity_idx, state);
}

bool TryApplySkullEntityImpact(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext& context,
    State& state
) {
    if (entity_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    const Entity& skull = state.entity_manager.entities[entity_idx];
    const Entity& other = state.entity_manager.entities[other_entity_idx];
    if (skull.type_ != EntityType::Skull || skull.condition == EntityCondition::Dead || !other.active) {
        return false;
    }
    if (other.type_ == EntityType::Skull) {
        return false;
    }
    if (skull.thrown_by.has_value() && other.vid == *skull.thrown_by) {
        return false;
    }

    const bool thrown_impact =
        context.phase == common::ContactPhase::SweptEntered && skull.projectile_contact_timer > 0;
    const bool blocked_impact =
        context.phase == common::ContactPhase::AttemptedBlocked && context.has_impact &&
        std::abs(context.impact_velocity) >= kSkullBreakImpactSpeed;
    if (!thrown_impact && !blocked_impact) {
        return false;
    }

    return BreakSkull(entity_idx, state);
}

common::ContactResolution OnEntityContactAsSkull(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)graphics;
    (void)audio;
    if (!context.mover_vid.has_value() || *context.mover_vid != state.entity_manager.entities[entity_idx].vid) {
        return common::ContactResolution{};
    }
    return common::ContactResolution{
        .blocks_movement = false,
        .stop_sweep = TryApplySkullEntityImpact(entity_idx, other_entity_idx, context, state),
    };
}

common::ContactResolution OnTileContactAsSkull(
    std::size_t entity_idx,
    const common::ContactContext& context,
    State& state
) {
    return common::ContactResolution{
        .blocks_movement = false,
        .stop_sweep = TryApplySkullTileImpact(entity_idx, context, state),
    };
}

extern const EntityArchetype kSkullArchetype{
    .type_ = EntityType::Skull,
    .size = kSkullSize,
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .collide_sound = audio_asset_ids::Thud,
    .death_sound = audio_asset_ids::BoxBreak,
    .on_death = OnDeathAsSkull,
    .on_entity_contact = OnEntityContactAsSkull,
    .on_tile_contact = OnTileContactAsSkull,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Skull),
};

extern const EntityArchetype kSkeletonArchetype{
    .type_ = EntityType::Skeleton,
    .size = kSkullSize,
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .ai_state = EntityAiState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .collide_sound = audio_asset_ids::Thud,
    .death_sound = audio_asset_ids::BoxBreak,
    .on_death = OnDeathAsSkeleton,
    .step_logic = StepEntityLogicAsSkeleton,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Skull),
};

} // namespace splonks::entities::skeleton
