#include "entities/box.hpp"

#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "controls.hpp"

#include <cmath>
#include <random>

namespace splonks::entities::box {

namespace {

constexpr float kControlledMoveAcc = 0.1F;
constexpr float kControlledAirMoveAcc = 0.04F;
constexpr float kControlledJumpVel = 2.25F;
constexpr float kControlledSlideVel = 3.75F;
constexpr std::uint32_t kControlledSlideCooldownFrames = 120;
constexpr float kBoxBreakawayImpactSpeed = 2.0F;

int RandInclusive(int minimum, int maximum) {
    static std::random_device device;
    static std::mt19937 generator(device());
    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(generator);
}

bool IsControlled(const Entity& entity, const State& state) {
    return state.controlled_entity_vid.has_value() && entity.vid == *state.controlled_entity_vid;
}

void SpawnEntityAtTopLeft(EntityType type_, const Vec2& pos, State& state) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return;
    }

    SetEntityAs(*entity, type_);
    entity->pos = pos;
    entity->vel = Vec2::New(0.0F, 0.0F);
}

void StepControlledBox(Entity& box, const controls::ControlIntent& control) {
    if (box.attack_delay_countdown > 0) {
        box.attack_delay_countdown -= 1;
    }

    if (control.left && !control.right) {
        box.acc.x -= box.grounded ? kControlledMoveAcc : kControlledAirMoveAcc;
        box.facing = LeftOrRight::Left;
    } else if (control.right && !control.left) {
        box.acc.x += box.grounded ? kControlledMoveAcc : kControlledAirMoveAcc;
        box.facing = LeftOrRight::Right;
    }

    if (control.jump_pressed && box.grounded) {
        box.vel.y = -kControlledJumpVel;
        box.grounded = false;
    }

    if (control.attack_pressed && box.grounded && box.attack_delay_countdown == 0) {
        box.vel.x = box.facing == LeftOrRight::Left ? -kControlledSlideVel : kControlledSlideVel;
        box.attack_delay_countdown = kControlledSlideCooldownFrames;
    }
}

} // namespace

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
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::AnthingExceptJumpOn,
    .death_sound_effect = SoundEffect::BoxBreak,
    .on_death = OnDeathAsBox,
    .step_logic = StepEntityLogicAsBox,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Box),
};

void StepEntityLogicAsBox(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    Entity& box = state.entity_manager.entities[entity_idx];
    if (box.condition == EntityCondition::Dead) {
        return;
    }

    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(box, state);
    if (!IsControlled(box, state)) {
        return;
    }

    StepControlledBox(box, control);
}

void OnDeathAsBox(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;

    const Entity& box = state.entity_manager.entities[entity_idx];
    const Vec2 spawn_pos = box.pos;

    if (RandInclusive(1, 500) == 1) {
        SpawnEntityAtTopLeft(EntityType::JetPack, spawn_pos, state);
    } else if (RandInclusive(1, 200) == 1) {
        SpawnEntityAtTopLeft(EntityType::Cape, spawn_pos, state);
    } else if (RandInclusive(1, 100) == 1) {
        SpawnEntityAtTopLeft(EntityType::Shotgun, spawn_pos, state);
    } else if (RandInclusive(1, 100) == 1) {
        SpawnEntityAtTopLeft(EntityType::Mattock, spawn_pos, state);
    } else if (RandInclusive(1, 100) == 1) {
        SpawnEntityAtTopLeft(EntityType::Teleporter, spawn_pos, state);
    } else if (RandInclusive(1, 90) == 1) {
        SpawnEntityAtTopLeft(EntityType::Gloves, spawn_pos, state);
    } else if (RandInclusive(1, 90) == 1) {
        SpawnEntityAtTopLeft(EntityType::Spectacles, spawn_pos, state);
    } else if (RandInclusive(1, 80) == 1) {
        SpawnEntityAtTopLeft(EntityType::WebCannon, spawn_pos, state);
    } else if (RandInclusive(1, 80) == 1) {
        SpawnEntityAtTopLeft(EntityType::Pistol, spawn_pos, state);
    } else if (RandInclusive(1, 80) == 1) {
        SpawnEntityAtTopLeft(EntityType::Mitt, spawn_pos, state);
    } else if (RandInclusive(1, 60) == 1) {
        SpawnEntityAtTopLeft(EntityType::Paste, spawn_pos, state);
    } else if (RandInclusive(1, 60) == 1) {
        SpawnEntityAtTopLeft(EntityType::SpringShoes, spawn_pos, state);
    } else if (RandInclusive(1, 60) == 1) {
        SpawnEntityAtTopLeft(EntityType::SpikeShoes, spawn_pos, state);
    } else if (RandInclusive(1, 60) == 1) {
        SpawnEntityAtTopLeft(EntityType::Machete, spawn_pos, state);
    } else if (RandInclusive(1, 40) == 1) {
        SpawnEntityAtTopLeft(EntityType::BombBox, spawn_pos, state);
    } else if (RandInclusive(1, 40) == 1) {
        SpawnEntityAtTopLeft(EntityType::Bow, spawn_pos, state);
    } else if (RandInclusive(1, 20) == 1) {
        SpawnEntityAtTopLeft(EntityType::Compass, spawn_pos, state);
    } else if (RandInclusive(1, 10) == 1) {
        SpawnEntityAtTopLeft(EntityType::Parachute, spawn_pos, state);
    } else if (RandInclusive(1, 2) == 1) {
        SpawnEntityAtTopLeft(EntityType::RopePile, spawn_pos, state);
    } else {
        SpawnEntityAtTopLeft(EntityType::BombBag, spawn_pos, state);
    }
}

bool TryApplyBoxImpact(
    std::size_t entity_idx,
    const common::ContactContext& context,
    State& state
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& box = state.entity_manager.entities[entity_idx];
    if (box.type_ != EntityType::Box || box.condition == EntityCondition::Dead) {
        return false;
    }
    if (context.phase != common::ContactPhase::AttemptedBlocked || !context.has_impact) {
        return false;
    }
    if (std::abs(context.impact_velocity) <= kBoxBreakawayImpactSpeed) {
        return false;
    }

    box.health = 0;
    return true;
}

} // namespace splonks::entities::box
