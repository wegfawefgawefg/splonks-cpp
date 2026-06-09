#include "ents/bomb.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"
#include "sim/fxp.hpp"
#include "state.hpp"

namespace splonks::ents::bomb {

namespace {

constexpr float kBombRotationDegreesPerPixel = 24.0F;
constexpr int kStickyBombFlag = 1;
constexpr float kLitBombSelfLight = 0.2F;
constexpr float kLitBombLightStrength = 0.55F;
constexpr int kLitBombLightRadius = 5;
constexpr Color3 kLitBombLightColor = Color3::New(1.0F, 0.48F, 0.16F);

bool IsStickyBomb(const Ent& bomb) {
    return bomb.counter_b >= sim::ToSimScalar(0.5F);
}

AFrameId GetBombIdleAnim(const Ent& bomb) {
    return IsStickyBomb(bomb) ? aframe_ids::StickyGrenade : aframe_ids::Grenade;
}

AFrameId GetBombLiveAnim(const Ent& bomb) {
    return IsStickyBomb(bomb) ? aframe_ids::StickyLiveGrenade : aframe_ids::LiveGrenade;
}

void StickBombInPlace(Ent& bomb) {
    bomb.vel = sim::FxVec2::zero();
    bomb.acc = sim::FxVec2::zero();
    bomb.has_physics = false;
    bomb.thrown_by.reset();
    bomb.thrown_immunity_timer = 0;
    bomb.proj_contact_timer = 0;
    bomb.can_apply_proj_contact = false;
}

void UpdateStickyBombAttach(Ent& bomb, State& state) {
    if (!IsStickyBomb(bomb) || !bomb.ent_a.has_value()) {
        return;
    }

    const Ent* const attached = state.ents.GetEnt(*bomb.ent_a);
    if (attached == nullptr || !attached->active) {
        bomb.ent_a.reset();
        bomb.has_physics = true;
        return;
    }

    bomb.pos = attached->pos + sim::PixelVec2(bomb.point_a.x, bomb.point_a.y);
    bomb.vel = sim::FxVec2::zero();
    bomb.acc = sim::FxVec2::zero();
}

void UpdateBombRotation(Ent& bomb) {
    if (bomb.held_by_vid.has_value() || bomb.attach_mode != AttachMode::None) {
        return;
    }
    if (bomb.vel.x.abs() < sim::ToSimScalar(0.01F)) {
        return;
    }

    float rotation = sim::ToRenderScalar(bomb.rotation) +
                     sim::ToRenderScalar(bomb.vel.x) * kBombRotationDegreesPerPixel;
    while (rotation >= 360.0F) {
        rotation -= 360.0F;
    }
    while (rotation < 0.0F) {
        rotation += 360.0F;
    }
    bomb.rotation = sim::ToSimScalar(rotation);
}

void UpdateBombFuseLight(Ent& bomb) {
    if (bomb.counter_a <= sim::Scalar::zero()) {
        bomb.self_light = sim::Scalar::zero();
        bomb.light_strength = sim::Scalar::zero();
        bomb.light_color = sim::ToSimColor3(Color3::White());
        bomb.light_radius = 0;
        return;
    }

    bomb.self_light = sim::ToSimScalar(kLitBombSelfLight);
    bomb.light_strength = sim::ToSimScalar(kLitBombLightStrength);
    bomb.light_color = sim::ToSimColor3(kLitBombLightColor);
    bomb.light_radius = kLitBombLightRadius;
}

} // namespace

extern const EntSpec kBombSpec{
    .type_ = EntType::Bomb,
    .size = EntSpecSize(8.0F, 6.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .affected_by_ground_friction = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::CrushingSpikesAndExplosion,
    .on_death = OnDeathAsBomb,
    .on_use = OnUseAsBomb,
    .step_logic = StepEntLogicAsBomb,
    .on_ent_contact = OnEntContactAsBomb,
    .on_tile_contact = OnTileContactAsBomb,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Grenade),
};

void MarkBombSticky(Ent& bomb) {
    bomb.counter_b = sim::Scalar::from_int(kStickyBombFlag);
    SetAnim(bomb, aframe_ids::StickyGrenade);
}

void OnDeathAsBomb(std::size_t ent_idx, State& state, Audio& audio) {
    common::OnDeathAsExplosion(ent_idx, state, audio);
}

void OnUseAsBomb(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)graphics;
    (void)audio;
    Ent& bomb = state.ents.ents[ent_idx];
    if (!bomb.use_state.pressed || bomb.counter_a > sim::Scalar::zero()) {
        return;
    }

    bomb.counter_a = sim::Scalar::from_int(144);
    SetAnim(bomb, GetBombLiveAnim(bomb));

    if (bomb.use_state.source == AttachMode::None) {
        StopUsingEnt(bomb);
    }
}

void StepEntLogicAsBomb(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    Ent& bomb = state.ents.ents[ent_idx];
    UpdateStickyBombAttach(bomb, state);

    // if bomb is in winding up
    // set anim and display state
    // start decrementing the counter
    if (bomb.counter_a > sim::Scalar::zero()) {
        bomb.counter_a -= sim::Scalar::from_int(1);
        if (bomb.counter_a <= sim::Scalar::zero()) {
            UpdateBombFuseLight(bomb);
            bomb.health = 0;
            common::DieIfDead(ent_idx, state, audio);
            return;
        }
    }

    UpdateBombFuseLight(bomb);
    UpdateBombRotation(bomb);
}

common::ContactResult OnEntContactAsBomb(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)graphics;
    (void)audio;
    Ent& bomb = state.ents.ents[ent_idx];
    const Ent& other = state.ents.ents[other_ent_idx];
    if (!IsStickyBomb(bomb) || bomb.ent_a.has_value() ||
        context.phase != common::ContactPhase::SweptEntered) {
        return {};
    }
    if (bomb.thrown_by.has_value() && other.vid == *bomb.thrown_by) {
        return {};
    }
    if (other.held_by_vid.has_value() && bomb.thrown_by.has_value() &&
        *other.held_by_vid == *bomb.thrown_by) {
        return {};
    }
    if (!other.can_collide || other.type_ == EntType::Bomb) {
        return {};
    }

    bomb.ent_a = other.vid;
    bomb.point_a = IVec2::New(
        (bomb.pos.x - other.pos.x).round_int(),
        (bomb.pos.y - other.pos.y).round_int()
    );
    StickBombInPlace(bomb);
    return {};
}

common::ContactResult OnTileContactAsBomb(
    std::size_t ent_idx,
    const common::ContactContext& context,
    State& state
) {
    Ent& bomb = state.ents.ents[ent_idx];
    if (!IsStickyBomb(bomb) || bomb.ent_a.has_value() ||
        context.phase != common::ContactPhase::AttemptedBlocked || !context.has_impact) {
        return {};
    }

    StickBombInPlace(bomb);
    return {.stop_sweep = true};
}

/** generalize this to all square or rectangular ents somehow */
} // namespace splonks::ents::bomb
