#include "entities/breakaway_container.hpp"

#include "audio.hpp"
#include "entity_archetype.hpp"
#include "entities/bat.hpp"
#include "entities/common.hpp"
#include "entities/mod.hpp"
#include "frame_data_id.hpp"
#include "entities/money.hpp"
#include "state.hpp"
#include "systems/controls.hpp"

#include <cmath>
#include <random>

namespace splonks::entities::breakaway_container {

namespace {

constexpr float kControlledPotMoveAcc = 0.07F;
constexpr float kControlledBoxMoveAcc = 0.1F;
constexpr float kControlledAirMoveAcc = 0.04F;
constexpr float kControlledPotJumpVel = 3.0F;
constexpr float kControlledBoxJumpVel = 2.25F;
constexpr float kControlledPotSlideVel = 3.0F;
constexpr float kControlledBoxSlideVel = 3.75F;
constexpr std::uint32_t kControlledSlideCooldownFrames = 120;

unsigned int RandomPercent() {
    static std::random_device device;
    static std::mt19937 generator(device());
    std::uniform_int_distribution<unsigned int> distribution(0, 99);
    return distribution(generator);
}

bool IsControlled(const Entity& entity, const State& state) {
    return state.controlled_entity_vid.has_value() && entity.vid == *state.controlled_entity_vid;
}

constexpr float kPotBreakawayImpactSpeed = 1.0F;
constexpr float kBoxBreakawayImpactSpeed = 2.0F;

float BreakawayImpactSpeedForType(EntityType type_) {
    switch (type_) {
    case EntityType::Pot:
        return kPotBreakawayImpactSpeed;
    case EntityType::Box:
        return kBoxBreakawayImpactSpeed;
    default:
        return kPotBreakawayImpactSpeed;
    }
}

void StepControlledBreakawayContainer(
    Entity& breakaway_container,
    EntityType type_,
    const systems::controls::ControlIntent& control
) {
    if (breakaway_container.attack_delay_countdown > 0) {
        breakaway_container.attack_delay_countdown -= 1;
    }

    const float move_acc = type_ == EntityType::Pot ? kControlledPotMoveAcc : kControlledBoxMoveAcc;
    const float jump_vel = type_ == EntityType::Pot ? kControlledPotJumpVel : kControlledBoxJumpVel;
    const float slide_vel =
        type_ == EntityType::Pot ? kControlledPotSlideVel : kControlledBoxSlideVel;

    if (control.left && !control.right) {
        breakaway_container.acc.x -=
            breakaway_container.grounded ? move_acc : kControlledAirMoveAcc;
        breakaway_container.facing = LeftOrRight::Left;
    } else if (control.right && !control.left) {
        breakaway_container.acc.x +=
            breakaway_container.grounded ? move_acc : kControlledAirMoveAcc;
        breakaway_container.facing = LeftOrRight::Right;
    }

    if (control.jump_pressed && breakaway_container.grounded) {
        breakaway_container.vel.y = -jump_vel;
        breakaway_container.grounded = false;
    }

    if (control.attack_pressed && breakaway_container.grounded &&
        breakaway_container.attack_delay_countdown == 0) {
        breakaway_container.vel.x =
            breakaway_container.facing == LeftOrRight::Left ? -slide_vel : slide_vel;
        breakaway_container.attack_delay_countdown = kControlledSlideCooldownFrames;
    }
}

} // namespace

extern const EntityArchetype kPotArchetype{
    .type_ = EntityType::Pot,
    .size = Vec2::New(8.0F, 7.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .super_state = EntitySuperState::Idle,
    .state = EntityState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::AnthingExceptJumpOn,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Pot),
};
extern const EntityArchetype kBoxArchetype{
    .type_ = EntityType::Box,
    .size = Vec2::New(12.0F, 12.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .super_state = EntitySuperState::Idle,
    .state = EntityState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::AnthingExceptJumpOn,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Box),
};

void StepEntityLogicAsBreakawayContainer(
    std::size_t entity_idx,
    State& state,
    Audio& audio
) {
    (void)audio;
    Entity& breakaway_container = state.entity_manager.entities[entity_idx];
    const EntitySuperState breakaway_container_super_state = breakaway_container.super_state;
    const Vec2 breakaway_container_pos = breakaway_container.pos;
    const EntityType breakaway_container_type = breakaway_container.type_;
    if (breakaway_container_super_state == EntitySuperState::Dead) {
        const unsigned int random_number = RandomPercent();
        switch (breakaway_container_type) {
        case EntityType::Pot:
            if (random_number <= 10) {
                // spawn a bat
                if (const std::optional<VID> bat_vid = state.entity_manager.NewEntity()) {
                    if (Entity* const bat = state.entity_manager.GetEntityMut(*bat_vid)) {
                        SetEntityAs(*bat, EntityType::Bat);
                        bat->pos = breakaway_container_pos;
                    }
                }
            } else if (random_number <= 40) {
                // spawn a gold
                if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                    if (Entity* const new_entity = state.entity_manager.GetEntityMut(*vid)) {
                        SetEntityAs(*new_entity, EntityType::Gold);
                        new_entity->pos = breakaway_container_pos;
                    }
                }
            }
            break;
        case EntityType::Box:
            if (random_number <= 30) {
                // spawn a big gold
                if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                    if (Entity* const new_entity = state.entity_manager.GetEntityMut(*vid)) {
                        SetEntityAs(*new_entity, EntityType::GoldStack);
                        new_entity->pos = breakaway_container_pos;
                    }
                }
            } else if (random_number <= 90) {
                // spawn a gold
                if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                    if (Entity* const new_entity = state.entity_manager.GetEntityMut(*vid)) {
                        SetEntityAs(*new_entity, EntityType::Gold);
                        new_entity->pos = breakaway_container_pos;
                    }
                }
            }
            break;
        default:
            break;
        }

        return;
    }

    const systems::controls::ControlIntent control =
        systems::controls::GetControlIntentForEntity(breakaway_container, state);
    if (IsControlled(breakaway_container, state) &&
        breakaway_container_super_state != EntitySuperState::Dead) {
        StepControlledBreakawayContainer(
            breakaway_container,
            breakaway_container_type,
            control
        );
    }

}

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsBreakawayContainer(
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

Vec2 GetBreakawayContainerSize(EntityType type_) {
    switch (type_) {
    case EntityType::Pot:
        return Vec2::New(8.0F, 7.0F);
    case EntityType::Box:
        return Vec2::New(12.0F, 12.0F);
    default:
        return Vec2::New(1.0F, 1.0F);
    }
}

bool TryApplyBreakawayContainerImpact(
    std::size_t entity_idx,
    const common::ContactContext& context,
    State& state
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.type_ != EntityType::Pot && entity.type_ != EntityType::Box) {
        return false;
    }
    if (context.phase != common::ContactPhase::AttemptedBlocked || !context.has_impact) {
        return false;
    }
    if (std::abs(context.impact_velocity) <= BreakawayImpactSpeedForType(entity.type_)) {
        return false;
    }

    entity.health = 0;
    return true;
}

} // namespace splonks::entities::breakaway_container
