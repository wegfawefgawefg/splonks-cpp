#include "entities/bomb.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"

#include <cmath>

namespace splonks::entities::bomb {

namespace {

constexpr float kBombRotationDegreesPerPixel = 24.0F;

void UpdateBombRotation(Entity& bomb) {
    if (bomb.held_by_vid.has_value() || bomb.attachment_mode != AttachmentMode::None) {
        return;
    }
    if (std::abs(bomb.vel.x) < 0.01F) {
        return;
    }

    bomb.rotation = std::fmod(
        bomb.rotation + (bomb.vel.x * kBombRotationDegreesPerPixel),
        360.0F
    );
    if (bomb.rotation < 0.0F) {
        bomb.rotation += 360.0F;
    }
}

} // namespace

extern const EntityArchetype kBombArchetype{
    .type_ = EntityType::Bomb,
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
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingAndSpikes,
    .on_death = OnDeathAsBomb,
    .on_use = OnUseAsBomb,
    .step_logic = StepEntityLogicAsBomb,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Grenade),
};

void OnDeathAsBomb(std::size_t entity_idx, State& state, Audio& audio) {
    common::OnDeathAsExplosion(entity_idx, state, audio);
}

void OnUseAsBomb(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)graphics;
    (void)audio;
    Entity& bomb = state.entity_manager.entities[entity_idx];
    if (!bomb.use_state.pressed || bomb.counter_a > 0.0F) {
        return;
    }

    bomb.counter_a = 144.0F;
    SetAnimation(bomb, frame_data_ids::LiveGrenade);

    if (bomb.use_state.source == AttachmentMode::None) {
        StopUsingEntity(bomb);
    }
}

void StepEntityLogicAsBomb(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    Entity& bomb = state.entity_manager.entities[entity_idx];

    // if bomb is in winding up
    // set animation and display state
    // start decrementing the counter
    if (bomb.counter_a > 0.0F) {
        bomb.counter_a -= 1.0F;
        if (bomb.counter_a <= 0.0F) {
            bomb.health = 0;
            common::DieIfDead(entity_idx, state, audio);
            return;
        }
    }

    UpdateBombRotation(bomb);
}

/** generalize this to all square or rectangular entities somehow */
} // namespace splonks::entities::bomb
