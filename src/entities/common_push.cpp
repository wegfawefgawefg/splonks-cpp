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

bool TryDisplaceEntityByOnePixel(
    std::size_t entity_idx,
    const IVec2& direction,
    State& state,
    const Graphics& graphics,
    Audio* audio
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }
    if (direction.x == 0 && direction.y == 0) {
        return false;
    }

    Entity& entity = state.entity_manager.entities[entity_idx];
    if (!entity.active) {
        return false;
    }

    const Vec2 candidate_pos = entity.pos + ToVec2(direction);
    const AABB candidate_aabb = AABB::New(
        candidate_pos, candidate_pos + entity.size - Vec2::New(1.0F, 1.0F));
    const BlockingContactSet contacts =
        GatherBlockingContactsForAabb(entity_idx, candidate_aabb, state, true, true);
    if (ResolveBlockingContactSet(entity_idx, contacts, state).blocks_movement) {
        return false;
    }

    entity.pos = candidate_pos;
    state.UpdateSidForEntity(entity_idx, graphics);
    if (audio != nullptr) {
        TryDispatchEntityEntityOverlapContacts(
            entity_idx,
            state,
            graphics,
            *audio,
            ContactContext{
                .phase = ContactPhase::SweptEntered,
                .has_impact = false,
                .mover_vid = entity.vid,
            }
        );
    }
    return true;
}

} // namespace splonks::entities::common
