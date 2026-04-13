#include "entities/pot.hpp"
#include "on_damage_effects.hpp"

#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
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

bool IsControlled(const Entity& entity, const State& state) {
    return state.controlled_entity_vid.has_value() && entity.vid == *state.controlled_entity_vid;
}

void SpawnEntityAtTopLeft(EntityType type_, const Vec2& pos, State& state) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return;
    }

    SetEntityAs(*entity, type_);
    entity->pos = pos;
    entity->vel = Vec2::New(0.0F, 0.0F);
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

} // namespace

extern const EntityArchetype kPotArchetype{
    .type_ = EntityType::Pot,
    .size = Vec2::New(8.0F, 7.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::AnthingExceptJumpOn,
    .death_sound_effect = SoundEffect::PotShatter,
    .on_death = OnDeathAsPot,
    .step_logic = StepEntityLogicAsPot,
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
    (void)graphics;
    (void)audio;
    (void)dt;

    Entity& pot = state.entity_manager.entities[entity_idx];
    if (pot.condition == EntityCondition::Dead) {
        return;
    }

    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(pot, state);
    if (!IsControlled(pot, state)) {
        return;
    }

    StepControlledPot(pot, control);
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
        SpawnEntityAtTopLeft(EntityType::GoldChunk, spawn_pos, state);
    } else if (RandInclusive(1, 6) == 1) {
        SpawnEntityAtTopLeft(EntityType::GoldNugget, spawn_pos, state);
    } else if (RandInclusive(1, 12) == 1) {
        SpawnEntityAtTopLeft(EntityType::EmeraldBig, spawn_pos, state);
    } else if (RandInclusive(1, 12) == 1) {
        SpawnEntityAtTopLeft(EntityType::SapphireBig, spawn_pos, state);
    } else if (RandInclusive(1, 12) == 1) {
        SpawnEntityAtTopLeft(EntityType::RubyBig, spawn_pos, state);
    } else if (RandInclusive(1, 6) == 1) {
        SpawnEntityAtTopLeft(EntityType::Spider, spider_spawn_pos, state);
    } else if (RandInclusive(1, 12) == 1) {
        SpawnEntityAtTopLeft(EntityType::Snake, snake_spawn_pos, state);
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
