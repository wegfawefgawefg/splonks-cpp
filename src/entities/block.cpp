#include "entities/block.hpp"

#include "audio.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "systems/controls.hpp"
#include "tile.hpp"

#include <cmath>
#include <vector>

namespace splonks::entities::block {

namespace {

constexpr float kControlledBlockMoveAcc = 0.18F;
constexpr float kControlledBlockSlideVel = 3.25F;
constexpr std::uint32_t kControlledBlockSlideCooldownFrames = 120;

Vec2 NormalizeOrZero(const Vec2& value) {
    const float length = Length(value);
    if (length == 0.0F) {
        return Vec2::New(0.0F, 0.0F);
    }
    return value / length;
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

    //TODO: if you hit the ground, do a clunky sound
    // if you hit something, do block damage and try to stun probs

    const Entity& block = state.entity_manager.entities[entity_idx];
    // REPEL THINGS FROM CENTER
    const AABB aabb = block.GetAABB();
    const VID block_vid = block.vid;
    const AABB repel_zone = {
        .tl = aabb.tl + Vec2::New(1.0F, 1.0F),
        .br = aabb.br - Vec2::New(1.0F, 1.0F),
    };
    const Vec2 center = block.GetCenter();
    const std::vector<VID> results = state.sid.QueryExclude(repel_zone.tl, repel_zone.br, block_vid);
    for (const VID& vid : results) {
        if (Entity* const entity = state.entity_manager.GetEntityMut(vid)) {
            const Vec2 entity_center = entity->GetCenter();
            const Vec2 repel_force = NormalizeOrZero(entity_center - center) * 4.0F;
            entity->acc += repel_force;
        }
    }

    // CRUSH ZONE
    const Entity& updated_block = state.entity_manager.entities[entity_idx];
    const AABB updated_aabb = updated_block.GetAABB();
    const AABB block_body_and_foot = {
        .tl = updated_aabb.tl + (Vec2::New(1.0F, 1.0F) * 4.0F),
        .br = updated_aabb.br - (Vec2::New(1.0F, 1.0F) * 4.0F),
    };

    // BLOW UP ANYTHING INSIDE OR UNDER YOU
    const std::vector<VID> crush_results =
        state.sid.QueryExclude(block_body_and_foot.tl, block_body_and_foot.br, updated_block.vid);
    for (const VID& vid : crush_results) {
        if (Entity* const entity = state.entity_manager.GetEntityMut(vid)) {
            if (!entity->impassable && entity->can_collide &&
                !(entity->super_state == EntitySuperState::Dead)) {
                const common::DamageResult damage_result =
                    common::TryToDamageEntity(entity->vid.id, state, audio, DamageType::Crush, 1);
                switch (damage_result) {
                case common::DamageResult::Hurt:
                case common::DamageResult::Died:
                    break;
                case common::DamageResult::None:
                    break;
                }
            }
        }
    }

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
