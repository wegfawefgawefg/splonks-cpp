#include "ents/jetpack.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "particles/sprite_particle.hpp"
#include "state.hpp"
#include "world_ops.hpp"

#include <memory>

namespace splonks::ents::jetpack {

namespace {

void SpawnJetpackSmoke(State& state, const Vec2& pos) {
    for (int i = 0; i < 16; ++i) {
        const float vel = rng::RandomFloat(0.1F, 0.5F);
        const float svel = rng::RandomFloat(vel * 0.1F, vel * 1.0F);
        const float sacc = rng::RandomFloat(vel * 0.01F, vel * 0.02F);
        SpriteParticle effect{};
        effect.aframe_animator = AFrameAnimator::New(aframe_ids::BigSmoke);
        effect.draw_layer = DrawLayer::Foreground;
        effect.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(0, 32));
        effect.pos = pos;
        effect.size = Vec2::New(1.0F, 1.0F) * 2.0F;
        effect.rot = rng::RandomFloat(0.0F, 360.0F);
        effect.alpha = 1.0F;
        effect.vel = Vec2::New(0.0F, rng::RandomFloat(0.0F, 0.3F));
        effect.svel = Vec2::New(svel, svel);
        effect.rotvel = rng::RandomFloat(-0.2F, -0.01F);
        effect.alpha_vel = vel * 0.001F;
        effect.acc = Vec2::New(0.0F, 0.0F);
        effect.sacc = Vec2::New(sacc, sacc);
        effect.rotacc = 0.0F;
        effect.alpha_acc = 0.0F;
        state.particles.Add(std::move(effect));
    }
}

} // namespace

extern const EntSpec kJetPackSpec{
    .type_ = EntType::JetPack,
    .size = Vec2::New(8.0F, 8.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_go_on_back = true,
    .can_be_stunned = false,
    .predict_local_attach_use = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .counter_a = kFuel,
    .damage_vuln = DamageVuln::CrushingSpikesAndExplosion,
    .on_death = OnDeathAsJetpack,
    .on_damage = OnDamageAsJetpack,
    .on_use = OnUseAsJetpack,
    .step_logic = StepEntLogicAsJetpack,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Jetpack),
};

void OnDeathAsJetpack(std::size_t ent_idx, State& state, Audio& audio) {
    common::OnDeathAsExplosion(ent_idx, state, audio);
}

EntDamageEffectResult OnDamageAsJetpack(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
) {
    (void)amount;
    (void)damage_applied;

    if (damage_type != DamageType::IgnitingAttack) {
        return EntDamageEffectResult::None;
    }
    if (ent_idx >= state.ents.ents.size()) {
        return EntDamageEffectResult::None;
    }

    Ent& jetpack = state.ents.ents[ent_idx];
    if (!jetpack.active || jetpack.condition == EntCondition::Dead) {
        return EntDamageEffectResult::None;
    }

    jetpack.health = 0;
    common::DieIfDead(ent_idx, state, audio);
    return EntDamageEffectResult::Consumed;
}

void OnUseAsJetpack(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)graphics;
    (void)audio;
    Ent& jetpack = state.ents.ents[ent_idx];
    if (jetpack.use_state.down == false) {
        return;
    }

    const std::optional<VID> held_by_vid = jetpack.held_by_vid;
    bool refill_fuel = false;
    if (held_by_vid.has_value()) {
        if (Ent* const holder = state.ents.GetEntMut(*held_by_vid)) {
            if (holder->grounded || holder->IsClimbing() || holder->IsHanging() ||
                holder->jumped_this_frame) {
                refill_fuel = true;
            }
        }
    }
    if (refill_fuel) {
        jetpack.counter_a = kFuel;
    }
    if (jetpack.counter_a <= 0.0F) {
        return;
    }

    if (held_by_vid.has_value()) {
        if (Ent* const holder = state.ents.GetEntMut(*held_by_vid)) {
            const float jetpack_max_upspeed = -2.0F;
            if (holder->vel.y > jetpack_max_upspeed) {
                holder->acc.y = -0.6F;
                holder->vel.y = Min(holder->vel.y, jetpack_max_upspeed);
            }
            if (!holder->IsHanging()) {
                TrySetAnim(*holder, EntDisplayState::Neutral);
            }
        }
    }

    jetpack.travel_sound_countdown -= 1.0F;
    if (jetpack.travel_sound_countdown < 0.0F) {
        jetpack.travel_sound_countdown = kTravelSoundDistInterval;
        const AudioAssetId sound_effect =
            jetpack.travel_sound == TravelSound::One ? audio_asset_ids::Jetpack1
                                                     : audio_asset_ids::Jetpack2;
        (void)PlayEntSoundEmitter(state, jetpack, sound_effect);
        jetpack.IncTravelSound();
    }
    jetpack.counter_a -= 1.0F;

    const Vec2 center = jetpack.GetCenter();
    SpawnJetpackSmoke(state, center + Vec2::New(3.0F, 3.0F));
    SpawnJetpackSmoke(state, center + Vec2::New(-3.0F, 3.0F));
}

/** jetpack goes up by default, and idles if it hits the ceiling.
 *  If the jetpack detects the player is beneath it,
 *  It checks if the player is within some dist below, some dist left or right.
 *      if yes, move towards the player right now.
 *  If no, give up and fly back to the ceiling.
 */
void StepEntLogicAsJetpack(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    Ent& jetpack = state.ents.ents[ent_idx];
    if (jetpack.attach_mode == AttachMode::Back) {
        AFrameId equipped_anim = aframe_ids::JetpackBack;
        if (jetpack.held_by_vid.has_value()) {
            if (const Ent* const holder = state.ents.GetEnt(*jetpack.held_by_vid)) {
                if (holder->IsHanging()) {
                    equipped_anim = aframe_ids::JetpackSide;
                } else if (holder->IsClimbing()) {
                    equipped_anim = aframe_ids::JetpackBack;
                }
            }
        }
        SetAnim(jetpack, equipped_anim);
    } else if (jetpack.held_by_vid.has_value()) {
        SetAnim(jetpack, aframe_ids::JetpackSide);
    } else {
        SetAnim(jetpack, aframe_ids::Jetpack);
    }

    if (jetpack.use_state.down == false) {
        jetpack.travel_sound_countdown = 0.0F;
    }
}

/** generalize this to all square or rectangular ents somehow */
} // namespace splonks::ents::jetpack
