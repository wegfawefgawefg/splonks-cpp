#include "render/debug.hpp"

#include "entity/archetype.hpp"
#include "entity.hpp"
#include "entities/common/common.hpp"
#include "graphics.hpp"
#include "state.hpp"
#include "text.hpp"
#include "tile.hpp"
#include "world_query.hpp"

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
    const Vec2 relative = world_pos - graphics.camera.target;
    const Vec2 internal_screen = relative * graphics.camera.zoom + graphics.camera.offset;
    const Vec2 screen = Vec2::New(
        presentation.x + internal_screen.x * presentation_scale,
        presentation.y + internal_screen.y * presentation_scale
    );
    return SDL_FRect{
        screen.x,
        screen.y,
        world_size.x * graphics.camera.zoom * presentation_scale,
        world_size.y * graphics.camera.zoom * presentation_scale,
    };
}

Vec2 WorldPointToScreen(
    const Graphics& graphics,
    const SDL_FRect& presentation,
    const Vec2& world_pos
) {
    const float presentation_scale = presentation.w / static_cast<float>(graphics.dims.x);
    const Vec2 relative = world_pos - graphics.camera.target;
    const Vec2 internal_screen = relative * graphics.camera.zoom + graphics.camera.offset;
    return Vec2::New(
        presentation.x + internal_screen.x * presentation_scale,
        presentation.y + internal_screen.y * presentation_scale
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
            if (entity.type_ == EntityType::Player) {
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
    const UVec2 room_dims = state.stage.GetRoomDims();
    for (const Vec2& render_offset : render_offsets) {
        for (unsigned int y = 0; y < room_layout_dims.y; ++y) {
            for (unsigned int x = 0; x < room_layout_dims.x; ++x) {
                const Vec2 room_tl = ToVec2(state.stage.GetRoomTlWc(IVec2::New(
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

} // namespace

void RenderDebugOverlay(SDL_Renderer* renderer, Graphics& graphics, const State& state) {
    if (!state.debug_overlay.show_entity_collision_boxes &&
        !state.debug_overlay.show_entity_ids &&
        !state.debug_overlay.show_entity_types &&
        !state.debug_overlay.show_void_death_line &&
        !state.debug_overlay.show_chunk_boundaries &&
        !state.debug_overlay.show_chunk_coords &&
        !state.debug_overlay.show_tile_indexes &&
        !state.debug_overlay.show_tile_types) {
        return;
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
    if (state.debug_overlay.show_void_death_line) {
        RenderVoidDeathLine(renderer, graphics, state, presentation, render_offsets);
    }
    if (state.debug_overlay.show_chunk_boundaries || state.debug_overlay.show_chunk_coords) {
        RenderChunkOverlay(renderer, graphics, state, presentation, render_offsets);
    }
    if (state.debug_overlay.show_tile_indexes || state.debug_overlay.show_tile_types) {
        RenderTileOverlay(renderer, graphics, state, presentation, render_offsets);
    }
}

} // namespace splonks
