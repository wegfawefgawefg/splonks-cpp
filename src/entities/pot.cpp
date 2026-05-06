#include "entities/pot.hpp"
#include "on_damage_effects.hpp"

#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "gameplay_events.hpp"
#include "state.hpp"
#include "controls.hpp"

#include <cmath>
#include <random>

namespace splonks::entities::pot {

namespace {

constexpr float kControlledMoveAcc = 0.07F;
constexpr float kControlledAirMoveAcc = 0.04F;
constexpr float kControlledJumpVel = 3.0F;
constexpr float kControlledSlideVel = 3.0F;
constexpr std::uint32_t kControlledSlideCooldownFrames = 120;
constexpr float kPotBreakawayImpactSpeed = 1.0F;

int RandInclusive(int minimum, int maximum) {
    static std::random_device device;
    static std::mt19937 generator(device());
    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(generator);
}


Entity* SpawnEntityAtTopLeft(EntityType type_, const Vec2& pos, State& state) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return nullptr;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return nullptr;
    }

    SetEntityAs(*entity, type_);
    entity->pos = pos;
    entity->vel = Vec2::New(0.0F, 0.0F);
    return entity;
}

void SpawnAndReplicateEntityAtTopLeft(EntityType type_, const Vec2& pos, State& state) {
    Entity* const entity = SpawnEntityAtTopLeft(type_, pos, state);
    if (entity == nullptr) {
        return;
    }
    EmitEntitySpawnedGameplayEvent(state, *entity);
}

void StepControlledPot(Entity& pot, const controls::ControlIntent& control) {
    if (pot.attack_delay_countdown > 0) {
        pot.attack_delay_countdown -= 1;
    }

    if (control.left && !control.right) {
        pot.acc.x -= pot.grounded ? kControlledMoveAcc : kControlledAirMoveAcc;
        pot.facing = LeftOrRight::Left;
    } else if (control.right && !control.left) {
        pot.acc.x += pot.grounded ? kControlledMoveAcc : kControlledAirMoveAcc;
        pot.facing = LeftOrRight::Right;
    }

    if (control.jump_pressed && pot.grounded) {
        pot.vel.y = -kControlledJumpVel;
        pot.grounded = false;
    }

    if (control.attack_pressed && pot.grounded && pot.attack_delay_countdown == 0) {
        pot.vel.x = pot.facing == LeftOrRight::Left ? -kControlledSlideVel : kControlledSlideVel;
        pot.attack_delay_countdown = kControlledSlideCooldownFrames;
    }
}

void ControlEntityAsPot(
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

    Entity& pot = state.entity_manager.entities[entity_idx];
    if (pot.condition == EntityCondition::Dead) {
        return;
    }

    StepControlledPot(pot, controls::GetControlIntentForEntity(pot, state));
}

} // namespace

common::ContactResolution BuildPotImpactResolution(
    bool applied
) {
    return common::ContactResolution{
        .blocks_movement = false,
        .stop_sweep = applied,
    };
}

common::ContactResolution OnEntityContactAsPot(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)other_entity_idx;
    (void)graphics;
    (void)audio;
    if (!context.mover_vid.has_value() || *context.mover_vid != state.entity_manager.entities[entity_idx].vid) {
        return common::ContactResolution{};
    }
    return BuildPotImpactResolution(TryApplyPotImpact(entity_idx, context, state));
}

common::ContactResolution OnTileContactAsPot(
    std::size_t entity_idx,
    const common::ContactContext& context,
    State& state
) {
    return BuildPotImpactResolution(TryApplyPotImpact(entity_idx, context, state));
}

extern const EntityArchetype kPotArchetype{
    .type_ = EntityType::Pot,
    .size = Vec2::New(8.0F, 7.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .buoyancy = 0.35F,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::AnthingExceptJumpOn,
    .death_sound = audio_asset_ids::PotShatter,
    .on_death = OnDeathAsPot,
    .control_logic = ControlEntityAsPot,
    .step_logic = StepEntityLogicAsPot,
    .on_entity_contact = OnEntityContactAsPot,
    .on_tile_contact = OnTileContactAsPot,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Pot),
};

void StepEntityLogicAsPot(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)entity_idx;
    (void)state;
    (void)graphics;
    (void)audio;
    (void)dt;
}

void OnDeathAsPot(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;

    const Entity& pot = state.entity_manager.entities[entity_idx];
    const Vec2 spawn_pos = pot.pos;
    SpawnBreakawayContainerShards(pot.GetCenter(), state);
    const Vec2 spider_spawn_pos = pot.pos + Vec2::New(-8.0F, -8.0F);

    Vec2 snake_spawn_pos = pot.pos + Vec2::New(-8.0F, -8.0F);
    if (pot.point_a.x < 0) {
        snake_spawn_pos = pot.pos + Vec2::New(0.0F, -8.0F);
    } else if (pot.point_a.x > 0) {
        snake_spawn_pos = pot.pos + Vec2::New(-16.0F, -8.0F);
    }

    if (RandInclusive(1, 3) == 1) {
        SpawnAndReplicateEntityAtTopLeft(EntityType::GoldChunk, spawn_pos, state);
    } else if (RandInclusive(1, 6) == 1) {
        SpawnAndReplicateEntityAtTopLeft(EntityType::GoldNugget, spawn_pos, state);
    } else if (RandInclusive(1, 12) == 1) {
        SpawnAndReplicateEntityAtTopLeft(EntityType::EmeraldBig, spawn_pos, state);
    } else if (RandInclusive(1, 12) == 1) {
        SpawnAndReplicateEntityAtTopLeft(EntityType::SapphireBig, spawn_pos, state);
    } else if (RandInclusive(1, 12) == 1) {
        SpawnAndReplicateEntityAtTopLeft(EntityType::RubyBig, spawn_pos, state);
    } else if (RandInclusive(1, 6) == 1) {
        SpawnAndReplicateEntityAtTopLeft(EntityType::Spider, spider_spawn_pos, state);
    } else if (RandInclusive(1, 12) == 1) {
        SpawnAndReplicateEntityAtTopLeft(EntityType::Snake, snake_spawn_pos, state);
    }
}

bool TryApplyPotImpact(
    std::size_t entity_idx,
    const common::ContactContext& context,
    State& state
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& pot = state.entity_manager.entities[entity_idx];
    if (pot.type_ != EntityType::Pot || pot.condition == EntityCondition::Dead) {
        return false;
    }
    if (context.phase != common::ContactPhase::AttemptedBlocked || !context.has_impact) {
        return false;
    }
    if (std::abs(context.impact_velocity) <= kPotBreakawayImpactSpeed) {
        return false;
    }

    pot.point_a.x = context.direction;
    pot.health = 0;
    return true;
}

} // namespace splonks::entities::pot
