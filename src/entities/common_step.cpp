#include "entities/common.hpp"

#include "tile.hpp"

namespace splonks::entities::common {

void ApplyDeactivateConditions(std::size_t entity_idx, State& state) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const bool vanish_on_death =
        entity.type_ == EntityType::Bomb || entity.type_ == EntityType::JetPack ||
        entity.type_ == EntityType::Pot || entity.type_ == EntityType::Box ||
        entity.type_ == EntityType::Rope;
    if ((vanish_on_death && entity.super_state == EntitySuperState::Dead) ||
        entity.marked_for_destruction) {
        state.entity_manager.SetInactive(entity_idx);
    }
}

void StoreHealthToLastHealth(std::size_t entity_idx, State& state) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    entity.last_health = entity.health;
}

void StepStunTimer(std::size_t entity_idx, State& state) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.super_state == EntitySuperState::Stunned) {
        if (entity.stun_timer == 0) {
            entity.super_state = EntitySuperState::Idle;
            TrySetDisplayState(entity, EntityDisplayState::Neutral);
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

    if (!entity.grounded && !entity.climbing) {
        return;
    }

    if (entity.travel_sound_countdown < 0.0F) {
        entity.travel_sound_countdown = kWalkerClimberTravelSoundDistInterval;

        SoundEffect which_step_sound;
        if (entity.climbing) {
            const auto [entity_tl, entity_br] = entity.GetBounds();
            const std::vector<const Tile*> newly_collided_tiles =
                state.stage.GetTilesInRectWc(ToIVec2(entity_tl), ToIVec2(entity_br));
            bool its_rope = false;
            bool its_ladder = false;
            for (const Tile* tile : newly_collided_tiles) {
                if (*tile == Tile::Rope) {
                    its_rope = true;
                }
                if (*tile == Tile::Ladder) {
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
