#include "entities/dice.hpp"

#include "entity/archetype.hpp"
#include "entity/core_types.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"
#include "state.hpp"
#include "utils.hpp"

#include <cmath>

namespace splonks::entities::dice {

namespace {

constexpr float kRollingState = 1.0F;
constexpr float kSettleSpeed = 0.2F;

int RollDicePairTotal(State& state) {
    return state.drng.RandomIntInclusive(1, 6) +
           state.drng.RandomIntInclusive(1, 6);
}

void StepEntityLogicAsDice(
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

    Entity& dice = state.entity_manager.entities[entity_idx];
    if (!dice.active || dice.type_ != EntityType::Dice || dice.counter_b != kRollingState) {
        return;
    }

    if (!dice.grounded || std::abs(dice.vel.x) > kSettleSpeed ||
        std::abs(dice.vel.y) > kSettleSpeed) {
        dice.counter_a = static_cast<float>(RollDicePairTotal(state));
        dice.rotation = std::fmod(dice.rotation + 24.0F + std::abs(dice.vel.x) * 8.0F, 360.0F);
        return;
    }

    dice.counter_b = 0.0F;
    dice.rotation = 0.0F;
    dice.vel = Vec2::New(0.0F, 0.0F);
}

} // namespace

extern const EntityArchetype kDiceArchetype{
    .type_ = EntityType::Dice,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .projectile_contact_damage_amount = 0,
    .can_apply_projectile_contact = false,
    .step_logic = StepEntityLogicAsDice,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Dice),
};

} // namespace splonks::entities::dice
