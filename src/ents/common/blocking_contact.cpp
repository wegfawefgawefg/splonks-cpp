#include "ents/common/common.hpp"

#include "tile.hpp"
#include "world_query.hpp"

#include <cmath>

namespace splonks::ents::common {

namespace {

int FloorDiv(int value, int divisor) {
    if (divisor == 0) {
        return 0;
    }

    int result = value / divisor;
    const int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        result -= 1;
    }
    return result;
}

bool TouchesStageBounds(const AABB& aabb, const Stage& stage) {
    if (aabb.tl.x < 0.0F && stage.IsBorderSideBlocking(StageBorderSideKind::Left)) {
        return true;
    }
    if (aabb.tl.y < 0.0F && stage.IsBorderSideBlocking(StageBorderSideKind::Top)) {
        return true;
    }
    if (aabb.br.x > static_cast<float>(stage.GetWidth() - 1) &&
        stage.IsBorderSideBlocking(StageBorderSideKind::Right)) {
        return true;
    }
    if (aabb.br.y > static_cast<float>(stage.GetHeight() - 1) &&
        stage.IsBorderSideBlocking(StageBorderSideKind::Bottom)) {
        return true;
    }
    return false;
}

bool AreDirectlyAttached(const Ent& first, const Ent& second) {
    return (first.held_by_vid.has_value() && *first.held_by_vid == second.vid) ||
           (second.held_by_vid.has_value() && *second.held_by_vid == first.vid);
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
    const AABB& aabb,
    const State& state,
    bool check_tiles,
    bool check_ents
) {
    BlockingContactSet contacts{};

    if (check_tiles) {
        contacts.touches_stage_bounds = TouchesStageBounds(aabb, state.stage);

        const IVec2 tile_tl = IVec2::New(
            FloorDiv(static_cast<int>(std::floor(aabb.tl.x)), static_cast<int>(kTileSize)),
            FloorDiv(static_cast<int>(std::floor(aabb.tl.y)), static_cast<int>(kTileSize))
        );
        const IVec2 tile_br = IVec2::New(
            FloorDiv(static_cast<int>(std::floor(aabb.br.x)), static_cast<int>(kTileSize)),
            FloorDiv(static_cast<int>(std::floor(aabb.br.y)), static_cast<int>(kTileSize))
        );
        for (const WorldTileQueryResult& tile_query : QueryTilesInRect(state.stage, tile_tl, tile_br)) {
            contacts.tile_contacts.push_back(TileContact{
                .tile_pos = tile_query.tile_pos,
                .tile = tile_query.tile,
                .blocks_movement = tile_query.tile != nullptr &&
                                   IsTileCollidable(*tile_query.tile),
            });
        }
    }

    if (check_ents) {
        const Ent& ent = state.ents.ents[ent_idx];
        const VID self_vid = ent.vid;
        const Vec2 anchor = (aabb.tl + aabb.br) / 2.0F;
        for (const VID& other_vid : QueryEntsInAabb(state, aabb, self_vid)) {
            const Ent* const other_ent = state.ents.GetEnt(other_vid);
            if (other_ent == nullptr || !other_ent->active) {
                continue;
            }
            if (AreDirectlyAttached(ent, *other_ent)) {
                continue;
            }
            const AABB other_aabb = GetNearestWorldAabb(state.stage, anchor, other_ent->GetAABB());
            if (AabbsIntersect(aabb, other_aabb)) {
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
