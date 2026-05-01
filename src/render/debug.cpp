#include "render/debug.hpp"

#include "audio.hpp"
#include "audio_acoustics.hpp"
#include "audio_emitters.hpp"
#include "entity/archetype.hpp"
#include "entity.hpp"
#include "entities/common/common.hpp"
#include "graphics.hpp"
#include "stage_acoustics.hpp"
#include "state.hpp"
#include "text.hpp"
#include "tile.hpp"
#include "tile_archetype.hpp"
#include "world_query.hpp"
#include "entities/shop.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <vector>

namespace splonks {

namespace {

struct VisibleWorldRect {
    Vec2 tl;
    Vec2 br;
};

SDL_FRect GetDebugPresentationRect(SDL_Renderer* renderer, const Graphics& graphics) {
    int output_width = static_cast<int>(graphics.window_dims.x);
    int output_height = static_cast<int>(graphics.window_dims.y);
    if (graphics.fullscreen) {
        SDL_GetCurrentRenderOutputSize(renderer, &output_width, &output_height);
    }
    return GetPresentationRect(graphics, output_width, output_height);
}

VisibleWorldRect GetVisibleWorldRect(const Graphics& graphics) {
    return VisibleWorldRect{
        .tl = graphics.camera.target - (graphics.camera.offset / graphics.camera.zoom),
        .br = graphics.camera.target +
              ((ToVec2(graphics.dims) - graphics.camera.offset) / graphics.camera.zoom),
    };
}

int FloorDivByFloat(float value, float divisor) {
    if (divisor <= 0.0F) {
        return 0;
    }
    return static_cast<int>(std::floor(value / divisor));
}

std::vector<Vec2> GetVisibleWrappedRenderOffsets(const Stage& stage, const Graphics& graphics) {
    std::vector<Vec2> offsets;
    offsets.push_back(Vec2::New(0.0F, 0.0F));

    const float stage_width = static_cast<float>(stage.GetWidth());
    const float stage_height = static_cast<float>(stage.GetHeight());
    if ((!stage.WrapsX() || stage_width <= 0.0F) && (!stage.WrapsY() || stage_height <= 0.0F)) {
        return offsets;
    }

    const VisibleWorldRect visible = GetVisibleWorldRect(graphics);
    const int min_copy_x = stage.WrapsX() ? FloorDivByFloat(visible.tl.x, stage_width) : 0;
    const int max_copy_x = stage.WrapsX() ? FloorDivByFloat(visible.br.x, stage_width) : 0;
    const int min_copy_y = stage.WrapsY() ? FloorDivByFloat(visible.tl.y, stage_height) : 0;
    const int max_copy_y = stage.WrapsY() ? FloorDivByFloat(visible.br.y, stage_height) : 0;

    offsets.clear();
    for (int copy_y = min_copy_y; copy_y <= max_copy_y; ++copy_y) {
        for (int copy_x = min_copy_x; copy_x <= max_copy_x; ++copy_x) {
            offsets.push_back(Vec2::New(
                static_cast<float>(copy_x) * stage_width,
                static_cast<float>(copy_y) * stage_height
            ));
        }
    }
    return offsets;
}

SDL_FRect WorldRectToScreen(
    const Graphics& graphics,
    const SDL_FRect& presentation,
    const Vec2& world_pos,
    const Vec2& world_size
) {
    const float presentation_scale = presentation.w / static_cast<float>(graphics.dims.x);
    const Vec2 internal_screen = Vec2::New(
        std::round(((world_pos.x - graphics.camera.target.x) * graphics.camera.zoom) +
                   graphics.camera.offset.x),
        std::round(((world_pos.y - graphics.camera.target.y) * graphics.camera.zoom) +
                   graphics.camera.offset.y)
    );
    const Vec2 internal_size = Vec2::New(
        std::round(world_size.x * graphics.camera.zoom),
        std::round(world_size.y * graphics.camera.zoom)
    );
    const Vec2 screen = Vec2::New(
        std::round(presentation.x + internal_screen.x * presentation_scale),
        std::round(presentation.y + internal_screen.y * presentation_scale)
    );
    return SDL_FRect{
        screen.x,
        screen.y,
        std::round(internal_size.x * presentation_scale),
        std::round(internal_size.y * presentation_scale),
    };
}

Vec2 WorldPointToScreen(
    const Graphics& graphics,
    const SDL_FRect& presentation,
    const Vec2& world_pos
) {
    const float presentation_scale = presentation.w / static_cast<float>(graphics.dims.x);
    const Vec2 internal_screen = Vec2::New(
        std::round(((world_pos.x - graphics.camera.target.x) * graphics.camera.zoom) +
                   graphics.camera.offset.x),
        std::round(((world_pos.y - graphics.camera.target.y) * graphics.camera.zoom) +
                   graphics.camera.offset.y)
    );
    return Vec2::New(
        std::round(presentation.x + internal_screen.x * presentation_scale),
        std::round(presentation.y + internal_screen.y * presentation_scale)
    );
}

bool IsScreenYVisible(const SDL_FRect& presentation, float y) {
    return y >= presentation.y - 16.0F && y <= presentation.y + presentation.h + 16.0F;
}

bool IsScreenRectVisible(const SDL_FRect& presentation, const SDL_FRect& rect) {
    return rect.x + rect.w >= presentation.x &&
           rect.x <= presentation.x + presentation.w &&
           rect.y + rect.h >= presentation.y &&
           rect.y <= presentation.y + presentation.h;
}

SDL_Color ToSdlColor(const DebugAnnotationColor& color) {
    return SDL_Color{color.r, color.g, color.b, color.a};
}

bool ShouldRenderShakeBrushPreview(const State& state) {
    const DebugShakeBrushState& brush = state.debug_shake_brush;
    return brush.enabled &&
           ((brush.affect_foreground_tiles && brush.foreground_tile_amount > 0.0F) ||
            (brush.affect_background_tiles && brush.background_tile_amount > 0.0F) ||
            (brush.affect_entities && brush.entity_amount > 0.0F));
}

bool ShouldRenderAudioBrushPreview(const State& state) {
    return state.debug_audio_brush.enabled;
}

bool ShouldRenderFluidBrushPreview(const State& state) {
    return state.debug_fluid_brush.enabled;
}

void RenderWorldCircleOutline(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const SDL_FRect& presentation,
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
        const Vec2 previous_screen = WorldPointToScreen(graphics, presentation, previous);
        const Vec2 current_screen = WorldPointToScreen(graphics, presentation, current);
        SDL_RenderLine(renderer, previous_screen.x, previous_screen.y, current_screen.x, current_screen.y);
        previous = current;
    }
}

void RenderWorldPointMarker(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const SDL_FRect& presentation,
    const Vec2& world_pos,
    const SDL_Color& color
) {
    const Vec2 screen = WorldPointToScreen(graphics, presentation, world_pos);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int offset = -1; offset <= 1; ++offset) {
        SDL_RenderLine(renderer, screen.x - 7.0F, screen.y + static_cast<float>(offset), screen.x + 7.0F, screen.y + static_cast<float>(offset));
        SDL_RenderLine(renderer, screen.x + static_cast<float>(offset), screen.y - 7.0F, screen.x + static_cast<float>(offset), screen.y + 7.0F);
    }
}

void RenderWorldVerticalGuide(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const SDL_FRect& presentation,
    float world_x,
    const SDL_Color& color,
    int thickness = 3
) {
    const Vec2 screen = WorldPointToScreen(
        graphics,
        presentation,
        Vec2::New(world_x, graphics.camera.target.y)
    );
    if (screen.x < presentation.x - 2.0F ||
        screen.x > presentation.x + presentation.w + 2.0F) {
        return;
    }

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    const int half_thickness = std::max(0, thickness / 2);
    for (int offset = -half_thickness; offset <= half_thickness; ++offset) {
        SDL_RenderLine(
            renderer,
            screen.x + static_cast<float>(offset),
            presentation.y,
            screen.x + static_cast<float>(offset),
            presentation.y + presentation.h
        );
    }
}

void RenderScreenLine(
    SDL_Renderer* renderer,
    const Vec2& from,
    const Vec2& to,
    const SDL_Color& color,
    int thickness = 1
) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    if (thickness <= 1) {
        SDL_RenderLine(renderer, from.x, from.y, to.x, to.y);
        return;
    }

    const Vec2 delta = to - from;
    const float length = std::sqrt((delta.x * delta.x) + (delta.y * delta.y));
    Vec2 perp = Vec2::New(0.0F, 1.0F);
    if (length > 0.0F) {
        perp = Vec2::New(-delta.y / length, delta.x / length);
    }

    const int half_thickness = std::max(0, thickness / 2);
    for (int offset = -half_thickness; offset <= half_thickness; ++offset) {
        const Vec2 screen_offset = perp * static_cast<float>(offset);
        SDL_RenderLine(
            renderer,
            from.x + screen_offset.x,
            from.y + screen_offset.y,
            to.x + screen_offset.x,
            to.y + screen_offset.y
        );
    }
}

Vec2 TileTopLeftWorld(const IVec2& tile_pos) {
    return Vec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize)),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize))
    );
}

Vec2 TileCenterWorld(const IVec2& tile_pos) {
    return TileTopLeftWorld(tile_pos) + Vec2::New(
        static_cast<float>(kTileSize) * 0.5F,
        static_cast<float>(kTileSize) * 0.5F
    );
}

Vec2 TileCenterWorldRelativeToOrigin(
    const IVec2& origin_tile,
    const Vec2& origin_world,
    const IVec2& ray_tile
) {
    return origin_world + Vec2::New(
        static_cast<float>((ray_tile.x - origin_tile.x) * static_cast<int>(kTileSize)),
        static_cast<float>((ray_tile.y - origin_tile.y) * static_cast<int>(kTileSize))
    );
}

Vec2 TileTopLeftWorldRelativeToOrigin(
    const IVec2& origin_tile,
    const Vec2& origin_world,
    const IVec2& ray_tile
) {
    return TileCenterWorldRelativeToOrigin(origin_tile, origin_world, ray_tile) -
           Vec2::New(
               static_cast<float>(kTileSize) * 0.5F,
               static_cast<float>(kTileSize) * 0.5F
           );
}

SDL_Color TileOpennessColor(float openness, std::uint8_t alpha) {
    const float clamped = std::clamp(openness, 0.0F, 1.0F);
    return SDL_Color{
        static_cast<std::uint8_t>(32 + clamped * 223.0F),
        static_cast<std::uint8_t>(32 + (1.0F - clamped) * 223.0F),
        48,
        alpha,
    };
}

void RenderAudioBrushPreview(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& presentation
) {
    if (!ShouldRenderAudioBrushPreview(state)) {
        return;
    }

    const Vec2 mouse_world = graphics.ScreenToWc(state.immediate_playing_inputs.mouse_pos);
    RenderWorldPointMarker(
        renderer,
        graphics,
        presentation,
        mouse_world,
        SDL_Color{0, 255, 255, 255}
    );

    const DebugAudioBrushState& brush = state.debug_audio_brush;
    if (brush.show_openness_rays) {
        const IVec2 mouse_tile = graphics.ScreenToTileCoords(state.immediate_playing_inputs.mouse_pos);
        const IVec2 wrapped_mouse_tile = state.stage.WrapTileCoord(mouse_tile);
        if (state.stage.IsTileCoordInside(wrapped_mouse_tile.x, wrapped_mouse_tile.y)) {
            const std::array<IVec2, 8> kDirections{{
                IVec2::New(1, 0),
                IVec2::New(1, 1),
                IVec2::New(0, 1),
                IVec2::New(-1, 1),
                IVec2::New(-1, 0),
                IVec2::New(-1, -1),
                IVec2::New(0, -1),
                IVec2::New(1, -1),
            }};
            const Vec2 ray_origin_world = GetNearestWorldPoint(
                state.stage,
                mouse_world,
                TileCenterWorld(wrapped_mouse_tile)
            );
            const std::vector<Vec2> visible_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
            for (const IVec2& direction : kDirections) {
                const TileStepRaycastResult ray = RaycastTileSteps(
                    state.stage,
                    wrapped_mouse_tile,
                    direction,
                    kStageOpennessRayLengthTiles
                );
                const SDL_Color ray_color = ray.blocked
                    ? SDL_Color{255, 64, 64, 255}
                    : SDL_Color{64, 255, 64, 255};
                const int segment_count = ray.open_steps + (ray.blocked ? 1 : 0);
                for (int step = 1; step <= segment_count; ++step) {
                    const IVec2 previous_tile = IVec2::New(
                        wrapped_mouse_tile.x + direction.x * (step - 1),
                        wrapped_mouse_tile.y + direction.y * (step - 1)
                    );
                    const IVec2 current_tile = IVec2::New(
                        wrapped_mouse_tile.x + direction.x * step,
                        wrapped_mouse_tile.y + direction.y * step
                    );
                    const Vec2 previous_world = TileCenterWorldRelativeToOrigin(
                        wrapped_mouse_tile,
                        ray_origin_world,
                        previous_tile
                    );
                    const Vec2 current_world = TileCenterWorldRelativeToOrigin(
                        wrapped_mouse_tile,
                        ray_origin_world,
                        current_tile
                    );
                    for (const Vec2& offset : visible_offsets) {
                        const Vec2 from_screen = WorldPointToScreen(
                            graphics,
                            presentation,
                            previous_world + offset
                        );
                        const Vec2 to_screen = WorldPointToScreen(
                            graphics,
                            presentation,
                            current_world + offset
                        );
                        RenderScreenLine(
                            renderer,
                            from_screen,
                            to_screen,
                            ray_color,
                            2
                        );
                    }
                }
                if (ray.blocked) {
                    const Vec2 blocker_world = TileTopLeftWorldRelativeToOrigin(
                        wrapped_mouse_tile,
                        ray_origin_world,
                        ray.blocker_unwrapped_tile
                    );
                    for (const Vec2& offset : visible_offsets) {
                        const SDL_FRect blocker_rect = WorldRectToScreen(
                            graphics,
                            presentation,
                            blocker_world + offset,
                            Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                        );
                        if (IsScreenRectVisible(presentation, blocker_rect)) {
                            SDL_RenderRect(renderer, &blocker_rect);
                        }
                    }
                }
            }
        }
    }

    const Vec2 listener_world = GetAudioListenerWorldPos(state);
    if (brush.show_occlusion_ray && brush.source_active) {
        const Vec2 source_world =
            GetNearestWorldPoint(state.stage, listener_world, brush.source_world_pos);
        const Vec2 ray_delta = GetNearestWorldDelta(state.stage, source_world, listener_world);
        const float ray_length = std::sqrt((ray_delta.x * ray_delta.x) + (ray_delta.y * ray_delta.y));
        const WorldRayHit hit = RaycastTiles(
            source_world,
            ray_delta,
            static_cast<int>(std::ceil(ray_length)) + 1,
            state
        );
        const bool occluded = ShouldAudioRayHitCountAsOccluded(state, listener_world, hit);
        const SDL_Color occlusion_color = occluded
            ? SDL_Color{255, 32, 32, 255}
            : SDL_Color{32, 255, 32, 255};
        const float listener_epsilon_px =
            std::max(0.0F, state.settings.audio.acoustics_occlusion_listener_epsilon_px);
        if (listener_epsilon_px > 0.0F) {
            RenderWorldCircleOutline(
                renderer,
                graphics,
                presentation,
                listener_world,
                listener_epsilon_px,
                SDL_Color{255, 220, 32, 255}
            );
        }
        const Vec2 source_screen = WorldPointToScreen(graphics, presentation, source_world);
        const Vec2 listener_screen = WorldPointToScreen(graphics, presentation, listener_world);
        RenderScreenLine(
            renderer,
            source_screen,
            listener_screen,
            occlusion_color,
            5
        );
    }

    if (!brush.source_active) {
        return;
    }

    const Vec2 source_world =
        GetNearestWorldPoint(state.stage, listener_world, brush.source_world_pos);
    const float pan_half_width = std::max(state.settings.audio.pan_half_width_px, 1.0F);
    RenderWorldVerticalGuide(
        renderer,
        graphics,
        presentation,
        listener_world.x,
        SDL_Color{0, 255, 255, 255}
    );
    RenderWorldVerticalGuide(
        renderer,
        graphics,
        presentation,
        listener_world.x - pan_half_width,
        SDL_Color{0, 160, 255, 255}
    );
    RenderWorldVerticalGuide(
        renderer,
        graphics,
        presentation,
        listener_world.x + pan_half_width,
        SDL_Color{0, 160, 255, 255}
    );

    const Vec2 listener_screen = WorldPointToScreen(graphics, presentation, listener_world);
    const Vec2 source_screen = WorldPointToScreen(graphics, presentation, source_world);
    RenderScreenLine(
        renderer,
        listener_screen,
        source_screen,
        SDL_Color{255, 192, 64, 255},
        2
    );
    RenderWorldPointMarker(
        renderer,
        graphics,
        presentation,
        listener_world,
        SDL_Color{0, 255, 255, 255}
    );
    RenderWorldPointMarker(
        renderer,
        graphics,
        presentation,
        source_world,
        SDL_Color{255, 192, 64, 255}
    );
}

void RenderShakeBrushPreview(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& presentation
) {
    if (!ShouldRenderShakeBrushPreview(state)) {
        return;
    }

    const DebugShakeBrushState& brush = state.debug_shake_brush;
    const UVec2 mouse_pos = state.immediate_playing_inputs.mouse_pos;
    const Vec2 mouse_world = graphics.ScreenToWc(mouse_pos);
    const IVec2 mouse_tile = graphics.ScreenToTileCoords(mouse_pos);
    const float radius_tiles = std::max(0.0F, brush.radius_tiles);

    const bool affects_fg = brush.affect_foreground_tiles && brush.foreground_tile_amount > 0.0F;
    const bool affects_bg = brush.affect_background_tiles && brush.background_tile_amount > 0.0F;
    if (affects_fg || affects_bg) {
        const int radius_ceiled = static_cast<int>(std::ceil(radius_tiles));
        for (int y = mouse_tile.y - radius_ceiled; y <= mouse_tile.y + radius_ceiled; ++y) {
            for (int x = mouse_tile.x - radius_ceiled; x <= mouse_tile.x + radius_ceiled; ++x) {
                const float dx = static_cast<float>(x - mouse_tile.x);
                const float dy = static_cast<float>(y - mouse_tile.y);
                const float distance = std::sqrt((dx * dx) + (dy * dy));
                if (distance > radius_tiles) {
                    continue;
                }

                const IVec2 wrapped = state.stage.WrapTileCoord(IVec2::New(x, y));
                if (!state.stage.IsTileCoordInside(wrapped.x, wrapped.y)) {
                    continue;
                }

                const Vec2 tile_world = Vec2::New(
                    static_cast<float>(wrapped.x * static_cast<int>(kTileSize)),
                    static_cast<float>(wrapped.y * static_cast<int>(kTileSize))
                );
                const Vec2 nearest_tile_world = GetNearestWorldPoint(state.stage, mouse_world, tile_world);
                const SDL_FRect tile_rect = WorldRectToScreen(
                    graphics,
                    presentation,
                    nearest_tile_world,
                    Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                );
                if (!IsScreenRectVisible(presentation, tile_rect)) {
                    continue;
                }

                if (affects_fg && affects_bg) {
                    SDL_SetRenderDrawColor(renderer, 64, 224, 255, 255);
                } else if (affects_fg) {
                    SDL_SetRenderDrawColor(renderer, 255, 220, 96, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, 96, 160, 255, 255);
                }
                SDL_RenderRect(renderer, &tile_rect);
            }
        }
    }

    if (brush.affect_entities && brush.entity_amount > 0.0F) {
        RenderWorldCircleOutline(
            renderer,
            graphics,
            presentation,
            mouse_world,
            radius_tiles * static_cast<float>(kTileSize),
            SDL_Color{255, 180, 64, 255}
        );
    }
}

void RenderFluidBrushPreview(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& presentation
) {
    if (!ShouldRenderFluidBrushPreview(state)) {
        return;
    }

    const DebugFluidBrushState& brush = state.debug_fluid_brush;
    const UVec2 mouse_pos = state.immediate_playing_inputs.mouse_pos;
    const Vec2 mouse_world = graphics.ScreenToWc(mouse_pos);
    const IVec2 mouse_tile = graphics.ScreenToTileCoords(mouse_pos);
    const int radius_tiles = std::max(0, brush.radius_tiles);

    for (int y = mouse_tile.y - radius_tiles; y <= mouse_tile.y + radius_tiles; ++y) {
        for (int x = mouse_tile.x - radius_tiles; x <= mouse_tile.x + radius_tiles; ++x) {
            const float dx = static_cast<float>(x - mouse_tile.x);
            const float dy = static_cast<float>(y - mouse_tile.y);
            const float distance = std::sqrt((dx * dx) + (dy * dy));
            if (distance > static_cast<float>(radius_tiles)) {
                continue;
            }

            const IVec2 wrapped = state.stage.WrapTileCoord(IVec2::New(x, y));
            if (!state.stage.IsTileCoordInside(wrapped.x, wrapped.y)) {
                continue;
            }

            const Vec2 tile_world = Vec2::New(
                static_cast<float>(wrapped.x * static_cast<int>(kTileSize)),
                static_cast<float>(wrapped.y * static_cast<int>(kTileSize))
            );
            const Vec2 nearest_tile_world = GetNearestWorldPoint(state.stage, mouse_world, tile_world);
            const SDL_FRect tile_rect = WorldRectToScreen(
                graphics,
                presentation,
                nearest_tile_world,
                Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
            );
            if (!IsScreenRectVisible(presentation, tile_rect)) {
                continue;
            }

            SDL_SetRenderDrawColor(renderer, 32, 220, 255, 255);
            SDL_RenderRect(renderer, &tile_rect);
        }
    }
}

void RenderEntityCollisionBoxes(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& presentation,
    const std::vector<Vec2>& render_offsets
) {
    for (const Vec2& render_offset : render_offsets) {
        for (const Entity& entity : state.entity_manager.entities) {
            if (!entity.active) {
                continue;
            }

            const AABB pbox_aabb = entity.GetAABB();
            const Vec2 pbox_size = pbox_aabb.br - pbox_aabb.tl + Vec2::New(1.0F, 1.0F);
            const SDL_FRect pbox_rect = WorldRectToScreen(
                graphics,
                presentation,
                pbox_aabb.tl + render_offset,
                pbox_size
            );
            const AABB cbox_aabb = entities::common::GetContactAabbForEntity(entity, graphics);
            const Vec2 cbox_size = cbox_aabb.br - cbox_aabb.tl + Vec2::New(1.0F, 1.0F);
            const SDL_FRect cbox_rect = WorldRectToScreen(
                graphics,
                presentation,
                cbox_aabb.tl + render_offset,
                cbox_size
            );
            if (!IsScreenRectVisible(presentation, pbox_rect) &&
                !IsScreenRectVisible(presentation, cbox_rect)) {
                continue;
            }

            SDL_Color pbox_color = SDL_Color{255, 255, 0, 255};
            SDL_Color cbox_color = SDL_Color{64, 224, 255, 255};
            if (IsPlayerLikeEntityType(entity.type_)) {
                pbox_color = SDL_Color{64, 255, 64, 255};
                cbox_color = SDL_Color{64, 160, 255, 255};
            } else if (!entity.can_collide) {
                pbox_color = SDL_Color{255, 180, 64, 255};
                cbox_color = SDL_Color{255, 96, 224, 255};
            }

            SDL_SetRenderDrawColor(
                renderer,
                pbox_color.r,
                pbox_color.g,
                pbox_color.b,
                pbox_color.a
            );
            SDL_RenderRect(renderer, &pbox_rect);
            SDL_SetRenderDrawColor(
                renderer,
                cbox_color.r,
                cbox_color.g,
                cbox_color.b,
                cbox_color.a
            );
            SDL_RenderRect(renderer, &cbox_rect);
        }
    }
}

void RenderVoidDeathLine(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& presentation,
    const std::vector<Vec2>& render_offsets
) {
    if (!state.stage.HasVoidDeathY()) {
        return;
    }

    const float void_death_y = state.stage.GetVoidDeathY();
    std::vector<float> rendered_offset_y_values;
    for (const Vec2& render_offset : render_offsets) {
        bool already_rendered = false;
        for (const float rendered_offset_y : rendered_offset_y_values) {
            if (rendered_offset_y == render_offset.y) {
                already_rendered = true;
                break;
            }
        }
        if (already_rendered) {
            continue;
        }
        rendered_offset_y_values.push_back(render_offset.y);

        const Vec2 screen = WorldPointToScreen(
            graphics,
            presentation,
            Vec2::New(graphics.camera.target.x + render_offset.x, void_death_y + render_offset.y)
        );
        if (!IsScreenYVisible(presentation, screen.y)) {
            continue;
        }

        SDL_SetRenderDrawColor(renderer, 255, 96, 96, 255);
        SDL_RenderLine(
            renderer,
            presentation.x,
            screen.y,
            presentation.x + presentation.w,
            screen.y
        );

        char text[64];
        std::snprintf(text, sizeof(text), "void y=%d", static_cast<int>(void_death_y));
        DrawText(
            renderer,
            graphics,
            10,
            graphics.ui_font,
            text,
            presentation.x + 6.0F,
            screen.y - 12.0F,
            SDL_Color{255, 96, 96, 255}
        );
    }
}

void RenderEntityLabels(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& presentation,
    const std::vector<Vec2>& render_offsets
) {
    if (!state.debug_overlay.show_entity_ids && !state.debug_overlay.show_entity_types) {
        return;
    }

    for (const Vec2& render_offset : render_offsets) {
        for (const Entity& entity : state.entity_manager.entities) {
            if (!entity.active) {
                continue;
            }

            const AABB pbox_aabb = entity.GetAABB();
            const Vec2 pbox_size = pbox_aabb.br - pbox_aabb.tl + Vec2::New(1.0F, 1.0F);
            const SDL_FRect pbox_rect = WorldRectToScreen(
                graphics,
                presentation,
                pbox_aabb.tl + render_offset,
                pbox_size
            );
            if (!IsScreenRectVisible(presentation, pbox_rect)) {
                continue;
            }

            float text_y = pbox_rect.y + (pbox_rect.h * 0.5F) - 5.0F;
            if (state.debug_overlay.show_entity_ids) {
                char label[32];
                std::snprintf(label, sizeof(label), "%zu", entity.vid.id);
                DrawText(
                    renderer,
                    graphics,
                    10,
                    graphics.ui_font,
                    label,
                    pbox_rect.x + (pbox_rect.w * 0.5F) - 4.0F,
                    text_y,
                    SDL_Color{255, 255, 255, 255}
                );
                text_y += 10.0F;
            }
            if (state.debug_overlay.show_entity_types) {
                DrawText(
                    renderer,
                    graphics,
                    10,
                    graphics.ui_font,
                    GetEntityTypeName(entity.type_),
                    pbox_rect.x + 1.0F,
                    text_y,
                    SDL_Color{255, 210, 96, 255}
                );
            }
        }
    }
}

void RenderEntityRenderCenters(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& presentation,
    const std::vector<Vec2>& render_offsets
) {
    if (!state.debug_overlay.show_entity_render_centers) {
        return;
    }

    const SDL_Color color{255, 255, 64, 255};
    for (const Vec2& render_offset : render_offsets) {
        for (const Entity& entity : state.entity_manager.entities) {
            if (!entity.active || !entity.render_enabled) {
                continue;
            }
            RenderWorldPointMarker(
                renderer,
                graphics,
                presentation,
                entity.GetCenter() + render_offset,
                color
            );
        }
    }
}

void RenderDebugAnnotations(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& presentation,
    const std::vector<Vec2>& render_offsets
) {
    if (!state.debug_overlay.show_debug_annotations) {
        return;
    }

    for (const Vec2& render_offset : render_offsets) {
        for (const DebugRectAnnotation& annotation : state.debug_rect_annotations) {
            const Vec2 world_size = annotation.area.br - annotation.area.tl + Vec2::New(1.0F, 1.0F);
            const SDL_FRect rect = WorldRectToScreen(
                graphics,
                presentation,
                annotation.area.tl + render_offset,
                world_size
            );
            if (!IsScreenRectVisible(presentation, rect)) {
                continue;
            }

            const SDL_Color color = ToSdlColor(annotation.color);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
            const float left = std::floor(rect.x);
            const float top = std::floor(rect.y);
            const float right = std::ceil(rect.x + rect.w);
            const float bottom = std::ceil(rect.y + rect.h);
            SDL_RenderLine(renderer, left, top, right, top);
            SDL_RenderLine(renderer, right, top, right, bottom);
            SDL_RenderLine(renderer, right, bottom, left, bottom);
            SDL_RenderLine(renderer, left, bottom, left, top);
        }

        for (const DebugLabelAnnotation& annotation : state.debug_label_annotations) {
            const Vec2 world_pos = annotation.world_pos + render_offset;
            const Vec2 screen = WorldPointToScreen(graphics, presentation, world_pos);
            if (screen.x < presentation.x - 32.0F || screen.x > presentation.x + presentation.w + 32.0F ||
                screen.y < presentation.y - 32.0F || screen.y > presentation.y + presentation.h + 32.0F) {
                continue;
            }

            const SDL_Color color = ToSdlColor(annotation.color);
            RenderWorldPointMarker(renderer, graphics, presentation, world_pos, color);
            DrawText(
                renderer,
                graphics,
                10,
                graphics.ui_font,
                annotation.text.c_str(),
                screen.x + 6.0F,
                screen.y - 12.0F,
                color
            );
        }

        if (state.debug_overlay.show_stagegen_annotations) {
            const SDL_Color stagegen_color{96, 255, 160, 255};
            for (const StageGenAnnotation& annotation : state.stage.stagegen_annotations) {
                const Vec2 world_pos = annotation.world_pos + render_offset;
                const Vec2 screen = WorldPointToScreen(graphics, presentation, world_pos);
                if (screen.x < presentation.x - 64.0F || screen.x > presentation.x + presentation.w + 64.0F ||
                    screen.y < presentation.y - 64.0F || screen.y > presentation.y + presentation.h + 64.0F) {
                    continue;
                }

                RenderWorldPointMarker(renderer, graphics, presentation, world_pos, stagegen_color);
                DrawText(
                    renderer,
                    graphics,
                    10,
                    graphics.ui_font,
                    annotation.text.c_str(),
                    screen.x + 6.0F,
                    screen.y - 12.0F,
                    stagegen_color
                );
            }
        }
    }
}

void RenderChunkOverlay(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& presentation,
    const std::vector<Vec2>& render_offsets
) {
    if (!state.debug_overlay.show_chunk_boundaries && !state.debug_overlay.show_chunk_coords) {
        return;
    }
    if (state.stage.rooms.empty()) {
        return;
    }

    const UVec2 room_layout_dims = state.stage.GetRoomLayoutDims();
    const UVec2 room_dims = state.stage.GetRegularRoomGridRoomDims();
    for (const Vec2& render_offset : render_offsets) {
        for (unsigned int y = 0; y < room_layout_dims.y; ++y) {
            for (unsigned int x = 0; x < room_layout_dims.x; ++x) {
                const Vec2 room_tl = ToVec2(state.stage.GetRegularRoomGridTlWc(IVec2::New(
                    static_cast<int>(x),
                    static_cast<int>(y)
                ))) + render_offset;
                const SDL_FRect room_rect = WorldRectToScreen(
                    graphics,
                    presentation,
                    room_tl,
                    ToVec2(room_dims)
                );
                if (!IsScreenRectVisible(presentation, room_rect)) {
                    continue;
                }

                if (state.debug_overlay.show_chunk_boundaries) {
                    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
                    SDL_RenderRect(renderer, &room_rect);
                }
                if (state.debug_overlay.show_chunk_coords) {
                    char label[32];
                    std::snprintf(label, sizeof(label), "(%u,%u)", x, y);
                    DrawText(
                        renderer,
                        graphics,
                        10,
                        graphics.ui_font,
                        label,
                        room_rect.x + 2.0F,
                        room_rect.y + 2.0F,
                        SDL_Color{255, 0, 255, 255}
                    );
                }
            }
        }
    }
}

void RenderTileOverlay(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& presentation,
    const std::vector<Vec2>& render_offsets
) {
    if (!state.debug_overlay.show_tile_indexes && !state.debug_overlay.show_tile_types) {
        return;
    }

    const VisibleWorldRect visible = GetVisibleWorldRect(graphics);
    for (const Vec2& render_offset : render_offsets) {
        for (const WorldTileQueryResult& tile_query : QueryTilesInWorldRect(
                 state.stage,
                 ToIVec2(visible.tl - render_offset),
                 ToIVec2(visible.br - render_offset))) {
            if (tile_query.tile == nullptr) {
                continue;
            }

            const Vec2 tile_tl = Vec2::New(
                static_cast<float>(tile_query.tile_pos.x * static_cast<int>(kTileSize)),
                static_cast<float>(tile_query.tile_pos.y * static_cast<int>(kTileSize))
            ) + render_offset;
            const SDL_FRect tile_rect = WorldRectToScreen(
                graphics,
                presentation,
                tile_tl,
                Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
            );
            if (!IsScreenRectVisible(presentation, tile_rect)) {
                continue;
            }

            float text_y = tile_rect.y + 1.0F;
            if (state.debug_overlay.show_tile_indexes) {
                char label[32];
                std::snprintf(
                    label,
                    sizeof(label),
                    "%d,%d",
                    tile_query.tile_pos.x,
                    tile_query.tile_pos.y
                );
                DrawText(
                    renderer,
                    graphics,
                    10,
                    graphics.ui_font,
                    label,
                    tile_rect.x + 1.0F,
                    text_y,
                    SDL_Color{160, 255, 255, 255}
                );
                text_y += 10.0F;
            }
            if (state.debug_overlay.show_tile_types) {
                DrawText(
                    renderer,
                    graphics,
                    10,
                    graphics.ui_font,
                    TileToString(*tile_query.tile),
                    tile_rect.x + 1.0F,
                    text_y,
                    SDL_Color{255, 255, 160, 255}
                );
            }
        }
    }
}

void RenderTileOpennessOverlay(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& presentation,
    const std::vector<Vec2>& render_offsets
) {
    if (!state.debug_overlay.show_tile_openness) {
        return;
    }

    const VisibleWorldRect visible = GetVisibleWorldRect(graphics);
    for (const Vec2& render_offset : render_offsets) {
        for (const WorldTileQueryResult& tile_query : QueryTilesInWorldRect(
                 state.stage,
                 ToIVec2(visible.tl - render_offset),
                 ToIVec2(visible.br - render_offset))) {
            if (tile_query.tile == nullptr) {
                continue;
            }

            const Vec2 tile_tl = TileTopLeftWorld(tile_query.tile_pos) + render_offset;
            const SDL_FRect tile_rect = WorldRectToScreen(
                graphics,
                presentation,
                tile_tl,
                Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
            );
            if (!IsScreenRectVisible(presentation, tile_rect)) {
                continue;
            }

            const float openness = GetStageTileOpenness(
                state,
                tile_query.tile_pos.x,
                tile_query.tile_pos.y
            );
            const SDL_Color fill_color = TileOpennessColor(openness, 104);
            SDL_SetRenderDrawColor(
                renderer,
                fill_color.r,
                fill_color.g,
                fill_color.b,
                fill_color.a
            );
            SDL_RenderFillRect(renderer, &tile_rect);

            if (tile_rect.w >= 36.0F && tile_rect.h >= 18.0F) {
                char label[16];
                std::snprintf(label, sizeof(label), "%.2f", openness);
                DrawText(
                    renderer,
                    graphics,
                    10,
                    graphics.ui_font,
                    label,
                    tile_rect.x + 1.0F,
                    tile_rect.y + 1.0F,
                    SDL_Color{255, 255, 255, 255}
                );
            }
        }
    }
}

void RenderFluidAmountOverlay(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& presentation,
    const std::vector<Vec2>& render_offsets
) {
    if (!state.debug_overlay.show_fluid_amounts) {
        return;
    }

    const VisibleWorldRect visible = GetVisibleWorldRect(graphics);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (const Vec2& render_offset : render_offsets) {
        for (const WorldTileQueryResult& tile_query : QueryTilesInWorldRect(
                 state.stage,
                 ToIVec2(visible.tl - render_offset),
                 ToIVec2(visible.br - render_offset))) {
            if (!state.stage.IsTileCoordInside(tile_query.tile_pos.x, tile_query.tile_pos.y)) {
                continue;
            }

            const auto tile_x = static_cast<unsigned int>(tile_query.tile_pos.x);
            const auto tile_y = static_cast<unsigned int>(tile_query.tile_pos.y);
            const Tile fluid_tile = state.stage.GetFluidTile(tile_x, tile_y);
            const float amount = state.stage.GetFluidAmount(tile_x, tile_y);
            if (!GetTileArchetype(fluid_tile).simulated_fluid || amount <= 0.0F) {
                continue;
            }

            const Vec2 tile_tl = TileTopLeftWorld(tile_query.tile_pos) + render_offset;
            const SDL_FRect tile_rect = WorldRectToScreen(
                graphics,
                presentation,
                tile_tl,
                Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
            );
            if (!IsScreenRectVisible(presentation, tile_rect)) {
                continue;
            }

            const std::uint8_t alpha = static_cast<std::uint8_t>(
                48 + ((static_cast<int>(std::round(amount)) * 128) / 255)
            );
            SDL_SetRenderDrawColor(renderer, 32, 180, 255, alpha);
            SDL_RenderFillRect(renderer, &tile_rect);
            SDL_SetRenderDrawColor(renderer, 128, 232, 255, 255);
            SDL_RenderRect(renderer, &tile_rect);

            if (tile_rect.w >= 24.0F && tile_rect.h >= 14.0F) {
                char label[16];
                std::snprintf(
                    label,
                    sizeof(label),
                    "%u",
                    static_cast<unsigned int>(std::round(amount))
                );
                DrawText(
                    renderer,
                    graphics,
                    10,
                    graphics.ui_font,
                    label,
                    tile_rect.x + 1.0F,
                    tile_rect.y + 1.0F,
                    SDL_Color{255, 255, 255, 255}
                );
            }
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void RenderLightOverlay(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& presentation,
    const std::vector<Vec2>& render_offsets
) {
    if (!state.debug_overlay.show_lights) {
        return;
    }

    for (const Vec2& render_offset : render_offsets) {
        for (const StageLight& light : state.stage.lights) {
            const Vec2 tile_tl = Vec2::New(
                static_cast<float>(light.tile_pos.x * static_cast<int>(kTileSize)),
                static_cast<float>(light.tile_pos.y * static_cast<int>(kTileSize))
            ) + render_offset;
            const SDL_FRect tile_rect = WorldRectToScreen(
                graphics,
                presentation,
                tile_tl,
                Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
            );
            if (!IsScreenRectVisible(presentation, tile_rect)) {
                continue;
            }

            SDL_SetRenderDrawColor(renderer, 255, 200, 64, 255);
            SDL_RenderRect(renderer, &tile_rect);

            char label[64];
            std::snprintf(label, sizeof(label), "L%zu b%d", light.vid.id, light.radius);
            DrawText(
                renderer,
                graphics,
                10,
                graphics.ui_font,
                label,
                tile_rect.x + 1.0F,
                tile_rect.y - 10.0F,
                SDL_Color{255, 200, 64, 255}
            );
        }
    }
}

void RenderAreaOverlay(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const State& state,
    const SDL_FRect& presentation,
    const std::vector<Vec2>& render_offsets
) {
    if (!state.debug_overlay.show_area_boundaries &&
        !state.debug_overlay.show_area_ids &&
        !state.debug_overlay.show_area_types) {
        return;
    }

    for (const Vec2& render_offset : render_offsets) {
        for (const Entity& entity : state.entity_manager.entities) {
            if (!entity.active || entity.type_ != EntityType::Shop) {
                continue;
            }

            const AABB area = entities::shop::GetShopArea(entity);
            const Vec2 area_size = area.br - area.tl + Vec2::New(1.0F, 1.0F);
            const SDL_FRect area_rect = WorldRectToScreen(
                graphics,
                presentation,
                area.tl + render_offset,
                area_size
            );
            if (!IsScreenRectVisible(presentation, area_rect)) {
                continue;
            }

            if (state.debug_overlay.show_area_boundaries) {
                SDL_SetRenderDrawColor(renderer, 96, 255, 160, 255);
                SDL_RenderRect(renderer, &area_rect);
            }

            float text_y = area_rect.y + 2.0F;
            if (state.debug_overlay.show_area_ids) {
                char label[32];
                std::snprintf(label, sizeof(label), "shop %zu", entity.vid.id);
                DrawText(
                    renderer,
                    graphics,
                    10,
                    graphics.ui_font,
                    label,
                    area_rect.x + 2.0F,
                    text_y,
                    SDL_Color{96, 255, 160, 255}
                );
                text_y += 10.0F;
            }

            if (state.debug_overlay.show_area_types) {
                DrawText(
                    renderer,
                    graphics,
                    10,
                    graphics.ui_font,
                    "Shop",
                    area_rect.x + 2.0F,
                    text_y,
                    SDL_Color{160, 255, 224, 255}
                );
            }
        }
    }
}

} // namespace

void RenderAudioEmitterOverlay(
    SDL_Renderer* renderer,
    Graphics& graphics,
    State& state,
    const Audio& audio,
    const SDL_FRect& presentation,
    const std::vector<Vec2>& render_offsets
) {
    if (!state.debug_overlay.show_audio_emitters) {
        return;
    }

    const bool show_occlusion_paths =
        state.debug_overlay.show_audio_occlusion_paths &&
        IsAudioOcclusionEnabled(state);
    const Vec2 listener_world = GetAudioListenerWorldPos(state);

    for (const AudioEmitter& emitter : state.audio_emitters.emitters) {
        if (!emitter.active) {
            continue;
        }

        Vec2 anchor_world = emitter.world_pos;
        if (emitter.attached_entity_vid.has_value()) {
            const Entity* const attached = state.entity_manager.GetEntity(*emitter.attached_entity_vid);
            if (attached != nullptr) {
                anchor_world = GetNearestWorldPoint(state.stage, emitter.world_pos, attached->GetCenter());
            }
        }

        for (const Vec2& render_offset : render_offsets) {
            const Vec2 source_world = emitter.world_pos + render_offset;
            const SDL_FRect source_rect = WorldRectToScreen(
                graphics,
                presentation,
                source_world - Vec2::New(2.0F, 2.0F),
                Vec2::New(4.0F, 4.0F)
            );
            if (!IsScreenRectVisible(presentation, source_rect)) {
                continue;
            }

            SDL_SetRenderDrawColor(
                renderer,
                emitter.playback_mode == AudioEmitterPlaybackMode::Looping ? 255 : 120,
                220,
                emitter.playback_mode == AudioEmitterPlaybackMode::Looping ? 96 : 255,
                255
            );
            SDL_RenderRect(renderer, &source_rect);

            if (show_occlusion_paths) {
                const Vec2 listener_for_source =
                    GetNearestWorldPoint(state.stage, source_world, listener_world);
                const Vec2 ray_delta =
                    GetNearestWorldDelta(state.stage, source_world, listener_for_source);
                const float ray_length =
                    std::sqrt((ray_delta.x * ray_delta.x) + (ray_delta.y * ray_delta.y));
                const WorldRayHit hit = RaycastTiles(
                    source_world,
                    ray_delta,
                    static_cast<int>(std::ceil(ray_length)) + 1,
                    state
                );
                const bool occluded =
                    ShouldAudioRayHitCountAsOccluded(state, listener_for_source, hit);
                const SDL_Color occlusion_color = occluded
                    ? SDL_Color{255, 48, 48, 220}
                    : SDL_Color{64, 255, 96, 220};
                const float listener_epsilon_px =
                    std::max(0.0F, state.settings.audio.acoustics_occlusion_listener_epsilon_px);
                if (listener_epsilon_px > 0.0F) {
                    RenderWorldCircleOutline(
                        renderer,
                        graphics,
                        presentation,
                        listener_for_source,
                        listener_epsilon_px,
                        SDL_Color{255, 220, 32, 255}
                    );
                }
                const Vec2 source_screen = WorldPointToScreen(graphics, presentation, source_world);
                const Vec2 listener_screen =
                    WorldPointToScreen(graphics, presentation, listener_for_source);
                RenderScreenLine(
                    renderer,
                    source_screen,
                    listener_screen,
                    occlusion_color,
                    3
                );
            }

            if (emitter.attached_entity_vid.has_value()) {
                const Vec2 anchor_source_world = anchor_world + render_offset;
                const Vec2 anchor_screen = WorldPointToScreen(graphics, presentation, anchor_source_world);
                const Vec2 source_screen = WorldPointToScreen(graphics, presentation, source_world);
                SDL_RenderLine(
                    renderer,
                    anchor_screen.x,
                    anchor_screen.y,
                    source_screen.x,
                    source_screen.y
                );
                SDL_RenderLine(renderer, anchor_screen.x - 4.0F, anchor_screen.y, anchor_screen.x + 4.0F, anchor_screen.y);
                SDL_RenderLine(renderer, anchor_screen.x, anchor_screen.y - 4.0F, anchor_screen.x, anchor_screen.y + 4.0F);
            }

            char label[160];
            const int owner_id = emitter.owner_entity_vid.has_value() ? static_cast<int>(emitter.owner_entity_vid->id) : -1;
            const int attach_id = emitter.attached_entity_vid.has_value() ? static_cast<int>(emitter.attached_entity_vid->id) : -1;
            std::snprintf(
                label,
                sizeof(label),
                "em %zu %s %s own:%d src:%d %s miss:%s",
                emitter.vid.id,
                audio.GetAudioAssetNameCStr(emitter.audio_asset_id),
                AudioEmitterPlaybackModeToString(emitter.playback_mode),
                owner_id,
                attach_id,
                AudioEmitterSourceModeToString(emitter.source_mode),
                AudioEmitterTargetLossPolicyToString(emitter.target_loss_policy)
            );
            DrawText(
                renderer,
                graphics,
                10,
                graphics.ui_font,
                label,
                source_rect.x + 6.0F,
                source_rect.y - 12.0F,
                SDL_Color{255, 220, 96, 255}
            );
        }
    }
}

void RenderDebugOverlay(
    SDL_Renderer* renderer,
    Graphics& graphics,
    State& state,
    const Audio& audio
) {
    if (!state.debug_overlay.show_entity_collision_boxes &&
        !state.debug_overlay.show_entity_ids &&
        !state.debug_overlay.show_entity_types &&
        !state.debug_overlay.show_entity_render_centers &&
        !state.debug_overlay.show_void_death_line &&
        !state.debug_overlay.show_chunk_boundaries &&
        !state.debug_overlay.show_chunk_coords &&
        !state.debug_overlay.show_tile_indexes &&
        !state.debug_overlay.show_tile_types &&
        !state.debug_overlay.show_tile_openness &&
        !state.debug_overlay.show_fluid_amounts &&
        !state.debug_overlay.show_lights &&
        !state.debug_overlay.show_area_boundaries &&
        !state.debug_overlay.show_area_ids &&
        !state.debug_overlay.show_area_types &&
        !state.debug_overlay.show_audio_emitters &&
        !state.debug_overlay.show_audio_occlusion_paths &&
        !state.debug_overlay.show_debug_annotations &&
        !ShouldRenderShakeBrushPreview(state) &&
        !ShouldRenderAudioBrushPreview(state)) {
        return;
    }

    if (state.debug_overlay.show_tile_openness || state.debug_audio_brush.show_openness_rays) {
        EnsureStageAcoustics(state);
    }

    const SDL_FRect presentation = GetDebugPresentationRect(renderer, graphics);
    const std::vector<Vec2> render_offsets =
        GetVisibleWrappedRenderOffsets(state.stage, graphics);
    if (state.debug_overlay.show_entity_collision_boxes) {
        RenderEntityCollisionBoxes(renderer, graphics, state, presentation, render_offsets);
    }
    if (state.debug_overlay.show_entity_ids || state.debug_overlay.show_entity_types) {
        RenderEntityLabels(renderer, graphics, state, presentation, render_offsets);
    }
    if (state.debug_overlay.show_entity_render_centers) {
        RenderEntityRenderCenters(renderer, graphics, state, presentation, render_offsets);
    }
    if (state.debug_overlay.show_void_death_line) {
        RenderVoidDeathLine(renderer, graphics, state, presentation, render_offsets);
    }
    if (state.debug_overlay.show_chunk_boundaries || state.debug_overlay.show_chunk_coords) {
        RenderChunkOverlay(renderer, graphics, state, presentation, render_offsets);
    }
    if (state.debug_overlay.show_tile_openness) {
        RenderTileOpennessOverlay(renderer, graphics, state, presentation, render_offsets);
    }
    if (state.debug_overlay.show_fluid_amounts) {
        RenderFluidAmountOverlay(renderer, graphics, state, presentation, render_offsets);
    }
    if (state.debug_overlay.show_tile_indexes || state.debug_overlay.show_tile_types) {
        RenderTileOverlay(renderer, graphics, state, presentation, render_offsets);
    }
    if (state.debug_overlay.show_lights) {
        RenderLightOverlay(renderer, graphics, state, presentation, render_offsets);
    }
    if (state.debug_overlay.show_area_boundaries ||
        state.debug_overlay.show_area_ids ||
        state.debug_overlay.show_area_types) {
        RenderAreaOverlay(renderer, graphics, state, presentation, render_offsets);
    }
    if (state.debug_overlay.show_audio_emitters) {
        RenderAudioEmitterOverlay(renderer, graphics, state, audio, presentation, render_offsets);
    }
    if (state.debug_overlay.show_debug_annotations) {
        RenderDebugAnnotations(renderer, graphics, state, presentation, render_offsets);
    }
    RenderShakeBrushPreview(renderer, graphics, state, presentation);
    RenderFluidBrushPreview(renderer, graphics, state, presentation);
    RenderAudioBrushPreview(renderer, graphics, state, presentation);
}

} // namespace splonks
