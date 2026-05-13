#include "render/tile_lighting.hpp"

#include "graphics.hpp"
#include "settings.hpp"
#include "state.hpp"
#include "stage_lighting.hpp"
#include "tile.hpp"
#include "tile_spec.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace splonks {

namespace {

bool IsForegroundSolidTile(Tile tile) {
    return GetTileSpec(tile).solid;
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

SDL_FColor MakeLightColor(Color3 color) {
    return SDL_FColor{
        std::clamp(color.r, 0.0F, 2.0F),
        std::clamp(color.g, 0.0F, 2.0F),
        std::clamp(color.b, 0.0F, 2.0F),
        1.0F,
    };
}

Vec2 RotateTileLocalPoint(const Vec2& point, TileRotation rotation) {
    const float tile_size = static_cast<float>(kTileSize);
    const Vec2 center = Vec2::New(tile_size * 0.5F, tile_size * 0.5F);
    const Vec2 local = point - center;

    switch (rotation & kTileRotationMask) {
    case kTileRotation90:
        return center + Vec2::New(-local.y, local.x);
    case kTileRotation180:
        return center + Vec2::New(-local.x, -local.y);
    case kTileRotation270:
        return center + Vec2::New(local.y, -local.x);
    case kTileRotation0:
    default:
        return point;
    }
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

    const Color3 brightness = GetForegroundLightColorForRender(state, tile_x, tile_y);
    SDL_SetTextureColorModFloat(
        texture,
        std::clamp(brightness.r, 0.0F, 2.0F),
        std::clamp(brightness.g, 0.0F, 2.0F),
        std::clamp(brightness.b, 0.0F, 2.0F)
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

    const Color3 brightness = GetBackwallLightColorForRender(state, tile_x, tile_y);
    SDL_SetTextureColorModFloat(
        texture,
        std::clamp(brightness.r, 0.0F, 2.0F),
        std::clamp(brightness.g, 0.0F, 2.0F),
        std::clamp(brightness.b, 0.0F, 2.0F)
    );
}

void ResetTerrainTileBrightness(SDL_Texture* texture) {
    if (texture == nullptr) {
        return;
    }
    SDL_SetTextureColorModFloat(texture, 1.0F, 1.0F, 1.0F);
}

bool RenderTileWithVertexLighting(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const State& state,
    const Graphics& graphics,
    const SDL_FRect& src,
    const SDL_FRect& dst,
    const Vec2& world_pos,
    TileRotation tile_rotation,
    Color3 (*sample_color)(const State&, const Vec2&)
) {
    if (renderer == nullptr || texture == nullptr || graphics.world_rotation_active ||
        src.w <= 0.0F || src.h <= 0.0F || dst.w <= 0.0F || dst.h <= 0.0F) {
        return false;
    }
    if (!state.settings.post_process.terrain_lighting ||
        !state.settings.post_process.terrain_exposure_lighting) {
        return false;
    }

    float texture_width = 0.0F;
    float texture_height = 0.0F;
    if (!SDL_GetTextureSize(texture, &texture_width, &texture_height) ||
        texture_width <= 0.0F || texture_height <= 0.0F) {
        return false;
    }

    const float u0 = src.x / texture_width;
    const float v0 = src.y / texture_height;
    const float u1 = (src.x + src.w) / texture_width;
    const float v1 = (src.y + src.h) / texture_height;
    const float tile_size = static_cast<float>(kTileSize);

    const auto brightness_color = [&state, sample_color](const Vec2& sample_pos) {
        return MakeLightColor(sample_color(state, sample_pos));
    };

    const std::array<SDL_Vertex, 4> vertices{
        SDL_Vertex{
            SDL_FPoint{dst.x, dst.y},
            brightness_color(world_pos + RotateTileLocalPoint(Vec2::New(0.0F, 0.0F), tile_rotation)),
            SDL_FPoint{u0, v0},
        },
        SDL_Vertex{
            SDL_FPoint{dst.x + dst.w, dst.y},
            brightness_color(
                world_pos + RotateTileLocalPoint(Vec2::New(tile_size, 0.0F), tile_rotation)
            ),
            SDL_FPoint{u1, v0},
        },
        SDL_Vertex{
            SDL_FPoint{dst.x + dst.w, dst.y + dst.h},
            brightness_color(
                world_pos + RotateTileLocalPoint(Vec2::New(tile_size, tile_size), tile_rotation)
            ),
            SDL_FPoint{u1, v1},
        },
        SDL_Vertex{
            SDL_FPoint{dst.x, dst.y + dst.h},
            brightness_color(
                world_pos + RotateTileLocalPoint(Vec2::New(0.0F, tile_size), tile_rotation)
            ),
            SDL_FPoint{u0, v1},
        },
    };
    constexpr std::array<int, 6> indices{0, 1, 2, 0, 2, 3};
    SDL_SetTextureColorModFloat(texture, 1.0F, 1.0F, 1.0F);
    SDL_RenderGeometry(
        renderer,
        texture,
        vertices.data(),
        static_cast<int>(vertices.size()),
        indices.data(),
        static_cast<int>(indices.size())
    );
    return true;
}

bool RenderTerrainTileWithVertexLighting(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const State& state,
    const Graphics& graphics,
    const SDL_FRect& src,
    const SDL_FRect& dst,
    const Vec2& world_pos,
    TileRotation tile_rotation
) {
    return RenderTileWithVertexLighting(
        renderer,
        texture,
        state,
        graphics,
        src,
        dst,
        world_pos,
        tile_rotation,
        SampleForegroundLightColorForRender
    );
}

bool RenderBackwallTileWithVertexLighting(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const State& state,
    const Graphics& graphics,
    const SDL_FRect& src,
    const SDL_FRect& dst,
    const Vec2& world_pos
) {
    return RenderTileWithVertexLighting(
        renderer,
        texture,
        state,
        graphics,
        src,
        dst,
        world_pos,
        kTileRotation0,
        SampleBackwallLightColorForRender
    );
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
