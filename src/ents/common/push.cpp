#include "ents/common/common.hpp"

#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>

namespace splonks::ents::common {

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

bool IsCrusherPushTarget(const Ent& ent) {
    if (!ent.active || !ent.can_collide || ent.impassable) {
        return false;
    }
    return true;
}

bool IsAtCrusherLeadingFace(
    const Stage& stage,
    const Ent& crusher,
    const Ent& other_ent,
    const IVec2& push_direction
) {
    const AABB crusher_aabb = crusher.GetAABB();
    const AABB other_aabb =
        GetNearestWorldAabb(stage, crusher.GetCenter(), other_ent.GetAABB());
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

bool TryApplyPushEntAction(
    VID pusher_vid,
    VID pushed_vid,
    float push_acc_delta,
    State& state,
    const Graphics& graphics
) {
    if (push_acc_delta == 0.0F) {
        return false;
    }

    Ent* const pusher = state.ents.GetEntMut(pusher_vid);
    Ent* const pushed = state.ents.GetEntMut(pushed_vid);
    if (pusher == nullptr || pushed == nullptr || !pusher->active || !pushed->active ||
        !pushed->pushable || pusher->condition == EntCondition::Dead || !pusher->grounded) {
        return false;
    }

    const AABB pusher_aabb = GetContactAabbForEnt(*pusher, graphics);
    const AABB push_zone{
        .tl = pusher_aabb.tl - Vec2::New(6.0F, 0.0F),
        .br = pusher_aabb.br + Vec2::New(6.0F, 0.0F),
    };
    const Vec2 pusher_center = (pusher_aabb.tl + pusher_aabb.br) / 2.0F;
    const AABB pushed_aabb = GetNearestWorldAabb(
        state.stage,
        pusher_center,
        GetContactAabbForEnt(*pushed, graphics)
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
    return true;
}

void TryPushBlocks(
    std::size_t ent_idx,
    State& state,
    const Graphics& graphics
) {
    Ent& ent = state.ents.ents[ent_idx];
    const bool ent_grounded = ent.grounded;
    const AABB ent_aabb = GetContactAabbForEnt(ent, graphics);
    const VID ent_vid = ent.vid;
    const Vec2 ent_vel = ent.vel;

    bool ready_to_push = false;
    if (ent_grounded) {
        const AABB try_to_push_zone = {
            .tl = ent_aabb.tl - Vec2::New(6.0F, 0.0F),
            .br = ent_aabb.br + Vec2::New(6.0F, 0.0F),
        };
        const std::vector<VID> search_results =
            QueryEntsInAabb(state, try_to_push_zone, ent_vid);
        for (const VID& vid : search_results) {
            const Ent& candidate = state.ents.ents[vid.id];
            if (!candidate.pushable) {
                continue;
            }
            if (Ent* const block_ent = state.ents.GetEntMut(vid)) {
                ready_to_push = true;
                const float push_zone_left_x = ent_aabb.tl.x - 1.0F;
                const float push_zone_right_x = ent_aabb.br.x + 1.0F;
                const AABB nearest_block_aabb =
                    GetNearestWorldAabb(state.stage, ent.GetCenter(), block_ent->GetAABB());
                const Vec2 block_tl = nearest_block_aabb.tl;
                const Vec2 block_br = nearest_block_aabb.br;
                float block_x_acc_delta = 0.0F;
                if (ent_vel.x > 0.0F && block_br.x > push_zone_left_x &&
                    block_tl.x > push_zone_left_x) {
                    block_x_acc_delta = sim::ToRenderScalar(block_ent->push_acc);
                } else if (ent_vel.x < 0.0F && block_tl.x < push_zone_right_x &&
                           block_br.x < push_zone_right_x) {
                    block_x_acc_delta = -sim::ToRenderScalar(block_ent->push_acc);
                }
                if (block_x_acc_delta != 0.0F) {
                    block_ent->acc.x += block_x_acc_delta;
                }
                break;
            }
        }
    }

    SetMovementFlag(ent, EntMovementFlag::Pushing, ready_to_push);
}

bool TryDisplaceEntByOnePixel(
    std::size_t ent_idx,
    const IVec2& direction,
    State& state,
    const Graphics& graphics,
    Audio* audio
) {
    if (ent_idx >= state.ents.ents.size()) {
        return false;
    }
    if (direction.x == 0 && direction.y == 0) {
        return false;
    }

    Ent& ent = state.ents.ents[ent_idx];
    if (!ent.active) {
        return false;
    }

    const Vec2 candidate_pos = ent.pos + ToVec2(direction);
    const AABB candidate_aabb = AABB::New(
        candidate_pos, candidate_pos + ent.GetSize() - Vec2::New(1.0F, 1.0F));
    const BlockingContactSet contacts =
        GatherBlockingContactsForAabb(ent_idx, candidate_aabb, state, true, true);
    if (ResolveBlockingContactSet(ent_idx, contacts, state).blocks_movement) {
        return false;
    }

    ent.pos = candidate_pos;
    state.stage.NormalizeEntPositionForWrap(ent);
    state.UpdateSidForEnt(ent_idx, graphics);
    if (audio != nullptr) {
        TryDispatchEntEntOverlapContacts(
            ent_idx,
            state,
            graphics,
            *audio,
            ContactContext{
                .phase = ContactPhase::SweptEntered,
                .has_impact = false,
                .mover_vid = ent.vid,
            }
        );
    }
    return true;
}

bool TryApplyCrusherPusherContact(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const ContactContext& context,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    if (ent_idx >= state.ents.ents.size() ||
        other_ent_idx >= state.ents.ents.size()) {
        return false;
    }

    const Ent& crusher = state.ents.ents[ent_idx];
    if (!context.mover_vid.has_value() || crusher.vid != *context.mover_vid) {
        return false;
    }

    const std::optional<IVec2> push_direction = GetPushDirectionForCrusherContact(context);
    if (!push_direction.has_value()) {
        return false;
    }

    Ent& other_ent = state.ents.ents[other_ent_idx];
    if (!IsCrusherPushTarget(other_ent)) {
        return false;
    }
    if (!IsAtCrusherLeadingFace(state.stage, crusher, other_ent, *push_direction)) {
        return false;
    }

    if (TryDisplaceEntByOnePixel(other_ent_idx, *push_direction, state, graphics, &audio)) {
        return true;
    }

    TryDamageEnt(
        other_ent_idx,
        state,
        audio,
        DamageType::Crush,
        1,
        DamageOptions{
            .source_vid = crusher.vid,
        }
    );
    return true;
}

} // namespace splonks::ents::common
