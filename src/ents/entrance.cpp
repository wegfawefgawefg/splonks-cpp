#include "ents/entrance.hpp"

#include "audio.hpp"
#include "audio_emitters.hpp"
#include "ent.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "state.hpp"

namespace splonks::ents::entrance {

namespace {

constexpr float kDoorRumbleVolumeScale = 0.85F;
constexpr float kLockVolumeScale = 0.95F;

AFrameAnimator MakeEntranceAnimator() {
    AFrameAnimator animator = AFrameAnimator::New(aframe_ids::Entrance);
    animator.loop = false;
    return animator;
}

void MaintainDoorSound(Ent& entrance, State& state) {
    (void)EnsureAttachedLoopingSoundEmitter(
        state,
        entrance.vid,
        entrance.vid,
        Vec2::New(0.0F, entrance.GetSize().y * 0.5F),
        audio_asset_ids::BoulderRoll,
        kDoorRumbleVolumeScale
    );
}

void StopDoorSound(Ent& entrance, State& state, Audio& audio) {
    (void)StopOwnedSoundEmitter(
        state,
        audio,
        entrance.vid,
        audio_asset_ids::BoulderRoll,
        AudioEmitterPlaybackMode::Looping
    );
}

} // namespace

void StepEntLogicAsEntrance(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    Ent& entrance = state.ents.ents[ent_idx];

    if (entrance.counter_a <= sim::Scalar::zero()) {
        entrance.aframe_animator.PlayOnce(aframe_ids::Entrance);
        entrance.counter_a = sim::Scalar::from_int(1);
    }

    if (!entrance.aframe_animator.IsFinished()) {
        MaintainDoorSound(entrance, state);
        return;
    }

    if (entrance.counter_b <= sim::Scalar::zero()) {
        StopDoorSound(entrance, state, audio);
        AudioEmitterPlayParams params;
        params.volume_scale = kLockVolumeScale;
        params.owner_ent_vid = entrance.vid;
        (void)PlayAttachedSoundEmitter(
            state,
            entrance.vid,
            Vec2::New(0.0F, entrance.GetSize().y * 0.5F),
            audio_asset_ids::BoulderLatch,
            params
        );
        entrance.counter_b = sim::Scalar::from_int(1);
    }
}

extern const EntSpec kEntranceSpec{
    .type_ = EntType::Entrance,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_hit = false,
    .can_receive_proj_contact = false,
    .can_be_picked_up = false,
    .affected_by_cobweb = false,
    .impassable = false,
    .can_be_hung_on = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Background,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .proj_contact_damage_amount = 0,
    .can_apply_proj_contact = false,
    .step_logic = StepEntLogicAsEntrance,
    .alignment = Alignment::Neutral,
    .aframe_animator = MakeEntranceAnimator(),
};

} // namespace splonks::ents::entrance
