#include "entities/stomp_pad.hpp"

#include "entity_archetype.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"

namespace splonks::entities::stomp_pad {

extern const EntityArchetype kStompPadArchetype{
    .type_ = EntityType::StompPad,
    .size = Vec2::New(8.0F, 7.0F),
    .health = 1000,
    .has_physics = false,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .state = EntityState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Pot),
};

void StepEntityLogicAsStompPad(std::size_t entity_idx, State& state, Audio& audio) {
    (void)entity_idx;
    (void)state;
    (void)audio;
}

void StepEntityPhysicsAsStompPad(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)entity_idx;
    (void)state;
    (void)graphics;
    (void)audio;
    (void)dt;
}

} // namespace splonks::entities::stomp_pad
