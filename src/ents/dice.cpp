#include "ents/dice.hpp"

#include "ent/spec.hpp"
#include "ent/core_types.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"
#include "state.hpp"
#include "utils.hpp"

#include <cmath>

namespace splonks::ents::dice {

namespace {

constexpr int kRollingState = 1;
constexpr float kSettleSpeed = 0.2F;

float WrapRotationDegrees(float degrees) {
    while (degrees >= 360.0F) {
        degrees -= 360.0F;
    }
    while (degrees < 0.0F) {
        degrees += 360.0F;
    }
    return degrees;
}

int RollDicePairTotal(State& state) {
    return state.drng.RandomIntInclusive(1, 6) +
           state.drng.RandomIntInclusive(1, 6);
}

void StepEntLogicAsDice(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& dice = state.ents.ents[ent_idx];
    if (!dice.active || dice.type_ != EntType::Dice ||
        dice.counter_b != FxScalar::from_int(kRollingState)) {
        return;
    }

    if (!dice.grounded || dice.vel.x.abs() > ToFxScalar(kSettleSpeed) ||
        dice.vel.y.abs() > ToFxScalar(kSettleSpeed)) {
        dice.counter_a = FxScalar::from_int(RollDicePairTotal(state));
        dice.rotation = ToFxScalar(
            WrapRotationDegrees(ToFloat(dice.rotation) + 24.0F +
                                ToFloat(dice.vel.x.abs()) * 8.0F)
        );
        return;
    }

    dice.counter_b = FxScalar::zero();
    dice.rotation = FxScalar::zero();
    dice.vel = FxVec2::zero();
}

} // namespace

extern const EntSpec kDiceSpec{
    .type_ = EntType::Dice,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .proj_contact_damage_amount = 0,
    .can_apply_proj_contact = false,
    .step_logic = StepEntLogicAsDice,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Dice),
};

} // namespace splonks::ents::dice
