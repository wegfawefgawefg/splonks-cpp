#include "ents/bones.hpp"

#include "audio_asset_id.hpp"
#include "ent/spec.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "on_damage_effects.hpp"
#include "state.hpp"

namespace splonks::ents::bones {

namespace {

AFrameAnimator MakeBonesAnimator() {
    AFrameAnimator animator = AFrameAnimator::New(aframe_ids::Bones);
    animator.SetForcedFrame(0);
    animator.animate = false;
    animator.loop = false;
    return animator;
}

} // namespace

void OnDeathAsBones(std::size_t ent_idx, State& state, Audio& audio) {
    (void)audio;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    const Ent& bones = state.ents.ents[ent_idx];
    SpawnBreakawayContainerShards(ToFVec2(bones.GetSimCenter()), state);
}

extern const EntSpec kBonesSpec{
    .type_ = EntType::Bones,
    .size = EntSpecSize(12.0F, 6.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::AnthingExceptJumpOn,
    .death_sound = audio_asset_ids::BoxBreak,
    .on_death = OnDeathAsBones,
    .alignment = Alignment::Neutral,
    .aframe_animator = MakeBonesAnimator(),
};

} // namespace splonks::ents::bones
