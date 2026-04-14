#include "entities/damsel.hpp"

#include "audio.hpp"
#include "entities/common/common.hpp"
#include "entities/common/ground_walker.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "math_types.hpp"
#include "particles/ultra_dynamic_particle.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <cmath>
#include <memory>

namespace splonks::entities::damsel {

namespace {

constexpr std::uint64_t kDamselIdleCryIntervalFrames = 1200;
constexpr std::uint64_t kDamselPanicCryIntervalFrames = 360;
constexpr float kRescueKissYOffsetFactor = 0.25F;
constexpr float kRescueKissFloatSpeed = -0.18F;
constexpr std::uint32_t kRescueKissLifetimeFrames = 48;
constexpr float kDamselWalkMinSpeed = 0.1F;
constexpr float kDamselPanicRunSpeed = 1.5F;
constexpr float kDamselHeldReleaseLatch = 1.0F;
constexpr int kDamselRescueHealthGain = 1;

bool IsEntityTouchingExit(const Entity& entity, const Stage& stage) {
    const auto [tl, br] = entity.GetBounds();
    const IVec2 tl_tile = ToIVec2(tl) / static_cast<int>(kTileSize);
    const IVec2 br_tile = ToIVec2(br) / static_cast<int>(kTileSize);
    for (const WorldTileQueryResult& tile_query : QueryTilesInRect(stage, tl_tile, br_tile)) {
        if (tile_query.tile != nullptr && *tile_query.tile == Tile::Exit) {
            return true;
        }
    }
    return false;
}

void SpawnRescueKissParticle(const Vec2& pos, State& state) {
    auto kiss = std::make_unique<UltraDynamicParticle>();
    kiss->frame_data_animator = FrameDataAnimator::New(frame_data_ids::Kiss);
    kiss->finish_on_animation_end = true;
    kiss->draw_layer = DrawLayer::Foreground;
    kiss->counter = kRescueKissLifetimeFrames;
    kiss->pos = pos;
    kiss->size = Vec2::New(12.0F, 10.0F);
    kiss->rot = 0.0F;
    kiss->alpha = 1.0F;
    kiss->vel = Vec2::New(0.0F, kRescueKissFloatSpeed);
    kiss->svel = Vec2::New(0.0F, 0.0F);
    kiss->rotvel = 0.0F;
    kiss->alpha_vel = -0.01F;
    kiss->acc = Vec2::New(0.0F, 0.0F);
    kiss->sacc = Vec2::New(0.0F, 0.0F);
    kiss->rotacc = 0.0F;
    kiss->alpha_acc = 0.0F;
    state.particles.Add(std::move(kiss));
}

Vec2 GetRescueKissPos(const State& state, const Entity& damsel) {
    if (!state.player_vid.has_value()) {
        return damsel.GetCenter();
    }
    const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr || !player->active) {
        return damsel.GetCenter();
    }
    const AABB player_aabb = player->GetAABB();
    return Vec2::New(
        (player_aabb.tl.x + player_aabb.br.x) * 0.5F,
        player_aabb.tl.y + player->size.y * kRescueKissYOffsetFactor
    );
}

void AwardDamselRescueHealth(State& state) {
    if (!state.player_vid.has_value()) {
        return;
    }

    Entity* const player = state.entity_manager.GetEntityMut(*state.player_vid);
    if (player == nullptr || !player->active || player->condition == EntityCondition::Dead) {
        return;
    }

    player->health += kDamselRescueHealthGain;
}

void DetachDamselFromHolder(Entity& damsel, State& state) {
    if (!damsel.held_by_vid.has_value()) {
        return;
    }

    Entity* const holder = state.entity_manager.GetEntityMut(*damsel.held_by_vid);
    if (holder != nullptr) {
        if (holder->holding_vid.has_value() && *holder->holding_vid == damsel.vid) {
            holder->holding_vid.reset();
            holder->holding = false;
            holder->holding_timer = kDefaultHoldingTimer;
        }
        if (holder->back_vid.has_value() && *holder->back_vid == damsel.vid) {
            holder->back_vid.reset();
        }
    }

    damsel.held_by_vid.reset();
    damsel.attachment_mode = AttachmentMode::None;
    StopUsingEntity(damsel);
}

void StartIdle(Entity& damsel) {
    damsel.ai_state = EntityAiState::Idle;
    damsel.vel.x = 0.0F;
    TrySetAnimation(damsel, EntityDisplayState::Neutral);
}

void StartPanicRun(Entity& damsel) {
    damsel.ai_state = EntityAiState::Patrolling;
    const int direction = damsel.facing == LeftOrRight::Left ? -1 : 1;
    damsel.vel.x = static_cast<float>(direction) * kDamselPanicRunSpeed;
    TrySetAnimation(damsel, EntityDisplayState::Walk);
}

void MaybeStartPanicRunFromCarryRelease(Entity& damsel) {
    if (damsel.held_by_vid.has_value()) {
        damsel.counter_a = kDamselHeldReleaseLatch;
        StartIdle(damsel);
        return;
    }

    if (damsel.counter_a <= 0.0F) {
        return;
    }

    damsel.counter_a = 0.0F;
    StartPanicRun(damsel);
}

bool TryRescueDamsel(std::size_t entity_idx, State& state, const Graphics& graphics, Audio& audio) {
    (void)graphics;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& damsel = state.entity_manager.entities[entity_idx];
    if (!damsel.active || damsel.condition == EntityCondition::Dead) {
        return false;
    }
    if (!IsEntityTouchingExit(damsel, state.stage)) {
        return false;
    }

    const Vec2 kiss_pos = GetRescueKissPos(state, damsel);
    SpawnRescueKissParticle(kiss_pos, state);
    audio.PlaySoundEffect(SoundEffect::Smooch);
    AwardDamselRescueHealth(state);

    DetachDamselFromHolder(damsel, state);
    damsel.damage_vulnerability = DamageVulnerability::Immune;
    damsel.can_collide = false;
    damsel.has_physics = false;
    state.entity_manager.SetInactive(entity_idx);
    state.UpdateSidForEntity(entity_idx, graphics);
    return true;
}


void StepPanicRun(Entity& damsel, const State& state, const Graphics& graphics) {
    int direction = damsel.facing == LeftOrRight::Left ? -1 : 1;
    if (common::HasWallAheadForGroundWalker(damsel, state, graphics, direction)) {
        damsel.facing = damsel.facing == LeftOrRight::Left ? LeftOrRight::Right : LeftOrRight::Left;
        direction = -direction;
    }

    damsel.vel.x = static_cast<float>(direction) * kDamselPanicRunSpeed;
    SetMovementFlag(damsel, EntityMovementFlag::Running, true);
    SetMovementFlag(damsel, EntityMovementFlag::Walking, true);
}

void UpdateDamselAnimation(Entity& damsel) {
    if (damsel.condition == EntityCondition::Dead || damsel.condition == EntityCondition::Stunned) {
        return;
    }

    if (damsel.frame_data_animator.animation_id == frame_data_ids::DamselCry) {
        if (!damsel.frame_data_animator.IsFinished()) {
            return;
        }
        damsel.frame_data_animator.loop = true;
    }

    const bool walking = damsel.grounded && std::abs(damsel.vel.x) >= kDamselWalkMinSpeed;
    TrySetAnimation(damsel, walking ? EntityDisplayState::Walk : EntityDisplayState::Neutral);
}

bool ShouldPlayAmbientCry(const Entity& damsel, std::uint64_t stage_frame) {
    const std::uint64_t interval = damsel.ai_state == EntityAiState::Patrolling
                                       ? kDamselPanicCryIntervalFrames
                                       : kDamselIdleCryIntervalFrames;
    return ((stage_frame + static_cast<std::uint64_t>(damsel.vid.id)) % interval) == 0;
}

} // namespace

void StepEntityLogicAsDamsel(
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

    Entity& damsel = state.entity_manager.entities[entity_idx];
    if (!damsel.active || damsel.condition == EntityCondition::Dead) {
        return;
    }

    if (TryRescueDamsel(entity_idx, state, graphics, audio)) {
        return;
    }

    if (damsel.last_condition == EntityCondition::Stunned &&
        damsel.condition == EntityCondition::Normal) {
        StartPanicRun(damsel);
    }

    MaybeStartPanicRunFromCarryRelease(damsel);

    if (damsel.condition == EntityCondition::Normal &&
        damsel.ai_state == EntityAiState::Patrolling && !damsel.held_by_vid.has_value()) {
        StepPanicRun(damsel, state, graphics);
    }

    if (damsel.condition == EntityCondition::Normal &&
        ShouldPlayAmbientCry(damsel, state.stage_frame)) {
        audio.PlaySoundEffect(SoundEffect::DamselAmbientCry);
        SetAnimation(damsel, frame_data_ids::DamselCry);
        damsel.frame_data_animator.loop = false;
    }

    UpdateDamselAnimation(damsel);
}

extern const EntityArchetype kDamselArchetype{
    .type_ = EntityType::Damsel,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 3,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = true,
    .stun_recovers_on_ground = true,
    .stun_recovers_while_held = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .ai_state = EntityAiState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .damage_sound = SoundEffect::DamselHurt,
    .step_logic = StepEntityLogicAsDamsel,
    .alignment = Alignment::Ally,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Damsel),
};

} // namespace splonks::entities::damsel
