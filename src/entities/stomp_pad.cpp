#include "entities/stomp_pad.hpp"

#include "entities/common.hpp"
#include "frame_data_id.hpp"

namespace splonks::entities::stomp_pad {

void SetEntityStompPad(Entity& entity) {
    entity.Reset();
    entity.type_ = EntityType::StompPad;
    entity.super_state = EntitySuperState::Idle;
    entity.state = EntityState::Idle;
    entity.display_state = EntityDisplayState::Neutral;
    entity.size = Vec2::New(8.0F, 7.0F);
    entity.health = 1000;
    entity.damage_vulnerability = DamageVulnerability::Vulnerable;
    entity.has_physics = false;
    entity.can_collide = true;
    entity.can_be_picked_up = false;
    entity.impassable = false;
    entity.hurt_on_contact = true;
    entity.facing = LeftOrRight::Left;
    entity.draw_layer = DrawLayer::Foreground;
    entity.can_be_stunned = false;
    entity.alignment = Alignment::Enemy;
    entity.frame_data_animator.SetAnimation(frame_data_ids::Pot);
}

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
