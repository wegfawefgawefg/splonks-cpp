#include "entities/common/common.hpp"

#include "tile.hpp"

namespace splonks::entities::common {

namespace {

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
    if (aabb.tl.x < 0.0F || aabb.tl.y < 0.0F) {
        return true;
    }
    if (aabb.br.x > static_cast<float>(stage.GetWidth() - 1)) {
        return true;
    }
    if (aabb.br.y > static_cast<float>(stage.GetHeight() - 1)) {
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

        const IVec2 tile_tl = ToIVec2(aabb.tl) / static_cast<int>(kTileSize);
        const IVec2 tile_br = ToIVec2(aabb.br) / static_cast<int>(kTileSize);
        for (int y = tile_tl.y; y <= tile_br.y; ++y) {
            for (int x = tile_tl.x; x <= tile_br.x; ++x) {
                if (x < 0 || y < 0) {
                    continue;
                }
                if (x >= static_cast<int>(state.stage.GetTileWidth()) ||
                    y >= static_cast<int>(state.stage.GetTileHeight())) {
                    continue;
                }
                contacts.tile_contacts.push_back(TileContact{
                    .tile_pos = IVec2::New(x, y),
                    .tile = &state.stage.GetTile(static_cast<unsigned int>(x), static_cast<unsigned int>(y)),
                });
            }
        }
    }

    if (check_entities) {
        const VID self_vid = state.entity_manager.entities[entity_idx].vid;
        for (const VID& other_vid : state.sid.QueryExclude(aabb.tl, aabb.br, self_vid)) {
            const Entity* const other_entity = state.entity_manager.GetEntity(other_vid);
            if (other_entity == nullptr || !other_entity->active) {
                continue;
            }
            if (AabbsIntersect(aabb, other_entity->GetAABB())) {
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
