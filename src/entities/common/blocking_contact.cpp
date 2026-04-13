#include "entities/common/common.hpp"

#include "tile.hpp"
#include "world_query.hpp"

#include <cmath>

namespace splonks::entities::common {

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

bool AabbsIntersect(const AABB& left, const AABB& right) {
    if (left.br.x < right.tl.x) {
        return false;
    }
    if (left.tl.x > right.br.x) {
        return false;
    }
    if (left.br.y < right.tl.y) {
        return false;
    }
    if (left.tl.y > right.br.y) {
        return false;
    }
    return true;
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

ContactResolution ResolveBlockingTileContacts(const BlockingContactSet& contacts) {
    ContactResolution result{};
    if (contacts.touches_stage_bounds) {
        result.blocks_movement = true;
    }
    for (const TileContact& tile_contact : contacts.tile_contacts) {
        if (tile_contact.tile != nullptr && IsTileCollidable(*tile_contact.tile)) {
            result.blocks_movement = true;
            break;
        }
    }
    return result;
}

ContactResolution ResolveBlockingEntityContacts(
    std::size_t entity_idx,
    const BlockingContactSet& contacts,
    const State& state
) {
    ContactResolution result{};
    const VID self_vid = state.entity_manager.entities[entity_idx].vid;
    for (const VID& other_vid : contacts.entity_vids) {
        if (other_vid == self_vid) {
            continue;
        }
        const Entity* const other_entity = state.entity_manager.GetEntity(other_vid);
        if (other_entity == nullptr || !other_entity->active) {
            continue;
        }
        if (other_entity->impassable) {
            result.blocks_movement = true;
        }
    }
    return result;
}

} // namespace

BlockingContactSet GatherBlockingContactsForAabb(
    std::size_t entity_idx,
    const AABB& aabb,
    const State& state,
    bool check_tiles,
    bool check_entities
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
            });
        }
    }

    if (check_entities) {
        const VID self_vid = state.entity_manager.entities[entity_idx].vid;
        const Vec2 anchor = (aabb.tl + aabb.br) / 2.0F;
        for (const VID& other_vid : QueryEntitiesInAabb(state, aabb, self_vid)) {
            const Entity* const other_entity = state.entity_manager.GetEntity(other_vid);
            if (other_entity == nullptr || !other_entity->active) {
                continue;
            }
            const AABB other_aabb = GetNearestWorldAabb(state.stage, anchor, other_entity->GetAABB());
            if (AabbsIntersect(aabb, other_aabb)) {
                contacts.entity_vids.push_back(other_vid);
            }
        }
    }

    return contacts;
}

ContactResolution ResolveBlockingContactSet(
    std::size_t entity_idx,
    const BlockingContactSet& contacts,
    const State& state
) {
    ContactResolution result{};

    const ContactResolution tile_resolution = ResolveBlockingTileContacts(contacts);
    result.blocks_movement |= tile_resolution.blocks_movement;
    result.stop_sweep |= tile_resolution.stop_sweep;

    const ContactResolution entity_resolution =
        ResolveBlockingEntityContacts(entity_idx, contacts, state);
    result.blocks_movement |= entity_resolution.blocks_movement;
    result.stop_sweep |= entity_resolution.stop_sweep;

    return result;
}

} // namespace splonks::entities::common
