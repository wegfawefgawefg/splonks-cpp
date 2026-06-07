#include "render/debug_lighting.hpp"

#include "ent.hpp"
#include "ent/spec.hpp"
#include "graphics.hpp"
#include "stage_lighting.hpp"
#include "state.hpp"
#include "text.hpp"
#include "tile.hpp"
#include "sim/fxp.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>

namespace splonks {

namespace {

Vec2 WorldPointToScreen(
    const Graphics& graphics,
    const SDL_FRect& pres,
    const Vec2& world_pos
) {
    const float pres_scale = pres.w / static_cast<float>(graphics.dims.x);
    const Vec2 internal_screen = Vec2::New(
        std::round(((world_pos.x - graphics.camera.target.x) * graphics.camera.zoom) +
                   graphics.camera.offset.x),
        std::round(((world_pos.y - graphics.camera.target.y) * graphics.camera.zoom) +
                   graphics.camera.offset.y)
    );
    return Vec2::New(
        std::round(pres.x + internal_screen.x * pres_scale),
        std::round(pres.y + internal_screen.y * pres_scale)
    );
}

SDL_Color LightDebugColor(Color3 color, std::uint8_t alpha = 255) {
    const float max_channel = std::max({color.r, color.g, color.b, 1.0F});
    return SDL_Color{
        static_cast<std::uint8_t>(std::clamp((color.r / max_channel) * 255.0F, 0.0F, 255.0F)),
        static_cast<std::uint8_t>(std::clamp((color.g / max_channel) * 255.0F, 0.0F, 255.0F)),
        static_cast<std::uint8_t>(std::clamp((color.b / max_channel) * 255.0F, 0.0F, 255.0F)),
        alpha,
    };
}

void RenderWorldPointMarker(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const SDL_FRect& pres,
    const Vec2& world_pos,
    const SDL_Color& color
) {
    const Vec2 screen = WorldPointToScreen(graphics, pres, world_pos);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int offset = -1; offset <= 1; ++offset) {
        SDL_RenderLine(renderer, screen.x - 7.0F, screen.y + static_cast<float>(offset), screen.x + 7.0F, screen.y + static_cast<float>(offset));
        SDL_RenderLine(renderer, screen.x + static_cast<float>(offset), screen.y - 7.0F, screen.x + static_cast<float>(offset), screen.y + 7.0F);
    }
}

void RenderWorldCircleOutline(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const SDL_FRect& pres,
    const Vec2& center,
    float radius,
    const SDL_Color& color
) {
    if (radius <= 0.0F) {
        return;
    }

    constexpr int kSegments = 24;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    Vec2 previous = center + Vec2::New(radius, 0.0F);
    for (int i = 1; i <= kSegments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSegments);
        const float angle = t * 6.28318530718F;
        const Vec2 current = center + Vec2::New(std::cos(angle) * radius, std::sin(angle) * radius);
        const Vec2 previous_screen = WorldPointToScreen(graphics, pres, previous);
        const Vec2 current_screen = WorldPointToScreen(graphics, pres, current);
        SDL_RenderLine(renderer, previous_screen.x, previous_screen.y, current_screen.x, current_screen.y);
        previous = current;
    }
}

void RenderLightMarker(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const SDL_FRect& pres,
    const Vec2& world_pos,
    int radius_tiles,
    float strength,
    Color3 color,
    const char* label
) {
    const SDL_Color marker_color = LightDebugColor(color);
    RenderWorldCircleOutline(
        renderer,
        graphics,
        pres,
        world_pos,
        static_cast<float>(radius_tiles) * static_cast<float>(kTileSize),
        marker_color
    );
    RenderWorldPointMarker(renderer, graphics, pres, world_pos, marker_color);

    const Vec2 label_screen = WorldPointToScreen(graphics, pres, world_pos);
    char text[96];
    std::snprintf(text, sizeof(text), "%s r%d %.2f", label, radius_tiles, strength);
    DrawText(
        renderer,
        graphics,
        10,
        graphics.ui_font,
        text,
        label_screen.x + 6.0F,
        label_screen.y - 12.0F,
        marker_color
    );
}

} // namespace

void RenderLightOverlay(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& pres,
    const std::vector<Vec2>& render_offsets
) {
    if (!state.debug_overlay.show_lights) {
        return;
    }

    for (const Vec2& render_offset : render_offsets) {
        for (const StageLight& light : state.stage.lights) {
            if (light.radius <= 0) {
                continue;
            }
            const IVec2 center_tile = state.stage.WrapTileCoord(light.tile_pos);
            if (!state.stage.IsTileCoordInside(center_tile.x, center_tile.y)) {
                continue;
            }
            const Vec2 world_pos = Vec2::New(
                (static_cast<float>(center_tile.x) + 0.5F) * static_cast<float>(kTileSize),
                (static_cast<float>(center_tile.y) + 0.5F) * static_cast<float>(kTileSize)
            ) + render_offset;
            RenderLightMarker(
                renderer,
                graphics,
                pres,
                world_pos,
                light.radius,
                1.0F + (static_cast<float>(light.radius) * 0.03F),
                Color3::New(1.0F, 0.78F, 0.25F),
                "stage"
            );
        }

        for (const Ent& ent : state.ents.ents) {
            const float light_strength = sim::ToRenderScalar(ent.light_strength);
            if (!ent.active || !ent.render_enabled ||
                ent.condition == EntCondition::Dead ||
                light_strength <= 0.0F || ent.light_radius <= 0) {
                continue;
            }
            RenderLightMarker(
                renderer,
                graphics,
                pres,
                ent.GetCenter() + render_offset,
                ent.light_radius,
                light_strength,
                sim::ToRenderColor3(ent.light_color),
                GetEntTypeName(ent.type_)
            );
        }

        for (const TransientLight& light : state.stage_lighting.transient_lights) {
            if (light.frames_remaining == 0 || light.total_frames == 0 ||
                light.strength <= 0.0F || light.radius <= 0) {
                continue;
            }
            const float fade =
                static_cast<float>(light.frames_remaining) /
                static_cast<float>(light.total_frames);
            RenderLightMarker(
                renderer,
                graphics,
                pres,
                light.world_pos + render_offset,
                light.radius,
                light.strength * std::clamp(fade, 0.0F, 1.0F),
                light.color,
                "flash"
            );
        }
    }
}

} // namespace splonks
