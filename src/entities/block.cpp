#include "entities/block.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "particles/ultra_dynamic_particle.hpp"
#include "state.hpp"
#include "controls.hpp"
#include "tile.hpp"

#include <cmath>
#include <memory>

namespace splonks::entities::block {

namespace {

constexpr float kControlledBlockMoveAcc = 0.18F;
constexpr float kControlledBlockSlideVel = 3.25F;
constexpr std::uint32_t kControlledBlockSlideCooldownFrames = 120;
constexpr float kBlockTrailSmokeDistInterval = 14.0F;

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
    case StageType::SplkMines1:
    case StageType::SplkMines2:
    case StageType::SplkMines3:
        return frame_data_ids::CaveBlock;
    }

    return frame_data_ids::CaveBlock;
}

bool IsControlled(const Entity& entity, const State& state) {
    return state.controlled_entity_vid.has_value() && entity.vid == *state.controlled_entity_vid;
}

void StepControlledBlock(Entity& block, const controls::ControlIntent& control) {
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

void SpawnBlockDeathParticles(const Vec2& center, FrameDataId animation_id, State& state) {
    for (int i = 0; i < 12; ++i) {
        auto shard = std::make_unique<UltraDynamicParticle>();
        shard->frame_data_animator = FrameDataAnimator::New(animation_id);
        shard->draw_layer = DrawLayer::Foreground;
        shard->counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(20, 42));
        shard->pos = center + Vec2::New(
                 rng::RandomFloat(-3.0F, 3.0F),
                 rng::RandomFloat(-3.0F, 3.0F)
             );
        const float size = rng::RandomFloat(3.0F, 7.0F);
        shard->size = Vec2::New(size, size);
        shard->rot = rng::RandomFloat(0.0F, 360.0F);
        shard->alpha = 1.0F;
        shard->vel = Vec2::New(
            rng::RandomFloat(-2.4F, 2.4F),
            rng::RandomFloat(-4.2F, -1.2F)
        );
        shard->svel = Vec2::New(0.0F, 0.0F);
        shard->rotvel = rng::RandomFloat(-0.7F, 0.7F);
        shard->alpha_vel = -0.018F;
        shard->acc = Vec2::New(0.0F, 0.18F);
        shard->sacc = Vec2::New(0.0F, 0.0F);
        shard->rotacc = 0.0F;
        shard->alpha_acc = -0.003F;
        state.particles.Add(std::move(shard));
    }
}

void SpawnBlockTrailSmoke(State& state, const Vec2& pos, LeftOrRight facing) {
    auto smoke = std::make_unique<UltraDynamicParticle>();
    smoke->frame_data_animator = FrameDataAnimator::New(frame_data_ids::LittleSmoke);
    smoke->draw_layer = DrawLayer::Foreground;
    smoke->counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(16, 28));
    smoke->pos = pos + Vec2::New(
             rng::RandomFloat(-1.0F, 1.0F),
             rng::RandomFloat(-1.0F, 1.0F)
         );
    const float size = rng::RandomFloat(3.0F, 5.5F);
    smoke->size = Vec2::New(size, size);
    smoke->rot = rng::RandomFloat(0.0F, 360.0F);
    smoke->alpha = rng::RandomFloat(0.55F, 0.85F);
    smoke->vel = Vec2::New(
        facing == LeftOrRight::Right ? rng::RandomFloat(-0.9F, -0.2F)
                                     : rng::RandomFloat(0.2F, 0.9F),
        rng::RandomFloat(-0.8F, -0.2F)
    );
    smoke->svel = Vec2::New(rng::RandomFloat(0.01F, 0.03F), rng::RandomFloat(0.01F, 0.03F));
    smoke->rotvel = rng::RandomFloat(-0.2F, 0.2F);
    smoke->alpha_vel = -0.02F;
    smoke->acc = Vec2::New(0.0F, 0.01F);
    smoke->sacc = Vec2::New(0.0F, 0.0F);
    smoke->rotacc = 0.0F;
    smoke->alpha_acc = -0.003F;
    state.particles.Add(std::move(smoke));
}

Vec2 GetBlockTrailingBottomCorner(const Entity& block) {
    const AABB aabb = block.GetAABB();
    return block.facing == LeftOrRight::Right
               ? Vec2::New(aabb.tl.x, aabb.br.y)
               : Vec2::New(aabb.br.x, aabb.br.y);
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
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Middle,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::ExplosionOnly,
    .on_death = OnDeathAsBlock,
    .step_logic = StepEntityLogicAsBlock,
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

void StepEntityLogicAsBlock(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    {
        Entity& entity = state.entity_manager.entities[entity_idx];
        SetAnimation(entity, BlockFrameDataIdForStageType(state.stage.stage_type));
    }

    {
        Entity& block = state.entity_manager.entities[entity_idx];
        const controls::ControlIntent control =
            controls::GetControlIntentForEntity(block, state);
        if (IsControlled(block, state) && block.condition != EntityCondition::Dead) {
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
    if (entity.grounded && entity.dist_traveled_this_frame > 0.0F && entity.travel_sound_countdown < 0.0F) {
        entity.travel_sound_countdown = kWalkerClimberTravelSoundDistInterval;
        const SoundEffect sound =
            entity.travel_sound == TravelSound::One ? SoundEffect::BlockDrag1
                                                    : SoundEffect::BlockDrag2;
        audio.PlaySoundEffect(sound);
        entity.IncTravelSound();
    }

    if (entity.grounded && entity.dist_traveled_this_frame > 0.0F) {
        entity.counter_c -= entity.dist_traveled_this_frame;
        while (entity.counter_c <= 0.0F) {
            entity.counter_c += kBlockTrailSmokeDistInterval;
            SpawnBlockTrailSmoke(state, GetBlockTrailingBottomCorner(entity), entity.facing);
        }
    }
}

void OnDeathAsBlock(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }
    Entity& block = state.entity_manager.entities[entity_idx];
    SpawnBlockDeathParticles(block.GetCenter(), BlockFrameDataIdForStageType(state.stage.stage_type), state);
}

/** generalize this to all square or rectangular entities somehow */
} // namespace splonks::entities::block
