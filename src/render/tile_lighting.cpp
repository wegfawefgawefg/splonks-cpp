#include "render/tile_lighting.hpp"

#include "graphics.hpp"
#include "settings.hpp"
#include "state.hpp"
#include "render/terrain_lighting.hpp"
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

bool IsTerrainLightingTile(Tile tile) {
    return GetTileArchetype(tile).solid;
}

Tile GetTileForLighting(const State& state, int tile_x, int tile_y) {
    if (tile_x < 0 || tile_y < 0 || tile_x >= static_cast<int>(state.stage.GetTileWidth()) ||
        tile_y >= static_cast<int>(state.stage.GetTileHeight())) {
        return state.settings.post_process.terrain_face_enclosed_stage_bounds
                   ? state.stage.stage_border_tile
                   : Tile::Air;
    }
    return state.stage.tiles[static_cast<std::size_t>(tile_y)][static_cast<std::size_t>(tile_x)];
}

SDL_FColor MakeOverlayColor(std::uint8_t r, std::uint8_t g, std::uint8_t b, float alpha) {
    return SDL_FColor{
        static_cast<float>(r) / 255.0F,
        static_cast<float>(g) / 255.0F,
        static_cast<float>(b) / 255.0F,
        std::clamp(alpha, 0.0F, 1.0F),
    };
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
    if (!IsTerrainLightingTile(tile)) {
        SDL_SetTextureColorModFloat(texture, 1.0F, 1.0F, 1.0F);
        return;
    }

    const TerrainLightingTile lighting = GetTerrainLightingTileForRender(state, tile_x, tile_y);
    SDL_SetTextureColorModFloat(
        texture,
        lighting.brightness,
        lighting.brightness,
        lighting.brightness
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

    const BackwallLightingTile lighting = GetBackwallLightingTileForRender(state, tile_x, tile_y);
    SDL_SetTextureColorModFloat(
        texture,
        lighting.brightness,
        lighting.brightness,
        lighting.brightness
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
    (void)graphics;
    const PostProcessSettings& settings = state.settings.post_process;
    if (!settings.terrain_lighting) {
        return;
    }

    const Tile tile = GetTileForLighting(state, tile_x, tile_y);
    if (!IsTerrainLightingTile(tile)) {
        return;
    }

    const TerrainLightingTile lighting = GetTerrainLightingTileForRender(state, tile_x, tile_y);

    if (settings.terrain_face_shading) {
        const float band_ratio = std::clamp(settings.terrain_face_band_size, 0.05F, 0.50F);
        const float gradient_softness =
            std::clamp(settings.terrain_face_gradient_softness, 0.0F, 1.0F);
        const float corner_rounding =
            std::clamp(settings.terrain_face_corner_rounding, 0.0F, 1.0F);
        const float band_w = std::max(1.0F, std::round(dst.w * band_ratio));
        const float band_h = std::max(1.0F, std::round(dst.h * band_ratio));
        const float side_top_trim = std::round(band_h * corner_rounding);

        const float top_inner_alpha =
            settings.terrain_face_top_highlight * (1.0F - gradient_softness);
        const float side_inner_alpha =
            settings.terrain_face_side_shade * (1.0F - gradient_softness);
        const float bottom_inner_alpha =
            settings.terrain_face_bottom_shade * (1.0F - gradient_softness);

        if (lighting.open_top) {
            const SDL_FRect top_rect{dst.x, dst.y, dst.w, band_h};
            DrawGradientQuad(
                renderer,
                top_rect,
                SDL_BLENDMODE_ADD,
                MakeOverlayColor(255, 255, 255, settings.terrain_face_top_highlight),
                MakeOverlayColor(255, 255, 255, settings.terrain_face_top_highlight),
                MakeOverlayColor(255, 255, 255, top_inner_alpha),
                MakeOverlayColor(255, 255, 255, top_inner_alpha)
            );
        }
        if (lighting.open_bottom) {
            const SDL_FRect bottom_rect{dst.x, dst.y + dst.h - band_h, dst.w, band_h};
            DrawGradientQuad(
                renderer,
                bottom_rect,
                SDL_BLENDMODE_MUL,
                MakeMultiplyShadeColor(bottom_inner_alpha),
                MakeMultiplyShadeColor(bottom_inner_alpha),
                MakeMultiplyShadeColor(settings.terrain_face_bottom_shade),
                MakeMultiplyShadeColor(settings.terrain_face_bottom_shade)
            );
        }
        if (lighting.open_left) {
            const float top_trim = lighting.open_top ? side_top_trim : 0.0F;
            const float bottom_trim = lighting.open_bottom ? side_top_trim : 0.0F;
            const SDL_FRect left_rect{
                dst.x,
                dst.y + top_trim,
                band_w,
                std::max(0.0F, dst.h - top_trim - bottom_trim),
            };
            DrawGradientQuad(
                renderer,
                left_rect,
                SDL_BLENDMODE_MUL,
                MakeMultiplyShadeColor(settings.terrain_face_side_shade),
                MakeMultiplyShadeColor(side_inner_alpha),
                MakeMultiplyShadeColor(side_inner_alpha),
                MakeMultiplyShadeColor(settings.terrain_face_side_shade)
            );
        }
        if (lighting.open_right) {
            const float top_trim = lighting.open_top ? side_top_trim : 0.0F;
            const float bottom_trim = lighting.open_bottom ? side_top_trim : 0.0F;
            const SDL_FRect right_rect{
                dst.x + dst.w - band_w,
                dst.y + top_trim,
                band_w,
                std::max(0.0F, dst.h - top_trim - bottom_trim),
            };
            DrawGradientQuad(
                renderer,
                right_rect,
                SDL_BLENDMODE_MUL,
                MakeMultiplyShadeColor(side_inner_alpha),
                MakeMultiplyShadeColor(settings.terrain_face_side_shade),
                MakeMultiplyShadeColor(settings.terrain_face_side_shade),
                MakeMultiplyShadeColor(side_inner_alpha)
            );
        }
    }

    if (settings.terrain_seam_ao) {
        const float ao_size_ratio = std::clamp(settings.terrain_seam_ao_size, 0.05F, 0.50F);
        const float ao_size = std::max(1.0F, std::round(std::min(dst.w, dst.h) * ao_size_ratio));
        const SDL_FColor ao_dark = MakeMultiplyShadeColor(settings.terrain_seam_ao_amount);
        const SDL_FColor ao_clear = MakeMultiplyShadeColor(0.0F);

        if (lighting.ao_top_left) {
            DrawCornerAoQuad(
                renderer,
                SDL_FRect{dst.x, dst.y, ao_size, ao_size},
                ao_dark,
                ao_clear,
                ao_clear,
                ao_clear
            );
        }
        if (lighting.ao_top_right) {
            DrawCornerAoQuad(
                renderer,
                SDL_FRect{dst.x + dst.w - ao_size, dst.y, ao_size, ao_size},
                ao_clear,
                ao_dark,
                ao_clear,
                ao_clear
            );
        }
        if (lighting.ao_bottom_left) {
            DrawCornerAoQuad(
                renderer,
                SDL_FRect{dst.x, dst.y + dst.h - ao_size, ao_size, ao_size},
                ao_clear,
                ao_clear,
                ao_clear,
                ao_dark
            );
        }
        if (lighting.ao_bottom_right) {
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
