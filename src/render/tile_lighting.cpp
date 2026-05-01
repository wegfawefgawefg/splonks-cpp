#include "render/tile_lighting.hpp"

#include "graphics.hpp"
#include "settings.hpp"
#include "state.hpp"
#include "stage_lighting.hpp"
#include "tile.hpp"
#include "tile_archetype.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace splonks {

namespace {

bool IsForegroundSolidTile(Tile tile) {
    return GetTileArchetype(tile).solid;
}

Tile GetTileForLighting(const State& state, int tile_x, int tile_y) {
    if (tile_x < 0 || tile_y < 0 || tile_x >= static_cast<int>(state.stage.GetTileWidth()) ||
        tile_y >= static_cast<int>(state.stage.GetTileHeight())) {
        return state.stage.GetTileOrBorder(tile_x, tile_y);
    }
    return state.stage.tiles[static_cast<std::size_t>(tile_y)][static_cast<std::size_t>(tile_x)];
}

SDL_FColor MakeMultiplyShadeColor(float shade_amount) {
    const float factor = std::clamp(1.0F - shade_amount, 0.0F, 1.0F);
    return SDL_FColor{factor, factor, factor, 1.0F};
}

void DrawGradientQuad(
    SDL_Renderer* renderer,
    const SDL_FRect& dst,
    SDL_BlendMode blend_mode,
    const SDL_FColor& top_left,
    const SDL_FColor& top_right,
    const SDL_FColor& bottom_right,
    const SDL_FColor& bottom_left
) {
    if (dst.w <= 0.0F || dst.h <= 0.0F) {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, blend_mode);

    const std::array<SDL_Vertex, 4> vertices{
        SDL_Vertex{SDL_FPoint{dst.x, dst.y}, top_left, SDL_FPoint{0.0F, 0.0F}},
        SDL_Vertex{SDL_FPoint{dst.x + dst.w, dst.y}, top_right, SDL_FPoint{0.0F, 0.0F}},
        SDL_Vertex{
            SDL_FPoint{dst.x + dst.w, dst.y + dst.h},
            bottom_right,
            SDL_FPoint{0.0F, 0.0F},
        },
        SDL_Vertex{SDL_FPoint{dst.x, dst.y + dst.h}, bottom_left, SDL_FPoint{0.0F, 0.0F}},
    };
    constexpr std::array<int, 6> indices{0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(
        renderer,
        nullptr,
        vertices.data(),
        static_cast<int>(vertices.size()),
        indices.data(),
        static_cast<int>(indices.size())
    );
}

void DrawCornerAoQuad(
    SDL_Renderer* renderer,
    const SDL_FRect& rect,
    const SDL_FColor& top_left,
    const SDL_FColor& top_right,
    const SDL_FColor& bottom_right,
    const SDL_FColor& bottom_left
) {
    DrawGradientQuad(
        renderer,
        rect,
        SDL_BLENDMODE_MUL,
        top_left,
        top_right,
        bottom_right,
        bottom_left
    );
}

} // namespace

void ApplyTerrainTileBrightness(
    SDL_Texture* texture,
    const State& state,
    const Graphics& graphics,
    int tile_x,
    int tile_y
) {
    (void)graphics;
    if (texture == nullptr) {
        return;
    }

    const Tile tile = GetTileForLighting(state, tile_x, tile_y);
    if (!IsForegroundSolidTile(tile)) {
        SDL_SetTextureColorModFloat(texture, 1.0F, 1.0F, 1.0F);
        return;
    }

    const float brightness = GetForegroundBrightnessForRender(state, tile_x, tile_y);
    SDL_SetTextureColorModFloat(
        texture,
        brightness,
        brightness,
        brightness
    );
}

void ApplyBackwallTileBrightness(
    SDL_Texture* texture,
    const State& state,
    const Graphics& graphics,
    int tile_x,
    int tile_y
) {
    (void)graphics;
    if (texture == nullptr) {
        return;
    }

    const float brightness = GetBackwallBrightnessForRender(state, tile_x, tile_y);
    SDL_SetTextureColorModFloat(
        texture,
        brightness,
        brightness,
        brightness
    );
}

void ResetTerrainTileBrightness(SDL_Texture* texture) {
    if (texture == nullptr) {
        return;
    }
    SDL_SetTextureColorModFloat(texture, 1.0F, 1.0F, 1.0F);
}

void RenderTerrainTileLighting(
    SDL_Renderer* renderer,
    const State& state,
    const Graphics& graphics,
    int tile_x,
    int tile_y,
    const SDL_FRect& dst
) {
    if (graphics.world_rotation_active) {
        return;
    }
    const PostProcessSettings& settings = state.settings.post_process;
    if (!settings.terrain_lighting) {
        return;
    }

    const Tile tile = GetTileForLighting(state, tile_x, tile_y);
    if (!IsForegroundSolidTile(tile)) {
        return;
    }

    const ForegroundTileTopology topology = GetForegroundTileTopologyForRender(state, tile_x, tile_y);

    if (settings.terrain_seam_ao) {
        const float ao_size_ratio = std::clamp(settings.terrain_seam_ao_size, 0.05F, 0.50F);
        const float ao_size = std::max(1.0F, std::round(std::min(dst.w, dst.h) * ao_size_ratio));
        const SDL_FColor ao_dark = MakeMultiplyShadeColor(settings.terrain_seam_ao_amount);
        const SDL_FColor ao_clear = MakeMultiplyShadeColor(0.0F);

        if (topology.ao_top_left) {
            DrawCornerAoQuad(
                renderer,
                SDL_FRect{dst.x, dst.y, ao_size, ao_size},
                ao_dark,
                ao_clear,
                ao_clear,
                ao_clear
            );
        }
        if (topology.ao_top_right) {
            DrawCornerAoQuad(
                renderer,
                SDL_FRect{dst.x + dst.w - ao_size, dst.y, ao_size, ao_size},
                ao_clear,
                ao_dark,
                ao_clear,
                ao_clear
            );
        }
        if (topology.ao_bottom_left) {
            DrawCornerAoQuad(
                renderer,
                SDL_FRect{dst.x, dst.y + dst.h - ao_size, ao_size, ao_size},
                ao_clear,
                ao_clear,
                ao_clear,
                ao_dark
            );
        }
        if (topology.ao_bottom_right) {
            DrawCornerAoQuad(
                renderer,
                SDL_FRect{dst.x + dst.w - ao_size, dst.y + dst.h - ao_size, ao_size, ao_size},
                ao_clear,
                ao_clear,
                ao_dark,
                ao_clear
            );
        }
    }
}

} // namespace splonks
