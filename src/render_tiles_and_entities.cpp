#include "render_tiles_and_entities.hpp"

#include "entity.hpp"
#include "entities/common.hpp"
#include "graphics.hpp"
#include "render_terrain_lighting.hpp"
#include "special_effects/special_effect.hpp"
#include "state.hpp"
#include "terrain_lighting.hpp"
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

} // namespace

void RenderStageTiles(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    EnsureTerrainLightingCache(state, graphics);
    const TileSet tile_set = TileSetForStageType(state.stage.stage_type);
    for (std::size_t y = 0; y < state.stage.tiles.size(); ++y) {
        for (std::size_t x = 0; x < state.stage.tiles[y].size(); ++x) {
            const Tile tile = state.stage.tiles[y][x];
            const IVec2 tile_pos = IVec2::New(
                static_cast<int>(x * kTileSize),
                static_cast<int>(y * kTileSize)
            );
            const TileSourceData* const tile_source_data =
                GetTileSourceData(graphics, tile_set, tile, tile_pos);
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

            const bool is_air_tile = tile == Tile::Air;
            if (IsTileTransparent(tile)) {
                const TileSourceData* const air_source_data =
                    GetTileSourceData(graphics, tile_set, Tile::Air, tile_pos);
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

void RenderStageTileWrapper(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    EnsureTerrainLightingCache(state, graphics);
    const TileSet tile_set = TileSetForStageType(state.stage.stage_type);
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
                const IVec2 tile_pos = IVec2::New(
                    tile_x * static_cast<int>(kTileSize),
                    tile_y * static_cast<int>(kTileSize)
                );
                const TileSourceData* const tile_source_data =
                    GetTileSourceData(graphics, tile_set, Tile::Dirt, tile_pos);
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

            if (entity.frame_data_animator.HasAnimation()) {
                const FrameDataAnimation* const animation =
                    graphics.frame_data_db.FindAnimation(entity.frame_data_animator.animation_id);
                if (animation == nullptr || animation->frame_indices.empty()) {
                    continue;
                }

                std::size_t frame_index = entity.frame_data_animator.current_frame;
                if (frame_index >= animation->frame_indices.size()) {
                    frame_index = 0;
                }
                const FrameData& frame_data =
                    graphics.frame_data_db.frames[animation->frame_indices[frame_index]];
                SDL_Texture* const sprite_texture =
                    graphics.GetFrameDataTexture(frame_data.image_id);
                if (sprite_texture == nullptr) {
                    continue;
                }

                const Vec2 sprite_world_size = Vec2::New(
                    static_cast<float>(frame_data.sample_rect.w),
                    static_cast<float>(frame_data.sample_rect.h)
                );
                const Vec2 sprite_scaled_size =
                    sprite_world_size * entity.frame_data_animator.scale;
                const Vec2 render_position =
                    entities::common::GetSpriteTopLeftForEntity(entity, frame_data);

                const SDL_FRect src{
                    static_cast<float>(frame_data.sample_rect.x),
                    static_cast<float>(frame_data.sample_rect.y),
                    static_cast<float>(frame_data.sample_rect.w),
                    static_cast<float>(frame_data.sample_rect.h),
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
                continue;
            }

        }
    }
}

void RenderSpecialEffects(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    if (graphics.special_effects_texture == nullptr) {
        return;
    }
    for (const auto& special_effect : state.special_effects) {
        const Vec2 pos = special_effect->GetPos();
        const Vec2 size = special_effect->GetSize();
        const float rotation = special_effect->GetRot();
        const SampleRegion& sample_region = special_effect->GetSampleRegion();
        const float alpha = special_effect->GetAlpha();
        const Vec2 half_size = size / 2.0F;
        SDL_SetTextureAlphaMod(
            graphics.special_effects_texture,
            static_cast<Uint8>(alpha * 255.0F)
        );
        const SDL_FRect src{
            static_cast<float>(sample_region.pos.x),
                static_cast<float>(sample_region.pos.y),
                static_cast<float>(sample_region.size.x),
                static_cast<float>(sample_region.size.y),
        };
        const SDL_FRect dst = WorldRectToScreen(graphics, pos - half_size, size);
        const SDL_FPoint center{dst.w / 2.0F, dst.h / 2.0F};
        SDL_RenderTextureRotated(
            renderer,
            graphics.special_effects_texture,
            &src,
            &dst,
            rotation,
            &center,
            SDL_FLIP_NONE
        );
    }
}

} // namespace splonks
