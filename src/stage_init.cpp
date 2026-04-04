#include "stage_init.hpp"

#include "entities/player.hpp"

#include <stdexcept>

namespace splonks {

void InitStage(State& state) {
    state.stage_frame = 0;
    state.entity_manager.ClearAllEntities();

    const IVec2 starting_room = state.stage.GetStartingRoom();
    const UVec2 starting_room_tl = ToUVec2(starting_room) * Stage::kRoomShape;
    const UVec2 starting_room_br = starting_room_tl + Stage::kRoomShape;

    if (const std::optional<VID> player_vid = state.entity_manager.NewEntity()) {
        state.player_vid = player_vid;
        if (Entity* const player = state.entity_manager.GetEntityMut(*player_vid)) {
            entities::player::SetEntityPlayer(*player);
        }
    }

    bool door_found = false;
    for (unsigned int y = starting_room_tl.y; y < starting_room_br.y && !door_found; ++y) {
        for (unsigned int x = starting_room_tl.x; x < starting_room_br.x; ++x) {
            if (state.stage.GetTile(x, y) == Tile::Entrance) {
                if (state.player_vid) {
                    if (Entity* const player = state.entity_manager.GetEntityMut(*state.player_vid)) {
                        player->pos = Vec2::New(static_cast<float>(x), static_cast<float>(y)) *
                                      static_cast<float>(kTileSize);
                        player->vel = Vec2::New(0.0F, 0.0F);
                    }
                }
                door_found = true;
                break;
            }
        }
    }

    if (!door_found) {
        throw std::runtime_error(
            "No door found in starting room. You have a game breaking bug in the map generation "
            "code. Don't ship with this in.");
    }
}

} // namespace splonks
