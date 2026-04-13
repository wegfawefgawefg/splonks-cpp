#include "render/stone_overlay.hpp"

#include "graphics.hpp"
#include "state.hpp"
#include "tile.hpp"
#include "tile_source_data.hpp"

#include <cmath>

namespace splonks {

namespace {

Vec2 WorldToScreen(const Graphics& graphics, const Vec2& world_pos) {
    const Vec2 screen =
        ((world_pos - graphics.camera.target) * graphics.camera.zoom) + graphics.camera.offset;
    return Vec2::New(std::round(screen.x), std::round(screen.y));
}

SDL_FRect WorldRectToScreen(const Graphics& graphics, const Vec2& world_pos, const Vec2& world_size) {
    const Vec2 screen_pos = WorldToScreen(graphics, world_pos);
    const Vec2 screen_size = Vec2::New(
        std::round(world_size.x * graphics.camera.zoom),
        std::round(world_size.y * graphics.camera.zoom)
    );
    return SDL_FRect{
        screen_pos.x,
        screen_pos.y,
        screen_size.x,
        screen_size.y,
    };
}

SDL_Rect ToScreenClipRect(const SDL_FRect& rect) {
    return SDL_Rect{
        static_cast<int>(std::floor(rect.x)),
        static_cast<int>(std::floor(rect.y)),
        static_cast<int>(std::ceil(rect.w)),
        static_cast<int>(std::ceil(rect.h)),
    };
}

} // namespace

void RenderStoneEntityOverlay(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    const Vec2& render_position,
    const Vec2& render_size
) {
    (void)state;

    if (render_size.x <= 0.0F || render_size.y <= 0.0F) {
        return;
    }

    const SDL_FRect overlay_dst = WorldRectToScreen(graphics, render_position, render_size);
    const SDL_Rect clip_rect = ToScreenClipRect(overlay_dst);
    SDL_SetRenderClipRect(renderer, &clip_rect);

    const int tile_size = static_cast<int>(kTileSize);
    const int tiles_wide = static_cast<int>(std::ceil(render_size.x / static_cast<float>(tile_size)));
    const int tiles_high = static_cast<int>(std::ceil(render_size.y / static_cast<float>(tile_size)));
    const IVec2 variation_anchor = ToIVec2(render_position);
    for (int tile_y = 0; tile_y < tiles_high; ++tile_y) {
        for (int tile_x = 0; tile_x < tiles_wide; ++tile_x) {
            const IVec2 tile_world_pos = IVec2::New(
                static_cast<int>(render_position.x) + (tile_x * tile_size),
                static_cast<int>(render_position.y) + (tile_y * tile_size)
            );
            const TileSourceData* const tile_source_data =
                GetTileSourceData(
                    graphics,
                    Tile::CaveBlock,
                    variation_anchor + IVec2::New(tile_x * tile_size, tile_y * tile_size)
                );
            if (tile_source_data == nullptr) {
                continue;
            }

            SDL_Texture* const tile_texture = GetTileTexture(graphics, *tile_source_data);
            if (tile_texture == nullptr) {
                continue;
            }

            SDL_SetTextureBlendMode(tile_texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(tile_texture, 180);

            const SDL_FRect src{
                static_cast<float>(tile_source_data->sample_rect.x),
                static_cast<float>(tile_source_data->sample_rect.y),
                static_cast<float>(tile_source_data->sample_rect.w),
                static_cast<float>(tile_source_data->sample_rect.h),
            };
            const SDL_FRect dst = WorldRectToScreen(
                graphics,
                ToVec2(tile_world_pos),
                Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
            );
            SDL_RenderTexture(renderer, tile_texture, &src, &dst);
            SDL_SetTextureAlphaMod(tile_texture, 255);
        }
    }

    SDL_SetRenderClipRect(renderer, nullptr);
}

} // namespace splonks
