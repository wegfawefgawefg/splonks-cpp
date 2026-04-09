#include "entities/block.hpp"

#include "audio.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "systems/controls.hpp"
#include "tile.hpp"

#include <cmath>

namespace splonks::entities::block {

namespace {

constexpr float kControlledBlockMoveAcc = 0.18F;
constexpr float kControlledBlockSlideVel = 3.25F;
constexpr std::uint32_t kControlledBlockSlideCooldownFrames = 120;

std::optional<IVec2> GetPushDirectionForBlockContact(const common::ContactContext& context) {
    if (context.impact_axis == common::BlockingImpactAxis::Horizontal) {
        if (context.direction > 0) {
            return IVec2::New(1, 0);
        }
        if (context.direction < 0) {
            return IVec2::New(-1, 0);
        }
        return std::nullopt;
    }

    if (context.direction > 0) {
        return IVec2::New(0, 1);
    }
    if (context.direction < 0) {
        return IVec2::New(0, -1);
    }
    return std::nullopt;
}

bool IsBlockPushTarget(const Entity& entity) {
    if (!entity.active || !entity.can_collide || entity.impassable) {
        return false;
    }
    if (entity.super_state == EntitySuperState::Crushed) {
        return false;
    }
    return true;
}

bool IsAtBlockLeadingFace(const Entity& block, const Entity& other_entity, const IVec2& push_direction) {
    const AABB block_aabb = block.GetAABB();
    const AABB other_aabb = other_entity.GetAABB();
    const Vec2 block_center = block.GetCenter();
    const Vec2 other_center = other_entity.GetCenter();

    const float overlap_x =
        std::min(block_aabb.br.x, other_aabb.br.x) - std::max(block_aabb.tl.x, other_aabb.tl.x);
    const float overlap_y =
        std::min(block_aabb.br.y, other_aabb.br.y) - std::max(block_aabb.tl.y, other_aabb.tl.y);

    if (push_direction.x > 0) {
        return overlap_y >= 0.0F && other_center.x >= block_center.x;
    }
    if (push_direction.x < 0) {
        return overlap_y >= 0.0F && other_center.x <= block_center.x;
    }
    if (push_direction.y > 0) {
        return overlap_x >= 0.0F && other_center.y >= block_center.y;
    }
    if (push_direction.y < 0) {
        return overlap_x >= 0.0F && other_center.y <= block_center.y;
    }

    return false;
}

FrameDataId BlockFrameDataIdForStageType(StageType stage_type) {
    switch (stage_type) {
    case StageType::Ice1:
    case StageType::Ice2:
    case StageType::Ice3:
        return frame_data_ids::IceBlock;
    case StageType::Desert1:
    case StageType::Desert2:
    case StageType::Desert3:
        return frame_data_ids::JungleBlock;
    case StageType::Temple1:
    case StageType::Temple2:
    case StageType::Temple3:
        return frame_data_ids::TempleBlock;
    case StageType::Boss:
        return frame_data_ids::BossBlock;
    case StageType::Blank:
    case StageType::Test1:
    case StageType::Cave1:
    case StageType::Cave2:
    case StageType::Cave3:
        return frame_data_ids::CaveBlock;
    }

    return frame_data_ids::CaveBlock;
}

bool IsControlled(const Entity& entity, const State& state) {
    return state.controlled_entity_vid.has_value() && entity.vid == *state.controlled_entity_vid;
}

void StepControlledBlock(Entity& block, const systems::controls::ControlIntent& control) {
    if (block.attack_delay_countdown > 0) {
        block.attack_delay_countdown -= 1;
    }

    if (control.left && !control.right) {
        block.acc.x -= kControlledBlockMoveAcc;
        block.facing = LeftOrRight::Left;
    } else if (control.right && !control.left) {
        block.acc.x += kControlledBlockMoveAcc;
        block.facing = LeftOrRight::Right;
    }

    if (control.attack_pressed && block.grounded && block.attack_delay_countdown == 0) {
        block.vel.x =
            block.facing == LeftOrRight::Left ? -kControlledBlockSlideVel : kControlledBlockSlideVel;
        block.attack_delay_countdown = kControlledBlockSlideCooldownFrames;
    }
}

} // namespace

void SetEntityBlock(Entity& entity) {
    entity.Reset();
    entity.active = true;
    entity.type_ = EntityType::Block;
    entity.health = 1;
    entity.size = Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize));
    entity.has_physics = true;
    entity.damage_vulnerability = DamageVulnerability::Immune;
    entity.can_collide = true;
    entity.can_be_picked_up = false;
    entity.impassable = true;
    entity.super_state = EntitySuperState::Idle;
    entity.state = EntityState::Projectile;
    entity.display_state = EntityDisplayState::Neutral;
    entity.facing = LeftOrRight::Left;
    entity.frame_data_animator.SetAnimation(frame_data_ids::CaveBlock);
    entity.can_be_stunned = false;
    entity.alignment = Alignment::Neutral;
}

bool TryApplyBlockContactToEntity(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    if (entity_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    const Entity& block = state.entity_manager.entities[entity_idx];
    if (!context.mover_vid.has_value() || block.vid != *context.mover_vid) {
        return false;
    }

    const std::optional<IVec2> push_direction = GetPushDirectionForBlockContact(context);
    if (!push_direction.has_value()) {
        return false;
    }

    Entity& other_entity = state.entity_manager.entities[other_entity_idx];
    if (!IsBlockPushTarget(other_entity)) {
        return false;
    }
    if (!IsAtBlockLeadingFace(block, other_entity, *push_direction)) {
        return false;
    }

    if (common::TryDisplaceEntityByOnePixel(
            other_entity_idx,
            *push_direction,
            state,
            graphics,
            &audio)) {
        return true;
    }

    common::TryToDamageEntity(other_entity_idx, state, audio, DamageType::Crush, 1);
    return true;
}

void StepEntityLogicAsBlock(std::size_t entity_idx, State& state, Audio& audio) {
    {
        Entity& entity = state.entity_manager.entities[entity_idx];
        entity.frame_data_animator.SetAnimation(BlockFrameDataIdForStageType(state.stage.stage_type));
    }

    {
        Entity& block = state.entity_manager.entities[entity_idx];
        const systems::controls::ControlIntent control =
            systems::controls::GetControlIntentForEntity(block, state);
        if (IsControlled(block, state) && block.super_state != EntitySuperState::Dead &&
            block.super_state != EntitySuperState::Crushed) {
            StepControlledBlock(block, control);
        }
    }

    // TODO: if you hit the ground, do a clunky sound
    // TODO: if you hit something hard, do a block thunk sound

    Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.grounded) {
        entity.travel_sound_countdown -= entity.dist_traveled_this_frame;
    }

    // TODO: extract into grounded movement sounds
    if (entity.travel_sound_countdown < 0.0F) {
        entity.travel_sound_countdown = kWalkerClimberTravelSoundDistInterval;
        const SoundEffect sound =
            entity.travel_sound == TravelSound::One ? SoundEffect::BlockDrag1
                                                    : SoundEffect::BlockDrag2;
        audio.PlaySoundEffect(sound);
        entity.IncTravelSound();
    }
}

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsBlock(
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

} // namespace splonks::entities::block
