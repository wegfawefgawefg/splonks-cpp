#include "ents/common/common.hpp"

#include "tile.hpp"
#include "world_query.hpp"

#include <algorithm>

namespace splonks::ents::common {

namespace {

bool TouchesStageBounds(FxAABB aabb, const Stage& stage) {
    return AabbTouchesBlockingStageBounds(stage, aabb);
}

bool AreDirectlyAttached(const Ent& first, const Ent& second) {
    return (first.held_by_vid.has_value() && *first.held_by_vid == second.vid) ||
           (second.held_by_vid.has_value() && *second.held_by_vid == first.vid);
}

bool TileContactLess(const TileContact& left, const TileContact& right) {
    if (left.tile_pos.y != right.tile_pos.y) {
        return left.tile_pos.y < right.tile_pos.y;
    }
    return left.tile_pos.x < right.tile_pos.x;
}

ContactResult ResolveBlockingTileContacts(const BlockingContactSet& contacts) {
    ContactResult result{};
    if (contacts.touches_stage_bounds) {
        result.blocks_movement = true;
    }
    for (const TileContact& tile_contact : contacts.tile_contacts) {
        if (tile_contact.blocks_movement) {
            result.blocks_movement = true;
            break;
        }
    }
    return result;
}

ContactResult ResolveBlockingEntContacts(
    std::size_t ent_idx,
    const BlockingContactSet& contacts,
    const State& state
) {
    ContactResult result{};
    const VID self_vid = state.ents.ents[ent_idx].vid;
    for (const VID& other_vid : contacts.ent_vids) {
        if (other_vid == self_vid) {
            continue;
        }
        const Ent* const other_ent = state.ents.GetEnt(other_vid);
        if (other_ent == nullptr || !other_ent->active) {
            continue;
        }
        if (other_ent->impassable) {
            result.blocks_movement = true;
        }
    }
    return result;
}

} // namespace

BlockingContactSet GatherBlockingContactsForAabb(
    std::size_t ent_idx,
    FxAABB aabb,
    const State& state,
    bool check_tiles,
    bool check_ents
) {
    BlockingContactSet contacts{};

    if (check_tiles) {
        contacts.touches_stage_bounds = TouchesStageBounds(aabb, state.stage);

        for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(state.stage, aabb)) {
            contacts.tile_contacts.push_back(TileContact{
                .tile_pos = tile_query.tile_pos,
                .tile = tile_query.tile,
                .blocks_movement = tile_query.tile != nullptr &&
                                   IsTileCollidable(*tile_query.tile),
            });
        }
        std::sort(
            contacts.tile_contacts.begin(),
            contacts.tile_contacts.end(),
            TileContactLess
        );
    }

    if (check_ents) {
        const Ent& ent = state.ents.ents[ent_idx];
        const VID self_vid = ent.vid;
        const FxVec2 anchor = aabb.center();
        for (const VID& other_vid : QueryEntsInAabb(state, aabb, self_vid)) {
            const Ent* const other_ent = state.ents.GetEnt(other_vid);
            if (other_ent == nullptr || !other_ent->active) {
                continue;
            }
            if (AreDirectlyAttached(ent, *other_ent)) {
                continue;
            }
            const FxAABB other_aabb =
                GetNearestWorldAabb(state.stage, anchor, other_ent->GetAABB());
            if (gfxp::aabbs_intersect(aabb, other_aabb)) {
                contacts.ent_vids.push_back(other_vid);
            }
        }
    }

    return contacts;
}

ContactResult ResolveBlockingContactSet(
    std::size_t ent_idx,
    const BlockingContactSet& contacts,
    const State& state
) {
    ContactResult result{};

    const ContactResult tile_resolution = ResolveBlockingTileContacts(contacts);
    result.blocks_movement |= tile_resolution.blocks_movement;
    result.stop_sweep |= tile_resolution.stop_sweep;

    const ContactResult ent_resolution =
        ResolveBlockingEntContacts(ent_idx, contacts, state);
    result.blocks_movement |= ent_resolution.blocks_movement;
    result.stop_sweep |= ent_resolution.stop_sweep;

    return result;
}

} // namespace splonks::ents::common
