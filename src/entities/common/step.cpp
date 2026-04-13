#include "entities/common/common.hpp"

#include "entity/archetype.hpp"
#include "tile.hpp"
#include "world_query.hpp"

namespace splonks::entities::common {

void ApplyDeactivateConditions(std::size_t entity_idx, State& state) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const bool vanish_on_death = splonks::GetEntityArchetype(entity.type_).vanish_on_death;
    if ((vanish_on_death && entity.condition == EntityCondition::Dead) ||
        entity.marked_for_destruction) {
        state.entity_manager.SetInactive(entity_idx);
    }
}

void StepStunTimer(std::size_t entity_idx, State& state) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.contact_sound_cooldown > 0) {
        entity.contact_sound_cooldown -= 1;
    }
    if (entity.condition == EntityCondition::Stunned) {
        if (entity.stun_timer == 0) {
            entity.condition = EntityCondition::Normal;
            TrySetAnimation(entity, EntityDisplayState::Neutral);
        } else {
            if (entity.stun_timer > 0) {
                entity.stun_timer -= 1;
            }
        }
    }
}

void StepTravelSoundWalkerClimber(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.travel_sound_countdown -= entity.dist_traveled_this_frame;

    if (!entity.grounded && !entity.IsClimbing()) {
        return;
    }

    if (entity.travel_sound_countdown < 0.0F) {
        entity.travel_sound_countdown = kWalkerClimberTravelSoundDistInterval;

        SoundEffect which_step_sound;
        if (entity.IsClimbing()) {
            const auto [entity_tl, entity_br] = entity.GetBounds();
            bool its_rope = false;
            bool its_ladder = false;
            for (const WorldTileQueryResult& tile_query : QueryTilesInWorldRect(
                     state.stage,
                     ToIVec2(entity_tl),
                     ToIVec2(entity_br))) {
                if (tile_query.tile == nullptr) {
                    continue;
                }
                if (*tile_query.tile == Tile::Rope) {
                    its_rope = true;
                }
                if (*tile_query.tile == Tile::Ladder) {
                    its_ladder = true;
                }
            }
            if (its_rope) {
                which_step_sound = entity.travel_sound == TravelSound::One
                                       ? SoundEffect::ClimbRope1
                                       : SoundEffect::ClimbRope2;
            } else if (its_ladder) {
                which_step_sound = entity.travel_sound == TravelSound::One
                                       ? SoundEffect::ClimbMetal1
                                       : SoundEffect::ClimbMetal2;
            } else {
                which_step_sound = SoundEffect::Step1;
            }
        } else {
            which_step_sound =
                entity.travel_sound == TravelSound::One ? SoundEffect::Step1 : SoundEffect::Step2;
        }
        audio.PlaySoundEffect(which_step_sound);
        entity.IncTravelSound();
    }
}

void DoThrownByStep(std::size_t entity_idx, State& state) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const std::optional<VID> thrown_by = entity.thrown_by;
    if (thrown_by) {
        if (entity.thrown_immunity_timer > 0) {
            entity.thrown_immunity_timer -= 1;
        }
    }
    if (entity.thrown_immunity_timer == 0) {
        entity.thrown_by.reset();
    }
}

} // namespace splonks::entities::common
