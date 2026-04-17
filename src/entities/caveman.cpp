#include "entities/caveman.hpp"
#include "audio.hpp"
#include "entities/common/common.hpp"
#include "entities/common/ground_walker.hpp"
#include "entity/archetype.hpp"
#include "entity/core_types.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>

namespace splonks::entities::caveman {

namespace {

constexpr float kCavemanWalkSpeed = 0.8F;
constexpr float kCavemanAttackSpeed = 1.6F;
constexpr float kCavemanWallHopSpeedX = 1.0F;
constexpr float kCavemanWallHopSpeedY = -6.0F;
constexpr float kCavemanSightVerticalTolerance = 12.0F;
constexpr int kCavemanSightDistance = 100;
constexpr std::uint64_t kCavemanSightScanIntervalFrames = 30;
constexpr int kCavemanIdleMinFrames = 24;
constexpr int kCavemanIdleMaxFrames = 64;
constexpr int kCavemanIdleChance = 120;

void FaceTowards(Entity& caveman, const Vec2& target_pos, const Stage& stage) {
    const Vec2 delta = GetNearestWorldDelta(stage, caveman.GetCenter(), target_pos);
    if (delta.x < 0.0F) {
        caveman.facing = LeftOrRight::Left;
    } else if (delta.x > 0.0F) {
        caveman.facing = LeftOrRight::Right;
    }
}

void StartIdle(Entity& caveman) {
    caveman.ai_state = EntityAiState::Idle;
    caveman.counter_a = static_cast<float>(rng::RandomIntInclusive(kCavemanIdleMinFrames, kCavemanIdleMaxFrames));
    caveman.vel.x = 0.0F;
    TrySetAnimation(caveman, EntityDisplayState::Neutral);
}

void StartWalking(Entity& caveman) {
    caveman.ai_state = EntityAiState::Patrolling;
    caveman.vel.x = caveman.facing == LeftOrRight::Left ? -kCavemanWalkSpeed : kCavemanWalkSpeed;
    TrySetAnimation(caveman, EntityDisplayState::Walk);
}

void StartAttacking(Entity& caveman) {
    caveman.ai_state = EntityAiState::Pursuing;
    caveman.vel.x = caveman.facing == LeftOrRight::Left ? -kCavemanAttackSpeed : kCavemanAttackSpeed;
    TrySetAnimation(caveman, EntityDisplayState::Walk);
}

bool ShouldRunSightScan(const Entity& caveman, std::uint64_t stage_frame) {
    return ((stage_frame + static_cast<std::uint64_t>(caveman.vid.id)) %
            kCavemanSightScanIntervalFrames) == 0;
}

bool CanSeePlayerAhead(
    const Entity& caveman,
    const State& state,
    const Graphics& graphics
) {
    if (!state.player_vid.has_value()) {
        return false;
    }

    const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr || !player->active || player->condition != EntityCondition::Normal) {
        return false;
    }

    const Vec2 caveman_center = caveman.GetCenter();
    const Vec2 player_center = GetNearestWorldPoint(state.stage, caveman_center, player->GetCenter());
    const Vec2 player_delta = player_center - caveman_center;
    if (std::abs(player_delta.y) > kCavemanSightVerticalTolerance ||
        std::abs(player_delta.x) > static_cast<float>(kCavemanSightDistance)) {
        return false;
    }

    const int direction = caveman.facing == LeftOrRight::Left ? -1 : 1;
    if ((direction < 0 && player_delta.x >= 0.0F) || (direction > 0 && player_delta.x <= 0.0F)) {
        return false;
    }

    const WorldRayHit hit = RaycastHorizontal(
        caveman,
        caveman_center,
        direction,
        static_cast<int>(std::abs(player_delta.x)),
        state,
        graphics,
        caveman.vid
    );
    return hit.type == WorldRayHitType::Entity && hit.entity_vid.has_value() && *hit.entity_vid == player->vid;
}

void MaybeWallHopWhileIdle(Entity& caveman, const State& state, const Graphics& graphics) {
    if (!caveman.grounded) {
        return;
    }

    const bool touching_left_wall = common::HasWallAheadForGroundWalker(caveman, state, graphics, -1);
    const bool touching_right_wall = common::HasWallAheadForGroundWalker(caveman, state, graphics, 1);
    if (!touching_left_wall && !touching_right_wall) {
        return;
    }

    caveman.vel.y = kCavemanWallHopSpeedY;
    caveman.vel.x = caveman.facing == LeftOrRight::Left ? -kCavemanWallHopSpeedX : kCavemanWallHopSpeedX;
    caveman.counter_a = std::max(0.0F, caveman.counter_a - 10.0F);
}

} // namespace

void StepEntityLogicAsCaveman(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    Entity& caveman = state.entity_manager.entities[entity_idx];
    if (caveman.last_condition == EntityCondition::Stunned &&
        caveman.condition == EntityCondition::Normal) {
        StartIdle(caveman);
    }
    if (caveman.condition != EntityCondition::Normal) {
        return;
    }

    if (caveman.ai_state != EntityAiState::Pursuing &&
        ShouldRunSightScan(caveman, state.stage_frame) &&
        CanSeePlayerAhead(caveman, state, graphics)) {
        if (state.player_vid.has_value()) {
            if (const Entity* const player = state.entity_manager.GetEntity(*state.player_vid)) {
                FaceTowards(caveman, player->GetCenter(), state.stage);
            }
        }
        audio.PlaySoundEffect(SoundEffect::CavemanNotice);
        StartAttacking(caveman);
        return;
    }

    if (caveman.ai_state == EntityAiState::Pursuing) {
        int direction = caveman.facing == LeftOrRight::Left ? -1 : 1;
        if (common::HasWallAheadForGroundWalker(caveman, state, graphics, direction)) {
            caveman.facing = caveman.facing == LeftOrRight::Left ? LeftOrRight::Right : LeftOrRight::Left;
            direction = -direction;
        }
        caveman.vel.x = static_cast<float>(direction) * kCavemanAttackSpeed;
        SetMovementFlag(caveman, EntityMovementFlag::Running, true);
        SetMovementFlag(caveman, EntityMovementFlag::Walking, true);
        TrySetAnimation(caveman, EntityDisplayState::Walk);
        return;
    }

    if (caveman.ai_state == EntityAiState::Idle) {
        caveman.vel.x = 0.0F;
        TrySetAnimation(caveman, EntityDisplayState::Neutral);
        MaybeWallHopWhileIdle(caveman, state, graphics);
        if (caveman.counter_a > 0.0F) {
            caveman.counter_a -= 1.0F;
            return;
        }

        caveman.facing = rng::RandomIntInclusive(0, 1) == 0 ? LeftOrRight::Left : LeftOrRight::Right;
        StartWalking(caveman);
        return;
    }

    int direction = caveman.facing == LeftOrRight::Left ? -1 : 1;
    if (common::HasWallAheadForGroundWalker(caveman, state, graphics, direction) ||
        !common::HasGroundAheadForGroundWalker(caveman, state, graphics, direction)) {
        caveman.facing = caveman.facing == LeftOrRight::Left ? LeftOrRight::Right : LeftOrRight::Left;
        direction = -direction;
    }

    if (rng::RandomIntInclusive(1, kCavemanIdleChance) == 1) {
        StartIdle(caveman);
        return;
    }

    caveman.vel.x = static_cast<float>(direction) * kCavemanWalkSpeed;
    SetMovementFlag(caveman, EntityMovementFlag::Walking, true);
    TrySetAnimation(caveman, EntityDisplayState::Walk);
}

extern const EntityArchetype kCavemanArchetype{
    .type_ = EntityType::Caveman,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 3,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .can_only_be_picked_up_if_dead_or_stunned = true,
    .impassable = false,
    .hurt_on_contact = true,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .ai_state = EntityAiState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .counter_a = static_cast<float>(kCavemanIdleMinFrames),
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .damage_sound = SoundEffect::CavemanHurt,
    .collide_sound = SoundEffect::Thud,
    .step_logic = StepEntityLogicAsCaveman,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Caveman),
};

} // namespace splonks::entities::caveman
