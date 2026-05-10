#include "entities/box.hpp"
#include "on_damage_effects.hpp"

#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "gameplay_authority.hpp"
#include "gameplay_messages.hpp"
#include "state.hpp"
#include "controls.hpp"
#include "world_ops.hpp"

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


Entity* SpawnEntityAtTopLeft(EntityType type_, const Vec2& pos, State& state) {
    return world_ops::SpawnEntity(state, type_, [pos](Entity& entity) {
        entity.pos = pos;
        entity.vel = Vec2::New(0.0F, 0.0F);
    });
}

EntityType RandomTeleporterVariant() {
    return RandInclusive(1, 2) == 1 ? EntityType::Teleporter : EntityType::TeleporterBackpack;
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

void ControlEntityAsBox(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& box = state.entity_manager.entities[entity_idx];
    if (box.condition == EntityCondition::Dead) {
        return;
    }

    StepControlledBox(box, controls::GetControlIntentForEntity(box, state));
}

} // namespace

common::ContactResolution BuildBoxImpactResolution(
    bool applied
) {
    return common::ContactResolution{
        .blocks_movement = false,
        .stop_sweep = applied,
    };
}

common::ContactResolution OnEntityContactAsBox(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)other_entity_idx;
    (void)graphics;
    (void)audio;
    if (!context.mover_vid.has_value() || *context.mover_vid != state.entity_manager.entities[entity_idx].vid) {
        return common::ContactResolution{};
    }
    return BuildBoxImpactResolution(TryApplyBoxImpact(entity_idx, context, state));
}

common::ContactResolution OnTileContactAsBox(
    std::size_t entity_idx,
    const common::ContactContext& context,
    State& state
) {
    return BuildBoxImpactResolution(TryApplyBoxImpact(entity_idx, context, state));
}

extern const EntityArchetype kBoxArchetype{
    .type_ = EntityType::Box,
    .size = Vec2::New(12.0F, 12.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .buoyancy = 1.0F,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::AnthingExceptJumpOn,
    .collide_sound = audio_asset_ids::Thud,
    .death_sound = audio_asset_ids::BoxBreak,
    .on_death = OnDeathAsBox,
    .control_logic = ControlEntityAsBox,
    .step_logic = StepEntityLogicAsBox,
    .on_entity_contact = OnEntityContactAsBox,
    .on_tile_contact = OnTileContactAsBox,
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
    (void)entity_idx;
    (void)state;
    (void)graphics;
    (void)audio;
    (void)dt;
}

void OnDeathAsBox(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;

    const Entity& box = state.entity_manager.entities[entity_idx];
    if (!HasLocalGameplayAuthorityForEntity(state, box.vid)) {
        return;
    }

    const Vec2 spawn_pos = box.pos;
    SpawnBreakawayContainerShards(box.GetCenter(), state);

    // Matches ClassicHD's actual open-crate roll order, with unimplemented Shotgun
    // intentionally substituted by Pistol for now. HD/2 use the same common
    // tail shape: 1/2 RopePile, otherwise guaranteed BombBag.
    if (RandInclusive(1, 500) == 1) {
        SpawnEntityAtTopLeft(EntityType::JetPack, spawn_pos, state);
    } else if (RandInclusive(1, 200) == 1) {
        SpawnEntityAtTopLeft(EntityType::Cape, spawn_pos, state);
    } else if (RandInclusive(1, 100) == 1) {
        SpawnEntityAtTopLeft(EntityType::Pistol, spawn_pos, state);
    } else if (RandInclusive(1, 100) == 1) {
        SpawnEntityAtTopLeft(EntityType::Mattock, spawn_pos, state);
    } else if (RandInclusive(1, 100) == 1) {
        SpawnEntityAtTopLeft(RandomTeleporterVariant(), spawn_pos, state);
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

    if (!HasLocalGameplayAuthorityForEntity(state, box.vid)) {
        if (context.mover_vid.has_value() &&
            HasLocalGameplayAuthorityForInteractionSource(state, *context.mover_vid)) {
            world_ops::RequestGameplayAction(
                state,
                DamageEntityAction{
                    .source_vid = context.mover_vid,
                    .target_vid = box.vid,
                    .damage_type = DamageType::Attack,
                    .amount = 1,
                }
            );
            return true;
        }
        return false;
    }

    box.health = 0;
    return true;
}

} // namespace splonks::entities::box
