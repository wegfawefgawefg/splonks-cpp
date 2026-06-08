#include "ents/block.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "particles/sprite_particle.hpp"
#include "state.hpp"
#include "controls.hpp"
#include "tile.hpp"

#include <cmath>
#include <memory>

namespace splonks::ents::block {

namespace {

constexpr float kControlledBlockMoveAcc = 0.18F;
constexpr float kControlledBlockSlideVel = 3.25F;
constexpr std::uint32_t kControlledBlockSlideCooldownFrames = 120;
constexpr float kBlockTrailSmokeDistInterval = 14.0F;
constexpr float kBlockPushAcc = 0.2F;

void StepControlledBlock(Ent& block, const controls::ControlIntent& control) {
    const sim::Scalar move_acc = sim::ToSimScalar(kControlledBlockMoveAcc);
    if (block.attack_delay_countdown > 0) {
        block.attack_delay_countdown -= 1;
    }

    if (control.left && !control.right) {
        block.acc.x -= move_acc;
        block.facing = Side::Left;
    } else if (control.right && !control.left) {
        block.acc.x += move_acc;
        block.facing = Side::Right;
    }

    if (control.attack_pressed && block.grounded && block.attack_delay_countdown == 0) {
        block.vel.x = block.facing == Side::Left
                          ? -sim::ToSimScalar(kControlledBlockSlideVel)
                          : sim::ToSimScalar(kControlledBlockSlideVel);
        block.attack_delay_countdown = kControlledBlockSlideCooldownFrames;
    }
}

void ControlEntAsBlock(
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

    Ent& block = state.ents.ents[ent_idx];
    if (block.condition == EntCondition::Dead) {
        return;
    }

    StepControlledBlock(block, controls::GetControlIntentForEnt(block, state));
}

void SpawnBlockDeathParticles(const Vec2& center, AFrameId anim_id, State& state) {
    for (int i = 0; i < 12; ++i) {
        SpriteParticle shard{};
        shard.aframe_animator = AFrameAnimator::New(anim_id);
        shard.draw_layer = DrawLayer::Foreground;
        shard.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(20, 42));
        shard.pos = center + Vec2::New(
                 rng::RandomFloat(-3.0F, 3.0F),
                 rng::RandomFloat(-3.0F, 3.0F)
             );
        const float size = rng::RandomFloat(3.0F, 7.0F);
        shard.size = Vec2::New(size, size);
        shard.rot = rng::RandomFloat(0.0F, 360.0F);
        shard.alpha = 1.0F;
        shard.vel = Vec2::New(
            rng::RandomFloat(-2.4F, 2.4F),
            rng::RandomFloat(-4.2F, -1.2F)
        );
        shard.svel = Vec2::New(0.0F, 0.0F);
        shard.rotvel = rng::RandomFloat(-0.7F, 0.7F);
        shard.alpha_vel = -0.018F;
        shard.acc = Vec2::New(0.0F, 0.18F);
        shard.sacc = Vec2::New(0.0F, 0.0F);
        shard.rotacc = 0.0F;
        shard.alpha_acc = -0.003F;
        state.particles.Add(std::move(shard));
    }
}

void SpawnBlockTrailSmoke(State& state, const Vec2& pos, Side facing) {
    SpriteParticle smoke{};
    smoke.aframe_animator = AFrameAnimator::New(aframe_ids::LittleSmoke);
    smoke.draw_layer = DrawLayer::Foreground;
    smoke.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(16, 28));
    smoke.pos = pos + Vec2::New(
             rng::RandomFloat(-1.0F, 1.0F),
             rng::RandomFloat(-1.0F, 1.0F)
         );
    const float size = rng::RandomFloat(3.0F, 5.5F);
    smoke.size = Vec2::New(size, size);
    smoke.rot = rng::RandomFloat(0.0F, 360.0F);
    smoke.alpha = rng::RandomFloat(0.55F, 0.85F);
    smoke.vel = Vec2::New(
        facing == Side::Right ? rng::RandomFloat(-0.9F, -0.2F)
                                     : rng::RandomFloat(0.2F, 0.9F),
        rng::RandomFloat(-0.8F, -0.2F)
    );
    smoke.svel = Vec2::New(rng::RandomFloat(0.01F, 0.03F), rng::RandomFloat(0.01F, 0.03F));
    smoke.rotvel = rng::RandomFloat(-0.2F, 0.2F);
    smoke.alpha_vel = -0.02F;
    smoke.acc = Vec2::New(0.0F, 0.01F);
    smoke.sacc = Vec2::New(0.0F, 0.0F);
    smoke.rotacc = 0.0F;
    smoke.alpha_acc = -0.003F;
    state.particles.Add(std::move(smoke));
}

Vec2 GetBlockTrailingBottomCorner(const Ent& block) {
    const AABB aabb = block.GetAABB();
    return block.facing == Side::Right
               ? Vec2::New(aabb.tl.x, aabb.br.y)
               : Vec2::New(aabb.br.x, aabb.br.y);
}

} // namespace

extern const EntSpec kBlockSpec{
    .type_ = EntType::Block,
    .size = EntSpecSize(static_cast<float>(kTileSize), static_cast<float>(kTileSize)),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = true,
    .hurt_on_contact = false,
    .crusher_pusher = true,
    .pushable = true,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .push_acc = sim::ToSimScalar(kBlockPushAcc),
    .draw_layer = DrawLayer::Middle,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::ExplosionOnly,
    .on_death = OnDeathAsBlock,
    .control_logic = ControlEntAsBlock,
    .step_logic = StepEntLogicAsBlock,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::CaveBlock),
};

bool TryApplyBlockContactToEnt(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    return common::TryApplyCrusherPusherContact(
        ent_idx,
        other_ent_idx,
        context,
        state,
        graphics,
        audio
    );
}

void StepEntLogicAsBlock(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    {
        Ent& ent = state.ents.ents[ent_idx];
        SetAnim(ent, state.stage.block_anim_id);
    }

    // TODO: if you hit the ground, do a clunky sound
    // TODO: if you hit something hard, do a block thunk sound

    Ent& ent = state.ents.ents[ent_idx];
    if (ent.grounded) {
        ent.travel_sound_countdown -= ent.dist_traveled_this_frame;
    }

    // TODO: extract into grounded movement sounds
    if (ent.grounded && ent.dist_traveled_this_frame > sim::Scalar::zero() &&
        ent.travel_sound_countdown < sim::Scalar::zero()) {
        ent.travel_sound_countdown =
            sim::Scalar::from_int(static_cast<std::int32_t>(kWalkerClimberTravelSoundDistInterval));
        const AudioAssetId sound =
            ent.travel_sound == TravelSound::One ? audio_asset_ids::BlockDrag1
                                                    : audio_asset_ids::BlockDrag2;
        (void)PlayEntSoundEmitter(state, ent, sound);
        ent.IncTravelSound();
    }

    if (ent.grounded && ent.dist_traveled_this_frame > sim::Scalar::zero()) {
        ent.counter_c -= sim::ToRenderScalar(ent.dist_traveled_this_frame);
        while (ent.counter_c <= 0.0F) {
            ent.counter_c += kBlockTrailSmokeDistInterval;
            SpawnBlockTrailSmoke(state, GetBlockTrailingBottomCorner(ent), ent.facing);
        }
    }
}

void OnDeathAsBlock(std::size_t ent_idx, State& state, Audio& audio) {
    (void)audio;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }
    Ent& block = state.ents.ents[ent_idx];
    SpawnBlockDeathParticles(block.GetCenter(), state.stage.block_anim_id, state);
}

/** generalize this to all square or rectangular ents somehow */
} // namespace splonks::ents::block
