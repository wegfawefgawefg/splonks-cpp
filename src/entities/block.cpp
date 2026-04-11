#include "entities/block.hpp"

#include "audio.hpp"
#include "entity_archetype.hpp"
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

extern const EntityArchetype kBlockArchetype{
    .type_ = EntityType::Block,
    .size = Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize)),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = true,
    .hurt_on_contact = false,
    .crusher_pusher = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Middle,
    .facing = LeftOrRight::Left,
    .super_state = EntitySuperState::Idle,
    .state = EntityState::Projectile,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::CaveBlock),
};

bool TryApplyBlockContactToEntity(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    return common::TryApplyCrusherPusherContact(
        entity_idx,
        other_entity_idx,
        context,
        state,
        graphics,
        audio
    );
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
