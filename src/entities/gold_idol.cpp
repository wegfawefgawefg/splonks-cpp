#include "entities/gold_idol.hpp"

#include "audio.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"

namespace splonks::entities::gold_idol {

void SetEntityGoldIdol(Entity& entity) {
    entity.Reset();
    entity.type_ = EntityType::GoldIdol;
    entity.super_state = EntitySuperState::Idle;
    entity.state = EntityState::Idle;
    TrySetDisplayState(entity, EntityDisplayState::Neutral);
    entity.size = Vec2::New(12.0F, 12.0F);
    entity.health = 1;
    entity.damage_vulnerability = DamageVulnerability::Immune;
    entity.has_physics = true;
    entity.can_collide = true;
    entity.can_be_picked_up = true;
    entity.impassable = false;
    entity.hurt_on_contact = false;
    entity.facing = LeftOrRight::Left;
    entity.draw_layer = DrawLayer::Foreground;
    entity.can_be_stunned = false;
    entity.alignment = Alignment::Neutral;
    entity.frame_data_animator.SetAnimation(HashFrameDataIdConstexpr("gold_idol"));
}

void StepEntityLogicAsGoldIdol(std::size_t entity_idx, State& state, Audio& audio) {
    (void)entity_idx;
    (void)state;
    (void)audio;
}

void StepEntityPhysicsAsGoldIdol(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    common::ApplyGravity(entity_idx, state, dt);
    common::PrePartialEulerStep(entity_idx, state, dt);
    common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    common::ApplyGroundFriction(entity_idx, state);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::gold_idol
