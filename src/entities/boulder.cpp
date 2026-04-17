#include "entities/boulder.hpp"

#include "audio.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "stage_break.hpp"
#include "state.hpp"
#include "world_query.hpp"

namespace splonks::entities::boulder {

namespace {

constexpr float kBoulderRollVelocity = 9.0F;
constexpr float kBoulderRestFrames = 5.0F;
constexpr float kBoulderRollSoundDistInterval = 96.0F;

AABB GetLeadingBreakStrip(const Entity& boulder) {
    const AABB aabb = boulder.GetAABB();
    if (boulder.facing == LeftOrRight::Right) {
        return AABB::New(
            Vec2::New(aabb.br.x + 1.0F, aabb.tl.y),
            Vec2::New(aabb.br.x + 1.0F, aabb.br.y)
        );
    }
    return AABB::New(
        Vec2::New(aabb.tl.x - 1.0F, aabb.tl.y),
        Vec2::New(aabb.tl.x - 1.0F, aabb.br.y)
    );
}

bool WouldBreakAnyTiles(const AABB& area, const State& state) {
    for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(state.stage, area)) {
        if (tile_query.tile == nullptr) {
            continue;
        }

        const Tile tile = *tile_query.tile;
        if (tile != Tile::Air && tile != Tile::Exit) {
            return true;
        }
    }
    return false;
}

void StepRollingSound(Entity& boulder, Audio& audio) {
    boulder.travel_sound_countdown -= boulder.dist_traveled_this_frame;
    if (boulder.travel_sound_countdown >= 0.0F) {
        return;
    }

    boulder.travel_sound_countdown = kBoulderRollSoundDistInterval;
    audio.PlaySoundEffect(SoundEffect::BoulderRoll);
}

} // namespace

extern const EntityArchetype kBoulderArchetype{
    .type_ = EntityType::Boulder,
    .size = Vec2::New(28.0F, 28.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_hit = false,
    .can_be_picked_up = false,
    .impassable = true,
    .hurt_on_contact = false,
    .crusher_pusher = true,
    .can_stomp = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .has_ground_friction = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Right,
    .condition = EntityCondition::Normal,
    .ai_state = EntityAiState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .step_logic = StepEntityLogicAsBoulder,
    .step_physics = StepEntityPhysicsAsBoulder,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(HashFrameDataIdConstexpr("boulder")),
};

void StepEntityLogicAsBoulder(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    Entity& boulder = state.entity_manager.entities[entity_idx];
    boulder.max_speed = kBoulderRollVelocity;

    if (boulder.ai_state == EntityAiState::Idle && boulder.grounded) {
        boulder.ai_state = EntityAiState::Disturbed;
        boulder.travel_sound_countdown = 0.0F;
        boulder.point_a = ToIVec2(boulder.pos);
        boulder.counter_b = 0.0F;
    }

    if (boulder.ai_state != EntityAiState::Disturbed) {
        return;
    }

    boulder.vel.x = boulder.facing == LeftOrRight::Right ? kBoulderRollVelocity : -kBoulderRollVelocity;
    StepRollingSound(boulder, audio);
}

void StepEntityPhysicsAsBoulder(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    Entity& boulder = state.entity_manager.entities[entity_idx];
    if (boulder.ai_state == EntityAiState::Disturbed) {
        const AABB break_strip = GetLeadingBreakStrip(boulder);
        const bool will_break_tiles = WouldBreakAnyTiles(break_strip, state);
        if (will_break_tiles && boulder.counter_a <= 0.0F) {
            audio.PlaySoundEffect(SoundEffect::BoulderTileCrash);
            boulder.counter_a = 1.0F;
        }
        if (will_break_tiles) {
            BreakStageTilesInRectWc(break_strip, state, audio);
        }
    }

    common::StepStandardPhysics(entity_idx, state, graphics, audio, dt);

    if (boulder.ai_state == EntityAiState::Disturbed) {
        const IVec2 current_pos = ToIVec2(boulder.pos);
        if (current_pos == boulder.point_a) {
            boulder.counter_b += 1.0F;
        } else {
            boulder.point_a = current_pos;
            boulder.counter_b = 0.0F;
        }

        if (boulder.counter_b >= kBoulderRestFrames) {
            boulder.ai_state = EntityAiState::Returning;
            boulder.vel.x = 0.0F;
        }
    }
}

} // namespace splonks::entities::boulder
