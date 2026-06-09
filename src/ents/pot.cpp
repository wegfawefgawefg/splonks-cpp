#include "ents/pot.hpp"
#include "on_damage_effects.hpp"

#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "state.hpp"
#include "controls.hpp"
#include "world_ops.hpp"

#include <cmath>

namespace splonks::ents::pot {

namespace {

constexpr float kControlledMoveAcc = 0.07F;
constexpr float kControlledAirMoveAcc = 0.04F;
constexpr float kControlledJumpVel = 3.0F;
constexpr float kControlledSlideVel = 3.0F;
constexpr std::uint32_t kControlledSlideCooldownFrames = 120;
constexpr float kPotBreakawayImpactSpeed = 1.0F;

Ent* SpawnEntAtTopLeft(EntType type_, sim::FxVec2 pos, State& state) {
    return world_ops::SpawnEnt(state, type_, [pos](Ent& ent) {
        ent.pos = pos;
        ent.vel = sim::FxVec2::zero();
    });
}

void StepControlledPot(Ent& pot, const controls::ControlIntent& control) {
    if (pot.attack_delay_countdown > 0) {
        pot.attack_delay_countdown -= 1;
    }

    if (control.left && !control.right) {
        pot.acc.x -= ToFxScalar(pot.grounded ? kControlledMoveAcc : kControlledAirMoveAcc);
        pot.facing = Side::Left;
    } else if (control.right && !control.left) {
        pot.acc.x += ToFxScalar(pot.grounded ? kControlledMoveAcc : kControlledAirMoveAcc);
        pot.facing = Side::Right;
    }

    if (control.jump_pressed && pot.grounded) {
        pot.vel.y = -ToFxScalar(kControlledJumpVel);
        pot.grounded = false;
    }

    if (control.attack_pressed && pot.grounded && pot.attack_delay_countdown == 0) {
        pot.vel.x = pot.facing == Side::Left ? -ToFxScalar(kControlledSlideVel)
                                             : ToFxScalar(kControlledSlideVel);
        pot.attack_delay_countdown = kControlledSlideCooldownFrames;
    }
}

void ControlEntAsPot(
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

    Ent& pot = state.ents.ents[ent_idx];
    if (pot.condition == EntCondition::Dead) {
        return;
    }

    StepControlledPot(pot, controls::GetControlIntentForEnt(pot, state));
}

} // namespace

common::ContactResult BuildPotImpactResolution(
    bool applied
) {
    return common::ContactResult{
        .blocks_movement = false,
        .stop_sweep = applied,
    };
}

common::ContactResult OnEntContactAsPot(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)other_ent_idx;
    (void)graphics;
    (void)audio;
    if (!context.mover_vid.has_value() || *context.mover_vid != state.ents.ents[ent_idx].vid) {
        return common::ContactResult{};
    }
    return BuildPotImpactResolution(TryApplyPotImpact(ent_idx, context, state));
}

common::ContactResult OnTileContactAsPot(
    std::size_t ent_idx,
    const common::ContactContext& context,
    State& state
) {
    return BuildPotImpactResolution(TryApplyPotImpact(ent_idx, context, state));
}

extern const EntSpec kPotSpec{
    .type_ = EntType::Pot,
    .size = EntSpecSize(8.0F, 7.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .buoyancy = ToFxScalar(0.35F),
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::AnthingExceptJumpOn,
    .death_sound = audio_asset_ids::PotShatter,
    .on_death = OnDeathAsPot,
    .control_logic = ControlEntAsPot,
    .step_logic = StepEntLogicAsPot,
    .on_ent_contact = OnEntContactAsPot,
    .on_tile_contact = OnTileContactAsPot,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Pot),
};

void StepEntLogicAsPot(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)ent_idx;
    (void)state;
    (void)graphics;
    (void)audio;
    (void)dt;
}

void OnDeathAsPot(std::size_t ent_idx, State& state, Audio& audio) {
    (void)audio;

    const Ent& pot = state.ents.ents[ent_idx];

    const sim::FxVec2 spawn_pos = pot.pos;
    SpawnBreakawayContainerShards(ToFVec2(pot.GetSimCenter()), state);
    const sim::FxVec2 spider_spawn_pos = pot.pos + sim::PixelVec2(-8, -8);

    sim::FxVec2 snake_spawn_pos = pot.pos + sim::PixelVec2(-8, -8);
    if (pot.point_a.x < 0) {
        snake_spawn_pos = pot.pos + sim::PixelVec2(0, -8);
    } else if (pot.point_a.x > 0) {
        snake_spawn_pos = pot.pos + sim::PixelVec2(-16, -8);
    }

    if (state.drng.RandomIntInclusive(1, 3) == 1) {
        SpawnEntAtTopLeft(EntType::GoldChunk, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 6) == 1) {
        SpawnEntAtTopLeft(EntType::GoldNugget, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 12) == 1) {
        SpawnEntAtTopLeft(EntType::EmeraldBig, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 12) == 1) {
        SpawnEntAtTopLeft(EntType::SapphireBig, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 12) == 1) {
        SpawnEntAtTopLeft(EntType::RubyBig, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 6) == 1) {
        SpawnEntAtTopLeft(EntType::Spider, spider_spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 12) == 1) {
        SpawnEntAtTopLeft(EntType::Snake, snake_spawn_pos, state);
    }
}

bool TryApplyPotImpact(
    std::size_t ent_idx,
    const common::ContactContext& context,
    State& state
) {
    if (ent_idx >= state.ents.ents.size()) {
        return false;
    }

    Ent& pot = state.ents.ents[ent_idx];
    if (pot.type_ != EntType::Pot || pot.condition == EntCondition::Dead) {
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

} // namespace splonks::ents::pot
