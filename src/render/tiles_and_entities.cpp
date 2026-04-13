#include "render/tiles_and_entities.hpp"

#include "entity/archetype.hpp"
#include "entity.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "render/stone_overlay.hpp"
#include "render/tile_lighting.hpp"
#include "special_effects/special_effect.hpp"
#include "state.hpp"
#include "render/terrain_lighting.hpp"
#include "tile.hpp"
#include <algorithm>
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
    EnsureTerrainLightingCache(state);
    const TileSet air_tile_set = TileSetForStageType(state.stage.stage_type);
    for (std::size_t y = 0; y < state.stage.tiles.size(); ++y) {
        for (std::size_t x = 0; x < state.stage.tiles[y].size(); ++x) {
            const Tile tile = state.stage.tiles[y][x];
            const IVec2 tile_pos = IVec2::New(
                static_cast<int>(x * kTileSize),
                static_cast<int>(y * kTileSize)
            );
            const SDL_FRect dst = WorldRectToScreen(
                graphics,
                ToVec2(tile_pos),
                Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
            );

            const bool is_air_tile = tile == Tile::Air;
            if (IsTileTransparent(tile)) {
                const TileSourceData* const air_source_data =
                    GetAirSourceData(graphics, air_tile_set, tile_pos);
                if (air_source_data != nullptr) {
                    SDL_Texture* const air_texture = GetTileTexture(graphics, *air_source_data);
                    if (air_texture != nullptr) {
                        const SDL_FRect air_src{
                            static_cast<float>(air_source_data->sample_rect.x),
                            static_cast<float>(air_source_data->sample_rect.y),
                            static_cast<float>(air_source_data->sample_rect.w),
                            static_cast<float>(air_source_data->sample_rect.h),
                        };
                        ApplyBackwallTileBrightness(
                            air_texture,
                            state,
                            graphics,
                            static_cast<int>(x),
                            static_cast<int>(y)
                        );
                        SDL_RenderTexture(renderer, air_texture, &air_src, &dst);
                        ResetTerrainTileBrightness(air_texture);
                    }
                }
            }

            if (is_air_tile) {
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

void RenderStageTileWrapper(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    EnsureTerrainLightingCache(state);
    const TileSet air_tile_set = TileSetForStageType(state.stage.stage_type);
    
    const Vec2 visible_tl_wc = graphics.camera.target - (graphics.camera.offset / graphics.camera.zoom);
    const Vec2 visible_br_wc =
        graphics.camera.target +
        ((ToVec2(graphics.dims) - graphics.camera.offset) / graphics.camera.zoom);

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
                const Tile border_tile = state.stage.GetTileOrBorder(tile_x, tile_y);
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
    EnsureTerrainLightingCache(state);
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
        const SDL_FRect dst = WorldRectToScreen(
            graphics,
            stamp.pos,
            Vec2::New(
                static_cast<float>(frame_data->sample_rect.w),
                static_cast<float>(frame_data->sample_rect.h)
            )
        );

        const int tile_x =
            static_cast<int>((stamp.pos.x + (static_cast<float>(frame_data->sample_rect.w) * 0.5F)) /
                             static_cast<float>(kTileSize));
        const int tile_y =
            static_cast<int>((stamp.pos.y + (static_cast<float>(frame_data->sample_rect.h) * 0.5F)) /
                             static_cast<float>(kTileSize));
        ApplyBackwallTileBrightness(sprite_texture, state, graphics, tile_x, tile_y);
        SDL_RenderTexture(renderer, sprite_texture, &src, &dst);
        ResetTerrainTileBrightness(sprite_texture);
    }
}

void RenderEmbeddedTreasureOverlays(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    if (!ShouldRevealEmbeddedTreasure(state)) {
        return;
    }

    EnsureTerrainLightingCache(state);
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
            const SDL_FRect dst = WorldRectToScreen(
                graphics,
                render_world_pos,
                Vec2::New(
                    static_cast<float>(frame_data->sample_rect.w),
                    static_cast<float>(frame_data->sample_rect.h)
                )
            );
            ApplyTerrainTileBrightness(
                sprite_texture,
                state,
                graphics,
                static_cast<int>(x),
                static_cast<int>(y)
            );
            SDL_SetTextureAlphaMod(sprite_texture, 224);
            SDL_RenderTexture(renderer, sprite_texture, &src, &dst);
            SDL_SetTextureAlphaMod(sprite_texture, 255);
            ResetTerrainTileBrightness(sprite_texture);
        }
    }
}

void RenderEntities(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
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
            if (!entity.active) {
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
            SDL_FRect dst = WorldRectToScreen(graphics, render_position, sprite_scaled_size);
            const SDL_FlipMode flip =
                entity.facing == LeftOrRight::Right ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
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
                    stone_overlay_aabb.tl,
                    stone_overlay_aabb.br - stone_overlay_aabb.tl + Vec2::New(1.0F, 1.0F)
                );
            }
            continue;

        }
    }
}

void RenderSpecialEffects(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    for (const auto& special_effect : state.special_effects) {
        const FrameDataAnimator& animator = special_effect->GetFrameDataAnimator();
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

        const Vec2 pos = special_effect->GetPos();
        const Vec2 size = special_effect->GetSize();
        const float rotation = special_effect->GetRot();
        const float alpha = special_effect->GetAlpha();
        const Vec2 half_size = size / 2.0F;
        const SDL_FRect dst = WorldRectToScreen(graphics, pos - half_size, size);
        const SDL_FPoint center{dst.w / 2.0F, dst.h / 2.0F};
        SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(alpha * 255.0F));
        const SDL_FRect src{
            static_cast<float>(frame_data->sample_rect.x),
            static_cast<float>(frame_data->sample_rect.y),
            static_cast<float>(frame_data->sample_rect.w),
            static_cast<float>(frame_data->sample_rect.h),
        };
        SDL_RenderTextureRotated(renderer, texture, &src, &dst, rotation, &center, SDL_FLIP_NONE);
        SDL_SetTextureAlphaMod(texture, 255);
    }
}

} // namespace splonks
