#include "render_debug.hpp"

#include "entity.hpp"
#include "graphics.hpp"
#include "room.hpp"
#include "state.hpp"
#include "text.hpp"
#include "tile.hpp"

#include <cstdio>

namespace splonks {

namespace {

SDL_FRect WorldRectToScreen(const Graphics& graphics, const Vec2& world_pos, const Vec2& world_size) {
    const Vec2 relative = world_pos - graphics.camera.target;
    const Vec2 screen = relative * graphics.camera.zoom + graphics.camera.offset;
    return SDL_FRect{
        screen.x,
        screen.y,
        world_size.x * graphics.camera.zoom,
        world_size.y * graphics.camera.zoom,
    };
}

void RenderArrow(SDL_Renderer* renderer, Vec2 pos, float length, Vec2 dir, SDL_Color color) {
    const Vec2 line_end = pos + (dir * length);
    // draw a tiny rectangle at the center
    constexpr float kRectWidth = 2.0F;
    const Vec2 rect_start = pos - (Vec2::New(kRectWidth, kRectWidth) / 2.0F);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    const SDL_FRect rect{rect_start.x, rect_start.y, kRectWidth, kRectWidth};
    SDL_RenderFillRect(renderer, &rect);
    SDL_RenderLine(renderer, pos.x, pos.y, line_end.x, line_end.y);
}

void RenderX(SDL_Renderer* renderer, Vec2 pos, float length, SDL_Color color) {
    RenderArrow(renderer, pos, length, Vec2::New(1.0F, 1.0F), color);
    RenderArrow(renderer, pos, length, Vec2::New(-1.0F, 1.0F), color);
    RenderArrow(renderer, pos, length, Vec2::New(1.0F, -1.0F), color);
    RenderArrow(renderer, pos, length, Vec2::New(-1.0F, -1.0F), color);
}

void RenderPlus(SDL_Renderer* renderer, Vec2 pos, float length, SDL_Color color) {
    RenderArrow(renderer, pos, length, Vec2::New(0.0F, 1.0F), color);
    RenderArrow(renderer, pos, length, Vec2::New(0.0F, -1.0F), color);
    RenderArrow(renderer, pos, length, Vec2::New(1.0F, 0.0F), color);
    RenderArrow(renderer, pos, length, Vec2::New(-1.0F, 0.0F), color);
}

} // namespace

void PrintCtrlsHelp(SDL_Renderer* renderer, Graphics& graphics, unsigned int screen_bottom) {
    // print character hanging
    DrawText(
        renderer,
        graphics,
        18,
        graphics.ui_font,
        "ctrls: wasd, 3 to regen map, r to warp to entrance. d while hanging drops.",
        0.0F,
        static_cast<float>(screen_bottom),
        SDL_Color{255, 255, 255, 255}
    );
    DrawText(
        renderer,
        graphics,
        18,
        graphics.ui_font,
        "get to exit to reset.",
        0.0F,
        static_cast<float>(screen_bottom + 18),
        SDL_Color{255, 255, 255, 255}
    );
}

void RenderStageLayout(SDL_Renderer* renderer, Graphics& graphics, const State& state) {
    // draw the origin with lines going in all directions
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderLine(renderer, 0.0F, 0.0F, 100.0F, 0.0F);
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderLine(renderer, 0.0F, 0.0F, 0.0F, 100.0F);
    //  draw the boundaries of the current stage
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    const SDL_FRect stage_rect{
        0.0F,
        0.0F,
        static_cast<float>(state.stage.GetStageDims().x),
        static_cast<float>(state.stage.GetStageDims().y),
    };
    SDL_RenderRect(renderer, &stage_rect);
    // draw the boundaries of every room in the current stage
    for (unsigned int y = 0; y < Stage::kRoomLayout.y; ++y) {
        for (unsigned int x = 0; x < Stage::kRoomLayout.x; ++x) {
            const UVec2 room_pos = UVec2::New(x, y) * state.stage.GetRoomDims();

            SDL_SetRenderDrawColor(renderer, 255, 105, 180, 255);
            const SDL_FRect room_rect{
                static_cast<float>(room_pos.x),
                static_cast<float>(room_pos.y),
                static_cast<float>(Stage::kRoomShape.x * kTileSize),
                static_cast<float>(Stage::kRoomShape.y * kTileSize),
            };
            SDL_RenderRect(renderer, &room_rect);

            // draw in tiny text the room number at the top left of the room
            char room_text[32];
            std::snprintf(room_text, sizeof(room_text), "(%u, %u)", x, y);
            DrawText(
                renderer,
                graphics,
                10,
                graphics.ui_font,
                room_text,
                static_cast<float>(room_pos.x),
                static_cast<float>(room_pos.y),
                SDL_Color{255, 105, 180, 255}
            );
            // below that draw the room boundaries
            char bounds_text[32];
            std::snprintf(bounds_text, sizeof(bounds_text), "(%u, %u)", room_pos.x, room_pos.y);
            DrawText(
                renderer,
                graphics,
                10,
                graphics.ui_font,
                bounds_text,
                static_cast<float>(room_pos.x),
                static_cast<float>(room_pos.y + 11U),
                SDL_Color{255, 105, 180, 255}
            );
        }
    }
}

void RenderRoomsOverlay(SDL_Renderer* renderer, Graphics& graphics, const State& state) {
    for (unsigned int y = 0; y < Stage::kRoomLayout.y; ++y) {
        for (unsigned int x = 0; x < Stage::kRoomLayout.x; ++x) {
            const UVec2 room_pos = UVec2::New(x, y) * state.stage.GetRoomDims();
            const UVec2 room_shape = state.stage.GetRoomDims();
            const RoomType room_type =
                static_cast<RoomType>(state.stage.rooms[static_cast<std::size_t>(y)]
                                                      [static_cast<std::size_t>(x)]);
            const UVec2 room_center = room_pos + (room_shape / 2U);
            const float arrow_length = static_cast<float>(kTileSize);
            const SDL_Color col{255, 0, 0, 255};
            switch (room_type) {
            case RoomType::LeftUpRight:
                RenderArrow(renderer, ToVec2(room_center), arrow_length, Vec2::New(-1.0F, 0.0F), col);
                RenderArrow(renderer, ToVec2(room_center), arrow_length, Vec2::New(1.0F, 0.0F), col);
                RenderArrow(renderer, ToVec2(room_center), arrow_length, Vec2::New(0.0F, -1.0F), col);
                break;
            case RoomType::LeftDownRight:
                RenderArrow(renderer, ToVec2(room_center), arrow_length, Vec2::New(-1.0F, 0.0F), col);
                RenderArrow(renderer, ToVec2(room_center), arrow_length, Vec2::New(1.0F, 0.0F), col);
                RenderArrow(renderer, ToVec2(room_center), arrow_length, Vec2::New(0.0F, 1.0F), col);
                break;
            case RoomType::LeftRight:
                RenderArrow(renderer, ToVec2(room_center), arrow_length, Vec2::New(-1.0F, 0.0F), col);
                RenderArrow(renderer, ToVec2(room_center), arrow_length, Vec2::New(1.0F, 0.0F), col);
                break;
            case RoomType::FourWay:
                RenderPlus(renderer, ToVec2(room_center), arrow_length, col);
                break;
            case RoomType::Box:
                RenderX(renderer, ToVec2(room_center), arrow_length, col);
                break;
            case RoomType::Exit:
                // put the word exit here
                DrawText(
                    renderer,
                    graphics,
                    static_cast<int>(kTileSize),
                    graphics.ui_font,
                    "exit",
                    static_cast<float>(room_center.x - kTileSize),
                    static_cast<float>(room_center.y - kTileSize),
                    SDL_Color{255, 0, 0, 255}
                );
                break;
            case RoomType::Entrance:
                // put the word entrance here
                DrawText(
                    renderer,
                    graphics,
                    static_cast<int>(kTileSize),
                    graphics.ui_font,
                    "entrance",
                    static_cast<float>(room_center.x - kTileSize),
                    static_cast<float>(room_center.y - kTileSize),
                    SDL_Color{255, 0, 0, 255}
                );
                break;
            }
        }
    }
}

void RenderEntityCollisionBoxes(SDL_Renderer* renderer, Graphics& graphics, const State& state) {
    for (const Entity& entity : state.entity_manager.entities) {
        if (!entity.active) {
            continue;
        }

        const AABB aabb = entity.GetAABB();
        const Vec2 size = aabb.br - aabb.tl + Vec2::New(1.0F, 1.0F);
        const SDL_FRect rect = WorldRectToScreen(graphics, aabb.tl, size);

        SDL_Color color = SDL_Color{255, 255, 0, 255};
        if (entity.type_ == EntityType::Player) {
            color = SDL_Color{64, 255, 64, 255};
        } else if (!entity.can_collide) {
            color = SDL_Color{255, 180, 64, 255};
        }

        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderRect(renderer, &rect);
    }
}

} // namespace splonks
