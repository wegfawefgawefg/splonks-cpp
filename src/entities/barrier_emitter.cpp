#include "entities/barrier_emitter.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "gameplay_events.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <optional>
#include <vector>

namespace splonks::entities::barrier_emitter {

namespace {

constexpr int kBeamSegmentCount = 3;
constexpr float kBeamSegmentSize = 16.0F;
constexpr unsigned int kBeamContactDamage = 1;

bool HasSolidSupportAbove(const Entity& emitter, const State& state) {
    const Vec2 probe = emitter.GetCenter() + Vec2::New(0.0F, -kBeamSegmentSize);
    const std::optional<WorldTileQueryResult> tile_query =
        QueryTileAtWorldPos(state.stage, ToIVec2(probe));
    return tile_query.has_value() && tile_query->tile != nullptr &&
           IsTileCollidable(*tile_query->tile);
}

std::vector<VID>& EnsureChildBeamVids(Entity& emitter) {
    if (!emitter.child_vids.has_value()) {
        emitter.child_vids.emplace();
    }
    return *emitter.child_vids;
}

Entity* SpawnBeamSegment(State& state, const Vec2& center, const VID& emitter_vid) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return nullptr;
    }

    Entity* const beam = state.entity_manager.GetEntityMut(*vid);
    if (beam == nullptr) {
        return nullptr;
    }

    SetEntityAs(*beam, EntityType::Beam);
    beam->SetCenter(center);
    beam->entity_a = emitter_vid;
    return beam;
}

void DestroyBeamChildren(Entity& emitter, State& state) {
    if (!emitter.child_vids.has_value()) {
        return;
    }

    for (const VID& child_vid : *emitter.child_vids) {
        if (Entity* const child = state.entity_manager.GetEntityMut(child_vid)) {
            EmitEntityDeactivatedGameplayEvent(state, *child);
            state.entity_manager.SetInactiveVid(child_vid);
        }
    }
    emitter.child_vids->clear();
}

void EnsureBeamSegments(std::size_t emitter_idx, State& state) {
    Entity& emitter = state.entity_manager.entities[emitter_idx];
    std::vector<VID>& beam_vids = EnsureChildBeamVids(emitter);
    beam_vids.resize(kBeamSegmentCount);

    for (std::size_t segment_idx = 0; segment_idx < static_cast<std::size_t>(kBeamSegmentCount); ++segment_idx) {
        const Vec2 segment_center =
            emitter.GetCenter() + Vec2::New(0.0F, kBeamSegmentSize * static_cast<float>(segment_idx + 1));
        Entity* beam = state.entity_manager.GetEntityMut(beam_vids[segment_idx]);
        bool spawned_beam = false;
        if (beam == nullptr || !beam->active || beam->type_ != EntityType::Beam ||
            beam->entity_a != emitter.vid) {
            beam = SpawnBeamSegment(state, segment_center, emitter.vid);
            if (beam == nullptr) {
                beam_vids[segment_idx] = VID{};
                continue;
            }
            beam_vids[segment_idx] = beam->vid;
            spawned_beam = true;
        }

        beam->SetCenter(segment_center);
        beam->facing = emitter.facing;
        beam->alpha = emitter.alpha;
        if (spawned_beam) {
            EmitEntitySpawnedGameplayEvent(state, *beam);
        }
    }
}

void StepEntityLogicAsBarrierEmitter(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& emitter = state.entity_manager.entities[entity_idx];
    if (!emitter.active || emitter.condition == EntityCondition::Dead) {
        return;
    }

    if (!HasSolidSupportAbove(emitter, state)) {
        emitter.health = 0;
        common::DieIfDead(entity_idx, state, audio);
        return;
    }

    EnsureBeamSegments(entity_idx, state);
}

void StepEntityLogicAsBeam(
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

    Entity& beam = state.entity_manager.entities[entity_idx];
    if (!beam.entity_a.has_value()) {
        EmitEntityDeactivatedGameplayEvent(state, beam);
        state.entity_manager.SetInactive(entity_idx);
        return;
    }

    const Entity* const emitter = state.entity_manager.GetEntity(*beam.entity_a);
    if (emitter == nullptr || !emitter->active || emitter->condition == EntityCondition::Dead) {
        EmitEntityDeactivatedGameplayEvent(state, beam);
        state.entity_manager.SetInactive(entity_idx);
    }
}

void OnDeathAsBarrierEmitter(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& emitter = state.entity_manager.entities[entity_idx];
    DestroyBeamChildren(emitter, state);
    AddShake(
        state,
        emitter.GetCenter(),
        1.4F,
        2.0F,
        ShakeMask::ForegroundTiles | ShakeMask::BackgroundTiles | ShakeMask::Entities
    );
}

} // namespace

extern const EntityArchetype kBarrierEmitterArchetype{
    .type_ = EntityType::BarrierEmitter,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .can_be_hung_on = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::AnthingExceptJumpOn,
    .death_sound = audio_asset_ids::BombExplosion,
    .on_death = OnDeathAsBarrierEmitter,
    .step_logic = StepEntityLogicAsBarrierEmitter,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::BeamEmitter),
};

extern const EntityArchetype kBeamArchetype{
    .type_ = EntityType::Beam,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = true,
    .can_be_hit = false,
    .can_be_picked_up = false,
    .impassable = false,
    .can_be_hung_on = false,
    .hurt_on_contact = true,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .projectile_contact_damage_amount = kBeamContactDamage,
    .can_apply_projectile_contact = false,
    .step_logic = StepEntityLogicAsBeam,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Beam),
};

} // namespace splonks::entities::barrier_emitter
