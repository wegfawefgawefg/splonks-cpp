#include "entities/bones.hpp"

#include "audio_asset_id.hpp"
#include "entity/archetype.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "on_damage_effects.hpp"
#include "state.hpp"

namespace splonks::entities::bones {

namespace {

FrameDataAnimator MakeBonesAnimator() {
    FrameDataAnimator animator = FrameDataAnimator::New(frame_data_ids::Bones);
    animator.SetForcedFrame(0);
    animator.animate = false;
    animator.loop = false;
    return animator;
}

} // namespace

void OnDeathAsBones(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    const Entity& bones = state.entity_manager.entities[entity_idx];
    SpawnBreakawayContainerShards(bones.GetCenter(), state);
}

extern const EntityArchetype kBonesArchetype{
    .type_ = EntityType::Bones,
    .size = Vec2::New(12.0F, 6.0F),
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
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::AnthingExceptJumpOn,
    .death_sound = audio_asset_ids::BoxBreak,
    .on_death = OnDeathAsBones,
    .alignment = Alignment::Neutral,
    .frame_data_animator = MakeBonesAnimator(),
};

} // namespace splonks::entities::bones
