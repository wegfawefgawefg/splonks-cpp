#include "entities/common.hpp"

namespace splonks::entities::common {

bool TryStompEntitiesBelow(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio,
    float bounce_velocity
) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    const VID entity_vid = entity.vid;
    const AABB aabb = GetContactAabbForEntity(entity, graphics);
    const AABB jump_foot = {
        .tl = Vec2::New(aabb.tl.x, aabb.br.y - 2.0F),
        .br = aabb.br,
    };

    bool jumped = false;
    const std::vector<VID> results =
        state.sid.QueryExclude(jump_foot.tl, jump_foot.br, entity_vid);
    if (!results.empty()) {
        if (Entity* const other_entity = state.entity_manager.GetEntityMut(results.front())) {
            if (!other_entity->impassable && other_entity->can_collide &&
                other_entity->super_state != EntitySuperState::Dead &&
                other_entity->super_state != EntitySuperState::Stunned &&
                other_entity->alignment == Alignment::Enemy) {
                jumped = true;

                audio.PlaySoundEffect(SoundEffect::Jump);

                const DamageResult damage_result = TryToDamageEntity(
                    other_entity->vid.id,
                    state,
                    audio,
                    DamageType::JumpOn,
                    1
                );
                if (Entity* const jumped_entity =
                        state.entity_manager.GetEntityMut(results.front())) {
                    if (jumped_entity->can_be_stunned) {
                        jumped_entity->super_state = EntitySuperState::Stunned;
                        jumped_entity->stun_timer = kDefaultStunTimer;
                    }
                    if (damage_result == DamageResult::Died ||
                        damage_result == DamageResult::Hurt) {
                        jumped_entity->thrown_by = entity_vid;
                        jumped_entity->thrown_immunity_timer = kThrownByImmunityDuration;
                    }
                }
            }
        }
    }

    // TODO: This is still snapshot-based contact, not swept stomp detection.
    if (jumped) {
        Entity& mutable_entity = state.entity_manager.entities[entity_idx];
        mutable_entity.vel.y = -bounce_velocity;
    }
    return jumped;
}

} // namespace splonks::entities::common
