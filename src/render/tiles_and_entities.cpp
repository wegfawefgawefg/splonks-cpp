#include "render/tiles_and_entities.hpp"

#include "entity/archetype.hpp"
#include "entity.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "render/stone_overlay.hpp"
#include "render/tile_lighting.hpp"
#include "particles/particle.hpp"
#include "state.hpp"
#include "stage_lighting.hpp"
#include "tile.hpp"
#include <algorithm>
#include <cmath>

namespace splonks {

namespace {

struct VisibleWorldRect {
    Vec2 tl;
    Vec2 br;
};

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

bool ShouldRenderBackgroundStamp(const State& state, const BackgroundStamp& stamp) {
    switch (stamp.condition) {
    case BackgroundStampCondition::None:
        return true;
    case BackgroundStampCondition::Wanted:
        for (const Entity& entity : state.entity_manager.entities) {
            if (entity.active && entity.wanted) {
                return true;
            }
        }
        return false;
    }

    return true;
}

bool ShouldRevealEmbeddedTreasure(const State& state) {
    for (const Entity& entity : state.entity_manager.entities) {
        if (!entity.active) {
            continue;
        }
        if (CanRevealEmbeddedTreasure(entity)) {
            return true;
        }
    }
    return false;
}

const FrameData* GetFirstFrameForAnimationOrFallback(
    const Graphics& graphics,
    FrameDataId animation_id
) {
    const FrameDataAnimation* animation = graphics.frame_data_db.FindAnimation(animation_id);
    if (animation == nullptr || animation->frame_indices.empty()) {
        animation = graphics.frame_data_db.FindAnimation(frame_data_ids::NoSprite);
        if (animation == nullptr || animation->frame_indices.empty()) {
            return nullptr;
        }
    }
    return &graphics.frame_data_db.frames[animation->frame_indices[0]];
}

} // namespace

void RenderStageTiles(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    EnsureStageLighting(state);
    const std::vector<Vec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const Vec2& render_offset : render_offsets) {
        for (std::size_t y = 0; y < state.stage.tiles.size(); ++y) {
            for (std::size_t x = 0; x < state.stage.tiles[y].size(); ++x) {
                const Tile tile = state.stage.tiles[y][x];
                const Tile backwall_tile = state.stage.backwall_tiles[y][x];
                const IVec2 tile_pos = IVec2::New(
                    static_cast<int>(x * kTileSize),
                    static_cast<int>(y * kTileSize)
                );
                const SDL_FRect dst = WorldRectToScreen(
                    graphics,
                    ToVec2(tile_pos) + render_offset,
                    Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                );

                if (backwall_tile != Tile::Air) {
                    const TileSourceData* const backwall_source_data =
                        GetTileSourceData(graphics, backwall_tile, tile_pos);
                    if (backwall_source_data != nullptr) {
                        SDL_Texture* const backwall_texture =
                            GetTileTexture(graphics, *backwall_source_data);
                        if (backwall_texture != nullptr) {
                            const SDL_FRect backwall_src{
                                static_cast<float>(backwall_source_data->sample_rect.x),
                                static_cast<float>(backwall_source_data->sample_rect.y),
                                static_cast<float>(backwall_source_data->sample_rect.w),
                                static_cast<float>(backwall_source_data->sample_rect.h),
                            };
                            ApplyBackwallTileBrightness(
                                backwall_texture,
                                state,
                                graphics,
                                static_cast<int>(x),
                                static_cast<int>(y)
                            );
                            SDL_RenderTexture(renderer, backwall_texture, &backwall_src, &dst);
                            ResetTerrainTileBrightness(backwall_texture);
                        }
                    }
                }

                if (tile == Tile::Air) {
                    continue;
                }

                const TileSourceData* const tile_source_data =
                    GetTileSourceData(graphics, tile, tile_pos);
                if (tile_source_data == nullptr) {
                    continue;
                }
                SDL_Texture* const tile_texture = GetTileTexture(graphics, *tile_source_data);
                if (tile_texture == nullptr) {
                    continue;
                }
                const SDL_FRect src{
                    static_cast<float>(tile_source_data->sample_rect.x),
                    static_cast<float>(tile_source_data->sample_rect.y),
                    static_cast<float>(tile_source_data->sample_rect.w),
                    static_cast<float>(tile_source_data->sample_rect.h),
                };

                ApplyTerrainTileBrightness(
                    tile_texture,
                    state,
                    graphics,
                    static_cast<int>(x),
                    static_cast<int>(y)
                );
                SDL_RenderTexture(renderer, tile_texture, &src, &dst);
                ResetTerrainTileBrightness(tile_texture);
                RenderTerrainTileLighting(
                    renderer,
                    state,
                    graphics,
                    static_cast<int>(x),
                    static_cast<int>(y),
                    dst
                );
            }
        }
    }
}

void RenderStageTileWrapper(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    EnsureStageLighting(state);
    const TileSet air_tile_set = TileSetForStageType(state.stage.stage_type);

    const VisibleWorldRect visible = GetVisibleWorldRect(graphics);
    const Vec2 visible_tl_wc = visible.tl;
    const Vec2 visible_br_wc = visible.br;

    const int visible_tl_tile_x =
        static_cast<int>(std::floor(visible_tl_wc.x / static_cast<float>(kTileSize))) - 1;
    const int visible_tl_tile_y =
        static_cast<int>(std::floor(visible_tl_wc.y / static_cast<float>(kTileSize))) - 1;
    const int visible_br_tile_x =
        static_cast<int>(std::ceil(visible_br_wc.x / static_cast<float>(kTileSize))) + 1;
    const int visible_br_tile_y =
        static_cast<int>(std::ceil(visible_br_wc.y / static_cast<float>(kTileSize))) + 1;

    const int stage_tile_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_tile_height = static_cast<int>(state.stage.GetTileHeight());

    for (int tile_y = visible_tl_tile_y; tile_y <= visible_br_tile_y; ++tile_y) {
        for (int tile_x = visible_tl_tile_x; tile_x <= visible_br_tile_x; ++tile_x) {
            const bool inside_stage = tile_x >= 0 && tile_y >= 0 && tile_x < stage_tile_width &&
                                      tile_y < stage_tile_height;
            if (!inside_stage) {
                const std::optional<StageBorderSideKind> side =
                    state.stage.GetOutOfBoundsSideForTileCoord(tile_x, tile_y);
                if (!side.has_value()) {
                    continue;
                }
                const Tile border_tile = state.stage.GetBorderTile(*side);
                const IVec2 tile_pos = IVec2::New(
                    tile_x * static_cast<int>(kTileSize),
                    tile_y * static_cast<int>(kTileSize)
                );
                if (border_tile == Tile::Air) {
                    const TileSourceData* const air_source_data =
                        GetAirSourceData(graphics, air_tile_set, tile_pos);
                    if (air_source_data == nullptr) {
                        continue;
                    }
                    SDL_Texture* const air_texture = GetTileTexture(graphics, *air_source_data);
                    if (air_texture == nullptr) {
                        continue;
                    }
                    const SDL_FRect air_src{
                        static_cast<float>(air_source_data->sample_rect.x),
                        static_cast<float>(air_source_data->sample_rect.y),
                        static_cast<float>(air_source_data->sample_rect.w),
                        static_cast<float>(air_source_data->sample_rect.h),
                    };
                    const SDL_FRect dst = WorldRectToScreen(
                        graphics,
                        ToVec2(tile_pos),
                        Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                    );
                    ApplyBackwallTileBrightness(air_texture, state, graphics, tile_x, tile_y);
                    SDL_RenderTexture(renderer, air_texture, &air_src, &dst);
                    ResetTerrainTileBrightness(air_texture);
                    continue;
                }
                const TileSourceData* const tile_source_data =
                    GetTileSourceData(graphics, border_tile, tile_pos);
                if (tile_source_data == nullptr) {
                    continue;
                }
                SDL_Texture* const tile_texture = GetTileTexture(graphics, *tile_source_data);
                if (tile_texture == nullptr) {
                    continue;
                }
                const SDL_FRect src{
                    static_cast<float>(tile_source_data->sample_rect.x),
                    static_cast<float>(tile_source_data->sample_rect.y),
                    static_cast<float>(tile_source_data->sample_rect.w),
                    static_cast<float>(tile_source_data->sample_rect.h),
                };
                const SDL_FRect dst = WorldRectToScreen(
                    graphics,
                    ToVec2(tile_pos),
                    Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                );
                ApplyTerrainTileBrightness(tile_texture, state, graphics, tile_x, tile_y);
                SDL_RenderTexture(renderer, tile_texture, &src, &dst);
                ResetTerrainTileBrightness(tile_texture);
                RenderTerrainTileLighting(renderer, state, graphics, tile_x, tile_y, dst);
            }
        }
    }
}

void RenderBackgroundStamps(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    EnsureStageLighting(state);
    const std::vector<Vec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const BackgroundStamp& stamp : state.stage.background_stamps) {
        if (stamp.animation_id == kInvalidFrameDataId) {
            continue;
        }
        if (!ShouldRenderBackgroundStamp(state, stamp)) {
            continue;
        }

        const FrameData* const frame_data =
            GetFirstFrameForAnimationOrFallback(graphics, stamp.animation_id);
        if (frame_data == nullptr) {
            continue;
        }
        SDL_Texture* const sprite_texture = graphics.GetFrameDataTexture(frame_data->image_id);
        if (sprite_texture == nullptr) {
            continue;
        }

        const SDL_FRect src{
            static_cast<float>(frame_data->sample_rect.x),
            static_cast<float>(frame_data->sample_rect.y),
            static_cast<float>(frame_data->sample_rect.w),
            static_cast<float>(frame_data->sample_rect.h),
        };

        const int tile_x =
            static_cast<int>((stamp.pos.x + (static_cast<float>(frame_data->sample_rect.w) * 0.5F)) /
                             static_cast<float>(kTileSize));
        const int tile_y =
            static_cast<int>((stamp.pos.y + (static_cast<float>(frame_data->sample_rect.h) * 0.5F)) /
                             static_cast<float>(kTileSize));
        for (const Vec2& render_offset : render_offsets) {
            const SDL_FRect dst = WorldRectToScreen(
                graphics,
                stamp.pos + render_offset,
                Vec2::New(
                    static_cast<float>(frame_data->sample_rect.w),
                    static_cast<float>(frame_data->sample_rect.h)
                )
            );
            ApplyBackwallTileBrightness(sprite_texture, state, graphics, tile_x, tile_y);
            SDL_RenderTexture(renderer, sprite_texture, &src, &dst);
            ResetTerrainTileBrightness(sprite_texture);
        }
    }
}

void RenderEmbeddedTreasureOverlays(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    if (!ShouldRevealEmbeddedTreasure(state)) {
        return;
    }

    EnsureStageLighting(state);
    const std::vector<Vec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (std::size_t y = 0; y < state.stage.embedded_treasures.size(); ++y) {
        for (std::size_t x = 0; x < state.stage.embedded_treasures[y].size(); ++x) {
            const EntityType embedded_treasure = state.stage.embedded_treasures[y][x];
            if (embedded_treasure == EntityType::None) {
                continue;
            }

            const FrameData* const frame_data = GetFirstFrameForAnimationOrFallback(
                graphics,
                GetDefaultAnimationIdForArchetype(embedded_treasure)
            );
            if (frame_data == nullptr) {
                continue;
            }

            SDL_Texture* const sprite_texture = graphics.GetFrameDataTexture(frame_data->image_id);
            if (sprite_texture == nullptr) {
                continue;
            }

            const Vec2 tile_world_pos = Vec2::New(
                static_cast<float>(x * kTileSize),
                static_cast<float>(y * kTileSize)
            );
            const int render_offset_x =
                (static_cast<int>(kTileSize) - frame_data->sample_rect.w) / 2;
            const int render_offset_y =
                (static_cast<int>(kTileSize) - frame_data->sample_rect.h) / 2;
            const Vec2 render_world_pos = tile_world_pos + Vec2::New(
                static_cast<float>(render_offset_x),
                static_cast<float>(render_offset_y)
            );
            const SDL_FRect src{
                static_cast<float>(frame_data->sample_rect.x),
                static_cast<float>(frame_data->sample_rect.y),
                static_cast<float>(frame_data->sample_rect.w),
                static_cast<float>(frame_data->sample_rect.h),
            };
            ApplyTerrainTileBrightness(
                sprite_texture,
                state,
                graphics,
                static_cast<int>(x),
                static_cast<int>(y)
            );
            SDL_SetTextureAlphaMod(sprite_texture, 224);
            for (const Vec2& render_offset : render_offsets) {
                const SDL_FRect dst = WorldRectToScreen(
                    graphics,
                    render_world_pos + render_offset,
                    Vec2::New(
                        static_cast<float>(frame_data->sample_rect.w),
                        static_cast<float>(frame_data->sample_rect.h)
                    )
                );
                SDL_RenderTexture(renderer, sprite_texture, &src, &dst);
            }
            SDL_SetTextureAlphaMod(sprite_texture, 255);
            ResetTerrainTileBrightness(sprite_texture);
        }
    }
}

void RenderEntities(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    const std::vector<Vec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    std::vector<std::size_t> draw_queue;
    std::vector<std::size_t> next_draw_queue;
    next_draw_queue.reserve(state.entity_manager.entities.size());
    for (std::size_t i = 0; i < state.entity_manager.entities.size(); ++i) {
        next_draw_queue.push_back(i);
    }

    for (DrawLayer layer : {DrawLayer::Background, DrawLayer::Middle, DrawLayer::Foreground}) {
        draw_queue.clear();
        draw_queue.insert(draw_queue.end(), next_draw_queue.begin(), next_draw_queue.end());
        next_draw_queue.clear();
        for (std::size_t entity_id : draw_queue) {
            const Entity& entity = state.entity_manager.entities[entity_id];
            if (!entity.active || !entity.render_enabled) {
                continue;
            }
            if (entity.draw_layer != layer) {
                next_draw_queue.push_back(entity_id);
                continue;
            }

            const FrameData* const frame_data =
                entities::common::GetCurrentFrameDataForEntity(entity, graphics);
            if (frame_data == nullptr) {
                continue;
            }

            SDL_Texture* const sprite_texture =
                graphics.GetFrameDataTexture(frame_data->image_id);
            if (sprite_texture == nullptr) {
                continue;
            }

            const Vec2 sprite_world_size = Vec2::New(
                static_cast<float>(frame_data->sample_rect.w),
                static_cast<float>(frame_data->sample_rect.h)
            );
            const Vec2 sprite_scaled_size =
                sprite_world_size * entity.frame_data_animator.scale;
            const Vec2 render_position =
                entities::common::GetSpriteTopLeftForEntity(entity, *frame_data);

            const SDL_FRect src{
                static_cast<float>(frame_data->sample_rect.x),
                static_cast<float>(frame_data->sample_rect.y),
                static_cast<float>(frame_data->sample_rect.w),
                static_cast<float>(frame_data->sample_rect.h),
            };
            const SDL_FlipMode flip =
                entity.facing == LeftOrRight::Right ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            for (const Vec2& render_offset : render_offsets) {
                SDL_FRect dst = WorldRectToScreen(
                    graphics,
                    render_position + render_offset,
                    sprite_scaled_size
                );
                SDL_RenderTextureRotated(
                    renderer,
                    sprite_texture,
                    &src,
                    &dst,
                    0.0,
                    nullptr,
                    flip
                );
                if (entity.stone) {
                    const AABB stone_overlay_aabb = entity.GetAABB();
                    RenderStoneEntityOverlay(
                        renderer,
                        state,
                        graphics,
                        stone_overlay_aabb.tl + render_offset,
                        stone_overlay_aabb.br - stone_overlay_aabb.tl + Vec2::New(1.0F, 1.0F)
                    );
                }
            }
            continue;

        }
    }
}

void RenderParticles(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    const std::vector<Vec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const auto& particle : state.particles.effects) {
        const FrameDataAnimator& animator = particle->GetFrameDataAnimator();
        if (!animator.HasAnimation()) {
            continue;
        }

        const FrameData* const frame_data =
            graphics.frame_data_db.FindFrame(animator.animation_id, animator.current_frame);
        if (frame_data == nullptr) {
            continue;
        }
        SDL_Texture* const texture = graphics.GetFrameDataTexture(frame_data->image_id);
        if (texture == nullptr) {
            continue;
        }

        const Vec2 pos = particle->GetPos();
        const Vec2 size = particle->GetSize();
        const float rotation = particle->GetRot();
        const float alpha = particle->GetAlpha();
        const Vec2 half_size = size / 2.0F;
        SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(alpha * 255.0F));
        const SDL_FRect src{
            static_cast<float>(frame_data->sample_rect.x),
            static_cast<float>(frame_data->sample_rect.y),
            static_cast<float>(frame_data->sample_rect.w),
            static_cast<float>(frame_data->sample_rect.h),
        };
        for (const Vec2& render_offset : render_offsets) {
            const SDL_FRect dst = WorldRectToScreen(graphics, (pos - half_size) + render_offset, size);
            const SDL_FPoint center{dst.w / 2.0F, dst.h / 2.0F};
            SDL_RenderTextureRotated(renderer, texture, &src, &dst, rotation, &center, SDL_FLIP_NONE);
        }
        SDL_SetTextureAlphaMod(texture, 255);
    }
}

} // namespace splonks
