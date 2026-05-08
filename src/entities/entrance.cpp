#include "entities/entrance.hpp"

#include "audio.hpp"
#include "audio_emitters.hpp"
#include "entity.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"

namespace splonks::entities::entrance {

namespace {

constexpr float kDoorRumbleVolumeScale = 0.85F;
constexpr float kLockVolumeScale = 0.95F;

FrameDataAnimator MakeEntranceAnimator() {
    FrameDataAnimator animator = FrameDataAnimator::New(frame_data_ids::Entrance);
    animator.loop = false;
    return animator;
}

void MaintainDoorSound(Entity& entrance, State& state) {
    (void)EnsureAttachedLoopingSoundEmitter(
        state,
        entrance.vid,
        entrance.vid,
        Vec2::New(0.0F, entrance.size.y * 0.5F),
        audio_asset_ids::BoulderRoll,
        kDoorRumbleVolumeScale
    );
}

void StopDoorSound(Entity& entrance, State& state, Audio& audio) {
    (void)StopOwnedSoundEmitter(
        state,
        audio,
        entrance.vid,
        audio_asset_ids::BoulderRoll,
        AudioEmitterPlaybackMode::Looping
    );
}

} // namespace

void StepEntityLogicAsEntrance(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    Entity& entrance = state.entity_manager.entities[entity_idx];

    if (entrance.counter_a <= 0.0F) {
        entrance.frame_data_animator.PlayOnce(frame_data_ids::Entrance);
        entrance.counter_a = 1.0F;
    }

    if (!entrance.frame_data_animator.IsFinished()) {
        MaintainDoorSound(entrance, state);
        return;
    }

    if (entrance.counter_b <= 0.0F) {
        StopDoorSound(entrance, state, audio);
        AudioEmitterPlayParams params;
        params.volume_scale = kLockVolumeScale;
        params.owner_entity_vid = entrance.vid;
        (void)PlayAttachedSoundEmitter(
            state,
            entrance.vid,
            Vec2::New(0.0F, entrance.size.y * 0.5F),
            audio_asset_ids::BoulderLatch,
            params
        );
        entrance.counter_b = 1.0F;
    }
}

extern const EntityArchetype kEntranceArchetype{
    .type_ = EntityType::Entrance,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_hit = false,
    .can_receive_projectile_contact = false,
    .can_be_picked_up = false,
    .affected_by_cobweb = false,
    .impassable = false,
    .can_be_hung_on = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Background,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .projectile_contact_damage_amount = 0,
    .can_apply_projectile_contact = false,
    .step_logic = StepEntityLogicAsEntrance,
    .replica_logic = StepEntityLogicAsEntrance,
    .alignment = Alignment::Neutral,
    .frame_data_animator = MakeEntranceAnimator(),
};

} // namespace splonks::entities::entrance
