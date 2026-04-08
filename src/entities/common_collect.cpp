#include "entities/common.hpp"

namespace splonks::entities::common {

void CollectTouchingPickups(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    const AABB aabb = GetContactAabbForEntity(entity, graphics);
    const VID entity_vid = entity.vid;
    const std::vector<VID> search_results = state.sid.QueryExclude(aabb.tl, aabb.br, entity_vid);
    unsigned int money_gained = 0;
    for (const VID& e_vid : search_results) {
        if (const Entity* const pickup = state.entity_manager.GetEntity(e_vid)) {
            switch (pickup->type_) {
            case EntityType::Gold:
                money_gained = 1;
                audio.PlaySoundEffect(SoundEffect::Gold);
                break;
            case EntityType::GoldStack:
                money_gained = 2;
                audio.PlaySoundEffect(SoundEffect::GoldStack);
                break;
            default:
                money_gained = 0;
                break;
            }
        }
        if (money_gained > 0) {
            state.entity_manager.SetInactiveVid(e_vid);
        }
        Entity& mutable_entity = state.entity_manager.entities[entity_idx];
        mutable_entity.money += money_gained;
    }
}

} // namespace splonks::entities::common
