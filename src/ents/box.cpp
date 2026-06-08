#include "ents/box.hpp"
#include "on_damage_effects.hpp"

#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "state.hpp"
#include "controls.hpp"
#include "world_ops.hpp"

#include <cmath>

namespace splonks::ents::box {

namespace {

constexpr float kControlledMoveAcc = 0.1F;
constexpr float kControlledAirMoveAcc = 0.04F;
constexpr float kControlledJumpVel = 2.25F;
constexpr float kControlledSlideVel = 3.75F;
constexpr std::uint32_t kControlledSlideCooldownFrames = 120;
constexpr float kBoxBreakawayImpactSpeed = 2.0F;

Ent* SpawnEntAtTopLeft(EntType type_, const Vec2& pos, State& state) {
    return world_ops::SpawnEnt(state, type_, [pos](Ent& ent) {
        ent.SetRenderPos(pos);
        ent.vel = sim::Vec2::zero();
    });
}

EntType RandomTeleporterVariant(State& state) {
    return state.drng.RandomIntInclusive(1, 2) == 1
               ? EntType::Teleporter
               : EntType::TeleporterBackpack;
}

void StepControlledBox(Ent& box, const controls::ControlIntent& control) {
    const sim::Scalar move_acc =
        sim::ToSimScalar(box.grounded ? kControlledMoveAcc : kControlledAirMoveAcc);
    if (box.attack_delay_countdown > 0) {
        box.attack_delay_countdown -= 1;
    }

    if (control.left && !control.right) {
        box.acc.x -= move_acc;
        box.facing = Side::Left;
    } else if (control.right && !control.left) {
        box.acc.x += move_acc;
        box.facing = Side::Right;
    }

    if (control.jump_pressed && box.grounded) {
        box.vel.y = -sim::ToSimScalar(kControlledJumpVel);
        box.grounded = false;
    }

    if (control.attack_pressed && box.grounded && box.attack_delay_countdown == 0) {
        box.vel.x = box.facing == Side::Left ? -sim::ToSimScalar(kControlledSlideVel)
                                             : sim::ToSimScalar(kControlledSlideVel);
        box.attack_delay_countdown = kControlledSlideCooldownFrames;
    }
}

void ControlEntAsBox(
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

    Ent& box = state.ents.ents[ent_idx];
    if (box.condition == EntCondition::Dead) {
        return;
    }

    StepControlledBox(box, controls::GetControlIntentForEnt(box, state));
}

} // namespace

common::ContactResult BuildBoxImpactResolution(
    bool applied
) {
    return common::ContactResult{
        .blocks_movement = false,
        .stop_sweep = applied,
    };
}

common::ContactResult OnEntContactAsBox(
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
    return BuildBoxImpactResolution(TryApplyBoxImpact(ent_idx, context, state));
}

common::ContactResult OnTileContactAsBox(
    std::size_t ent_idx,
    const common::ContactContext& context,
    State& state
) {
    return BuildBoxImpactResolution(TryApplyBoxImpact(ent_idx, context, state));
}

extern const EntSpec kBoxSpec{
    .type_ = EntType::Box,
    .size = EntSpecSize(12.0F, 12.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .buoyancy = sim::ToSimScalar(1.0F),
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::AnthingExceptJumpOn,
    .collide_sound = audio_asset_ids::Thud,
    .death_sound = audio_asset_ids::BoxBreak,
    .on_death = OnDeathAsBox,
    .control_logic = ControlEntAsBox,
    .step_logic = StepEntLogicAsBox,
    .on_ent_contact = OnEntContactAsBox,
    .on_tile_contact = OnTileContactAsBox,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Box),
};

void StepEntLogicAsBox(
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

void OnDeathAsBox(std::size_t ent_idx, State& state, Audio& audio) {
    (void)audio;

    const Ent& box = state.ents.ents[ent_idx];

    const Vec2 spawn_pos = box.GetRenderPos();
    SpawnBreakawayContainerShards(box.GetRenderCenter(), state);

    // Matches ClassicHD's actual open-crate roll order, with unimplemented Shotgun
    // intentionally substituted by Pistol for now. HD/2 use the same common
    // tail shape: 1/2 RopePile, otherwise guaranteed BombBag.
    if (state.drng.RandomIntInclusive(1, 500) == 1) {
        SpawnEntAtTopLeft(EntType::JetPack, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 200) == 1) {
        SpawnEntAtTopLeft(EntType::Cape, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 100) == 1) {
        SpawnEntAtTopLeft(EntType::Pistol, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 100) == 1) {
        SpawnEntAtTopLeft(EntType::Mattock, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 100) == 1) {
        SpawnEntAtTopLeft(RandomTeleporterVariant(state), spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 90) == 1) {
        SpawnEntAtTopLeft(EntType::Gloves, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 90) == 1) {
        SpawnEntAtTopLeft(EntType::Spectacles, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 80) == 1) {
        SpawnEntAtTopLeft(EntType::WebCannon, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 80) == 1) {
        SpawnEntAtTopLeft(EntType::Pistol, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 80) == 1) {
        SpawnEntAtTopLeft(EntType::Mitt, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 60) == 1) {
        SpawnEntAtTopLeft(EntType::Paste, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 60) == 1) {
        SpawnEntAtTopLeft(EntType::SpringShoes, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 60) == 1) {
        SpawnEntAtTopLeft(EntType::SpikeShoes, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 60) == 1) {
        SpawnEntAtTopLeft(EntType::Machete, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 40) == 1) {
        SpawnEntAtTopLeft(EntType::BombBox, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 40) == 1) {
        SpawnEntAtTopLeft(EntType::Bow, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 20) == 1) {
        SpawnEntAtTopLeft(EntType::Compass, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 10) == 1) {
        SpawnEntAtTopLeft(EntType::Parachute, spawn_pos, state);
    } else if (state.drng.RandomIntInclusive(1, 2) == 1) {
        SpawnEntAtTopLeft(EntType::RopePile, spawn_pos, state);
    } else {
        SpawnEntAtTopLeft(EntType::BombBag, spawn_pos, state);
    }
}

bool TryApplyBoxImpact(
    std::size_t ent_idx,
    const common::ContactContext& context,
    State& state
) {
    if (ent_idx >= state.ents.ents.size()) {
        return false;
    }

    Ent& box = state.ents.ents[ent_idx];
    if (box.type_ != EntType::Box || box.condition == EntCondition::Dead) {
        return false;
    }
    if (context.phase != common::ContactPhase::AttemptedBlocked || !context.has_impact) {
        return false;
    }
    if (std::abs(context.impact_velocity) <= kBoxBreakawayImpactSpeed) {
        return false;
    }

    box.health = 0;
    return true;
}

} // namespace splonks::ents::box
