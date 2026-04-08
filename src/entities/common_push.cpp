#include "entities/common.hpp"

#include "entities/block.hpp"

namespace splonks::entities::common {

void TryPushBlocks(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics
) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    const bool entity_grounded = entity.grounded;
    const AABB entity_aabb = GetContactAabbForEntity(entity, graphics);
    const VID entity_vid = entity.vid;
    const Vec2 entity_vel = entity.vel;

    bool ready_to_push = false;
    if (entity_grounded) {
        const AABB try_to_push_zone = {
            .tl = entity_aabb.tl - Vec2::New(6.0F, 0.0F),
            .br = entity_aabb.br + Vec2::New(6.0F, 0.0F),
        };
        const std::vector<VID> search_results =
            state.sid.QueryExclude(try_to_push_zone.tl, try_to_push_zone.br, entity_vid);
        for (const VID& vid : search_results) {
            const Entity& candidate = state.entity_manager.entities[vid.id];
            if (candidate.type_ != EntityType::Block) {
                continue;
            }
            if (Entity* const block_entity = state.entity_manager.GetEntityMut(vid)) {
                ready_to_push = true;
                const float push_zone_left_x = entity_aabb.tl.x - 1.0F;
                const float push_zone_right_x = entity_aabb.br.x + 1.0F;
                const auto [block_tl, block_br] = block_entity->GetBounds();
                float block_x_acc_delta = 0.0F;
                if (entity_vel.x > 0.0F && block_br.x > push_zone_left_x &&
                    block_tl.x > push_zone_left_x) {
                    block_x_acc_delta = block::kBlockPushAcc;
                } else if (entity_vel.x < 0.0F && block_tl.x < push_zone_right_x &&
                           block_br.x < push_zone_right_x) {
                    block_x_acc_delta = -block::kBlockPushAcc;
                }
                block_entity->acc.x += block_x_acc_delta;
                break;
            }
        }
    }

    if (ready_to_push) {
        entity.state = EntityState::Pushing;
    } else if (entity.state == EntityState::Pushing) {
        entity.state = EntityState::Idle;
    }
}

} // namespace splonks::entities::common
