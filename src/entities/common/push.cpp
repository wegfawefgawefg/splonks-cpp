#include "entities/common/common.hpp"

#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>

namespace splonks::entities::common {

namespace {

std::optional<IVec2> GetPushDirectionForCrusherContact(const ContactContext& context) {
    if (context.impact_axis == BlockingImpactAxis::Horizontal) {
        if (context.direction > 0) {
            return IVec2::New(1, 0);
        }
        if (context.direction < 0) {
            return IVec2::New(-1, 0);
        }
        return std::nullopt;
    }

    if (context.direction > 0) {
        return IVec2::New(0, 1);
    }
    if (context.direction < 0) {
        return IVec2::New(0, -1);
    }
    return std::nullopt;
}

bool IsCrusherPushTarget(const Entity& entity) {
    if (!entity.active || !entity.can_collide || entity.impassable) {
        return false;
    }
    return true;
}

bool IsAtCrusherLeadingFace(
    const Stage& stage,
    const Entity& crusher,
    const Entity& other_entity,
    const IVec2& push_direction
) {
    const AABB crusher_aabb = crusher.GetAABB();
    const AABB other_aabb =
        GetNearestWorldAabb(stage, crusher.GetCenter(), other_entity.GetAABB());
    const Vec2 crusher_center = crusher.GetCenter();
    const Vec2 other_center = (other_aabb.tl + other_aabb.br) / 2.0F;

    const float overlap_x = std::min(crusher_aabb.br.x, other_aabb.br.x) -
                            std::max(crusher_aabb.tl.x, other_aabb.tl.x);
    const float overlap_y = std::min(crusher_aabb.br.y, other_aabb.br.y) -
                            std::max(crusher_aabb.tl.y, other_aabb.tl.y);

    if (push_direction.x > 0) {
        return overlap_y >= 0.0F && other_center.x >= crusher_center.x;
    }
    if (push_direction.x < 0) {
        return overlap_y >= 0.0F && other_center.x <= crusher_center.x;
    }
    if (push_direction.y > 0) {
        return overlap_x >= 0.0F && other_center.y >= crusher_center.y;
    }
    if (push_direction.y < 0) {
        return overlap_x >= 0.0F && other_center.y <= crusher_center.y;
    }

    return false;
}

} // namespace

bool TryApplyPushEntityAction(
    VID pusher_vid,
    VID pushed_vid,
    float push_acc_delta,
    State& state,
    const Graphics& graphics
) {
    if (push_acc_delta == 0.0F) {
        return false;
    }

    Entity* const pusher = state.entity_manager.GetEntityMut(pusher_vid);
    Entity* const pushed = state.entity_manager.GetEntityMut(pushed_vid);
    if (pusher == nullptr || pushed == nullptr || !pusher->active || !pushed->active ||
        !pushed->pushable || pusher->condition == EntityCondition::Dead || !pusher->grounded) {
        return false;
    }

    const AABB pusher_aabb = GetContactAabbForEntity(*pusher, graphics);
    const AABB push_zone{
        .tl = pusher_aabb.tl - Vec2::New(6.0F, 0.0F),
        .br = pusher_aabb.br + Vec2::New(6.0F, 0.0F),
    };
    const Vec2 pusher_center = (pusher_aabb.tl + pusher_aabb.br) / 2.0F;
    const AABB pushed_aabb = GetNearestWorldAabb(
        state.stage,
        pusher_center,
        GetContactAabbForEntity(*pushed, graphics)
    );
    if (!AabbsIntersect(push_zone, pushed_aabb)) {
        return false;
    }

    const Vec2 pushed_center = (pushed_aabb.tl + pushed_aabb.br) / 2.0F;
    if (push_acc_delta > 0.0F && pushed_center.x < pusher_center.x) {
        return false;
    }
    if (push_acc_delta < 0.0F && pushed_center.x > pusher_center.x) {
        return false;
    }

    pushed->acc.x += push_acc_delta;
    world_ops::PatchEntityState(state, *pusher, *pushed);
    return true;
}

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
            QueryEntitiesInAabb(state, try_to_push_zone, entity_vid);
        for (const VID& vid : search_results) {
            const Entity& candidate = state.entity_manager.entities[vid.id];
            if (!candidate.pushable) {
                continue;
            }
            if (Entity* const block_entity = state.entity_manager.GetEntityMut(vid)) {
                ready_to_push = true;
                const float push_zone_left_x = entity_aabb.tl.x - 1.0F;
                const float push_zone_right_x = entity_aabb.br.x + 1.0F;
                const AABB nearest_block_aabb =
                    GetNearestWorldAabb(state.stage, entity.GetCenter(), block_entity->GetAABB());
                const Vec2 block_tl = nearest_block_aabb.tl;
                const Vec2 block_br = nearest_block_aabb.br;
                float block_x_acc_delta = 0.0F;
                if (entity_vel.x > 0.0F && block_br.x > push_zone_left_x &&
                    block_tl.x > push_zone_left_x) {
                    block_x_acc_delta = block_entity->push_acc;
                } else if (entity_vel.x < 0.0F && block_tl.x < push_zone_right_x &&
                           block_br.x < push_zone_right_x) {
                    block_x_acc_delta = -block_entity->push_acc;
                }
                if (block_x_acc_delta != 0.0F) {
                    block_entity->acc.x += block_x_acc_delta;
                    world_ops::PatchEntityState(state, entity, *block_entity);
                }
                break;
            }
        }
    }

    SetMovementFlag(entity, EntityMovementFlag::Pushing, ready_to_push);
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
    state.stage.NormalizeEntityPositionForWrap(entity);
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

bool TryApplyCrusherPusherContact(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const ContactContext& context,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    if (entity_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    const Entity& crusher = state.entity_manager.entities[entity_idx];
    if (!context.mover_vid.has_value() || crusher.vid != *context.mover_vid) {
        return false;
    }

    const std::optional<IVec2> push_direction = GetPushDirectionForCrusherContact(context);
    if (!push_direction.has_value()) {
        return false;
    }

    Entity& other_entity = state.entity_manager.entities[other_entity_idx];
    if (!IsCrusherPushTarget(other_entity)) {
        return false;
    }
    if (!IsAtCrusherLeadingFace(state.stage, crusher, other_entity, *push_direction)) {
        return false;
    }

    if (TryDisplaceEntityByOnePixel(other_entity_idx, *push_direction, state, graphics, &audio)) {
        return true;
    }

    TryDamageEntity(
        other_entity_idx,
        state,
        audio,
        DamageType::Crush,
        1,
        DamageOptions{
            .source_vid = crusher.vid,
            .allow_remote_player_target = true,
        }
    );
    return true;
}

} // namespace splonks::entities::common
