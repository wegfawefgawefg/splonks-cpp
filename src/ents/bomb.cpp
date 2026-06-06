#include "ents/bomb.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "state.hpp"

#include <cmath>

namespace splonks::ents::bomb {

namespace {

constexpr float kBombRotationDegreesPerPixel = 24.0F;
constexpr float kStickyBombFlag = 1.0F;
constexpr float kLitBombSelfLight = 0.2F;
constexpr float kLitBombLightStrength = 0.55F;
constexpr int kLitBombLightRadius = 5;
constexpr Color3 kLitBombLightColor = Color3::New(1.0F, 0.48F, 0.16F);

bool IsStickyBomb(const Ent& bomb) {
    return bomb.counter_b >= 0.5F;
}

AFrameId GetBombIdleAnim(const Ent& bomb) {
    return IsStickyBomb(bomb) ? aframe_ids::StickyGrenade : aframe_ids::Grenade;
}

AFrameId GetBombLiveAnim(const Ent& bomb) {
    return IsStickyBomb(bomb) ? aframe_ids::StickyLiveGrenade : aframe_ids::LiveGrenade;
}

void StickBombInPlace(Ent& bomb) {
    bomb.vel = Vec2::New(0.0F, 0.0F);
    bomb.acc = Vec2::New(0.0F, 0.0F);
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

    bomb.pos = attached->pos + Vec2::New(
        static_cast<float>(bomb.point_a.x),
        static_cast<float>(bomb.point_a.y)
    );
    bomb.vel = Vec2::New(0.0F, 0.0F);
    bomb.acc = Vec2::New(0.0F, 0.0F);
}

void UpdateBombRotation(Ent& bomb) {
    if (bomb.held_by_vid.has_value() || bomb.attach_mode != AttachMode::None) {
        return;
    }
    if (std::abs(bomb.vel.x) < 0.01F) {
        return;
    }

    bomb.rotation += bomb.vel.x * kBombRotationDegreesPerPixel;
    while (bomb.rotation >= 360.0F) {
        bomb.rotation -= 360.0F;
    }
    while (bomb.rotation < 0.0F) {
        bomb.rotation += 360.0F;
    }
}

void UpdateBombFuseLight(Ent& bomb) {
    if (bomb.counter_a <= 0.0F) {
        bomb.self_light = 0.0F;
        bomb.light_strength = 0.0F;
        bomb.light_color = Color3::White();
        bomb.light_radius = 0;
        return;
    }

    bomb.self_light = kLitBombSelfLight;
    bomb.light_strength = kLitBombLightStrength;
    bomb.light_color = kLitBombLightColor;
    bomb.light_radius = kLitBombLightRadius;
}

} // namespace

extern const EntSpec kBombSpec{
    .type_ = EntType::Bomb,
    .size = Vec2::New(8.0F, 6.0F),
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
    bomb.counter_b = kStickyBombFlag;
    SetAnim(bomb, aframe_ids::StickyGrenade);
}

void OnDeathAsBomb(std::size_t ent_idx, State& state, Audio& audio) {
    common::OnDeathAsExplosion(ent_idx, state, audio);
}

void OnUseAsBomb(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)graphics;
    (void)audio;
    Ent& bomb = state.ents.ents[ent_idx];
    if (!bomb.use_state.pressed || bomb.counter_a > 0.0F) {
        return;
    }

    bomb.counter_a = 144.0F;
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
    if (bomb.counter_a > 0.0F) {
        bomb.counter_a -= 1.0F;
        if (bomb.counter_a <= 0.0F) {
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
        static_cast<int>(std::lround(bomb.pos.x - other.pos.x)),
        static_cast<int>(std::lround(bomb.pos.y - other.pos.y))
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
